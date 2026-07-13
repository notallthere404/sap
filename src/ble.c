#include "ble.h"
#include "app_error.h"
#include "app_state.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bluetooth, LOG_LEVEL_INF);

/*
  UUIDs:
  Service: 27b396da-a5b5-4f97-995e-a798427620b0
  Reading: 27b396da-a5b5-4f97-995e-a798427620b1
*/
#define BT_UUID_SERVICE_VAL                                                    \
  0xb0, 0x20, 0x76, 0x42, 0x98, 0xa7, 0x5e, 0x99, 0x97, 0x4f, 0xb5, 0xa5,      \
      0xda, 0x96, 0xb3, 0x27

#define BT_UUID_READING_VAL                                                    \
  0xb1, 0x20, 0x76, 0x42, 0x98, 0xa7, 0x5e, 0x99, 0x97, 0x4f, 0xb5, 0xa5,      \
      0xda, 0x96, 0xb3, 0x27

#define DEVICE_UUID_SERVICE BT_UUID_DECLARE_128(BT_UUID_SERVICE_VAL)
#define DEVICE_UUID_READING BT_UUID_DECLARE_128(BT_UUID_READING_VAL)

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/*
  Appearance:
  Multi-sensor: 0x0556
*/
#define DEVICE_APPEARANCE BT_APPEARANCE_MULTISENSOR

/* Advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),

    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, (DEVICE_APPEARANCE >> 0) & 0xff,
                  (DEVICE_APPEARANCE >> 8) & 0xff),
};

/* Scan data */
static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SERVICE_VAL),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Active connection reference and notification subscription state */
static struct bt_conn *ble_conn;
static bool can_notify;

/* Latest reading exposed over GATT, guarded by mutex */
static char current[96] = "No reading";
K_MUTEX_DEFINE(current_lock);

/*
    Enables the Bluetooth stack during startup
*/
int ble_init(void) {
  int err = bt_enable(NULL);
  if (err != 0) {
    app_set_error(APP_ERROR_BLE_INIT);
    LOG_ERR("Bluetooth init failed: %d", err);
    return err;
  }

  LOG_INF("Bluetooth init success");

  return 0;
}

/*
    Creates a fixed random static identity address for the device
*/
int generate_addr(void) {
  bt_addr_le_t addr;
  int err;

  err = bt_addr_le_from_str("FF:EE:DD:CC:BB:AA", "random", &addr);
  if (err != 0) {
    LOG_ERR("Invalid address: %d", err);
    return 1;
  }

  err = bt_id_create(&addr, NULL);
  if (err < 0) {
    LOG_ERR("Invalid id: %d", err);
    return 1;
  }

  return 0;
}

/*
    Starts connectable advertising from the system workqueue
*/
static void adv_work_handler(struct k_work *work) {
  ARG_UNUSED(work);

  int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd,
                            ARRAY_SIZE(sd));

  if (err == -EALREADY) {
    LOG_INF("Bluetooth advertising already active");
    return;
  }

  if (err != 0) {
    app_set_error(APP_ERROR_BLE_ADVERTISING);
    LOG_ERR("Bluetooth advertising failed: %d", err);
    return;
  }

  LOG_INF("Bluetooth advertising started");
}

static K_WORK_DELAYABLE_DEFINE(adv_work, adv_work_handler);

int ble_adv_start(void) { return k_work_schedule(&adv_work, K_NO_WAIT); }

/*
    Cancels pending advertising work and stops any active advertising
*/
int ble_adv_stop(void) {
  int err;

  k_work_cancel_delayable(&adv_work);

  err = bt_le_adv_stop();
  if (err != 0 && err != -EALREADY && err != -EINVAL) {
    LOG_ERR("Bluetooth advertising failed while stopping: %d", err);
    return err;
  }

  LOG_INF("Bluetooth advertising stopped");

  return 0;
}

/*
    Keeps a reference to the new connection and reports the result
*/
static void connected(struct bt_conn *conn, uint8_t hci_err) {
  if (hci_err != 0) {
    app_set_error(APP_ERROR_BLE_CONNECTION);
    LOG_ERR("Bluetooth connection failed: %s", bt_hci_err_to_str(hci_err));
    return;
  }

  ble_conn = bt_conn_ref(conn);

  app_set_state(APP_STATE_PAIRED);
  LOG_INF("Bluetooth connected");
}

/*
    Releases the connection reference and disables notifications
*/
static void disconnected(struct bt_conn *conn, uint8_t msg) {
  if (ble_conn != NULL) {
    bt_conn_unref(ble_conn);
    ble_conn = NULL;
  }

  can_notify = false;

  app_state_t state = app_get_state();
  if (state == APP_STATE_PAIRED || state == APP_STATE_PAIRING) {
    app_set_state(APP_STATE_BACKGROUND);
  }
  LOG_INF("Bluetooth disconnected");
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static ssize_t ble_reading(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, void *buf,
                           uint16_t len, uint16_t offset) {
  char copy[sizeof(current)];

  k_mutex_lock(&current_lock, K_FOREVER);
  snprintf(copy, sizeof(copy), "%s", current);
  k_mutex_unlock(&current_lock);

  return bt_gatt_attr_read(conn, attr, buf, len, offset, copy, strlen(copy));
}

/*
  Track client subscription to notifications
*/
static void ble_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
  can_notify = (value == BT_GATT_CCC_NOTIFY);
}

/*
  Define gatt service as:

*/
BT_GATT_SERVICE_DEFINE(
    ble_svc, BT_GATT_PRIMARY_SERVICE(DEVICE_UUID_SERVICE),

    BT_GATT_CHARACTERISTIC(DEVICE_UUID_READING,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, ble_reading, NULL, NULL),

    BT_GATT_CCC(ble_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

int ble_set_reading(const char *str) {
  k_mutex_lock(&current_lock, K_FOREVER);
  snprintf(current, sizeof(current), "%s", str);
  k_mutex_unlock(&current_lock);

  if (can_notify && ble_conn != NULL) {
    return bt_gatt_notify(ble_conn, &ble_svc.attrs[2], current,
                          strlen(current));
  }

  return 0;
}
