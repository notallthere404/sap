#include "app_error.h"
#include "app_state.h"
#include "ble.h"
#include "button.h"
#include "diode.h"
#include "probe.h"

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

void app_handle_state(app_state_t state);
void app_handle_error(void);

static app_state_t app_state = APP_STATE_INVALID;
static app_error_t app_error = APP_ERROR_NONE;
/*
  TODO: Retry counter for error handling
static int error_retry = 0;
*/
app_state_t app_get_state(void) { return app_state; }
app_error_t app_get_error(void) { return app_error; }

/*
  TODO: Build standby loop
*/

void event_standby(void) {
  int err;

  err = probes_suspend();
  if (err != 0) {
    app_set_error(APP_ERROR_FATAL);
  }
}

void event_wake(void) {
  int err;

  err = diodes_init();
  if (err != 0) {
    app_set_error(APP_ERROR_FATAL);
  }

  err = buttons_init();
  if (err != 0) {
    app_set_error(APP_ERROR_FATAL);
  }

  err = probes_init();
  if (err != 0) {
    app_set_error(APP_ERROR_SENSOR_INIT);
  }

  err = ble_init();
  if (err != 0) {
    app_set_error(APP_ERROR_BLE_INIT);
  }

  app_set_state(APP_STATE_BACKGROUND);
}

void event_background(void) {
  int err;

  while (app_state == APP_STATE_BACKGROUND) {
    reading_t r;

    err = read_air_probe(&r);
    if (err != 0) {
      LOG_WRN("BME280 read failed: %d", err);
    }

    err = read_light_probe(&r);
    if (err != 0) {
      LOG_WRN("VEML7700 read failed: %d", err);
    }

    k_sleep(K_MSEC(800));
  }
}

void event_paired(void) {
  int err;

  while (app_state == APP_STATE_PAIRED) {
    char msg[96];
    reading_t r;

    err = read_air_probe(&r);
    if (err != 0) {
      LOG_WRN("BME280 read failed: %d", err);
    }

    err = read_light_probe(&r);
    if (err != 0) {
      LOG_WRN("VEML7700 read failed: %d", err);
    }

    /*
      Budget JSON serializer
    */
    snprintf(msg, sizeof(msg), "{\"t\":%.1f,\"p\":%.1f,\"h\":%.1f,\"l\":%u}",
             (double)r.temp, (double)r.pres, (double)r.humi, r.lux);

    ble_set_reading(msg);

    k_sleep(K_MSEC(800));
  }
}

/*
  Updates app state based on external triggers (e.g. button press)
*/
void app_set_state(app_state_t state) {
  if (state == app_state) {
    return;
  }

  app_state = state;

  diode_set_state(state);
  app_handle_state(state);
}

void app_handle_state(app_state_t state) {
  switch (state) {
  case APP_STATE_ERROR:
    app_handle_error();
    break;

  case APP_STATE_STANDBY:
    break;

  case APP_STATE_WAKING:
    event_wake();
    break;

  case APP_STATE_BACKGROUND:
    ble_adv_stop();
    event_background();
    break;

  case APP_STATE_PAIRING:
    ble_adv_start();
    break;

  case APP_STATE_PAIRED:
    ble_adv_stop();
    event_paired();
    break;

  default:
    break;
  }
}

void app_set_error(app_error_t err) {
  app_error = err;

  if (err != APP_ERROR_NONE) {
    app_set_state(APP_STATE_ERROR);
  }
}

/*
    TODO: Error handling scaffold
    Write after bt gatt
*/
void app_handle_error(void) {
  switch (app_error) {
  case APP_ERROR_SENSOR_INIT:
    break;

  case APP_ERROR_BLE_INIT:
    break;

  default:
    break;
  }

  app_error = APP_ERROR_NONE;
  app_set_state(APP_STATE_BACKGROUND);
}

int main(void) {
  // TODO: Replace with set to standby after PM mode has been fleshed out
  app_set_state(APP_STATE_WAKING);

  return 0;
}
