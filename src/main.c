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

static K_EVENT_DEFINE(state_event);

void event_standby(void) {
  int err;

  err = ble_adv_stop();
  if (err != 0) {
    app_set_error(APP_ERROR_FATAL);
  }

  err = ble_suspend();
  if (err != 0) {
    app_set_error(APP_ERROR_FATAL);
  }

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

/*
    Runs one sampling pass while unpaired
*/
void event_background(void) {
  int err;
  reading_t r;

  err = read_air_probe(&r);
  if (err != 0) {
    LOG_WRN("BME280 read failed: %d", err);
  }

  err = read_light_probe(&r);
  if (err != 0) {
    LOG_WRN("VEML7700 read failed: %d", err);
  }
}

/*
    Runs one sampling pass and publishes the reading over BLE
*/
void event_paired(void) {
  int err;
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
}

/*
  Updates app state based on external triggers
*/
void app_set_state(app_state_t state) {
  if (state == app_state) {
    return;
  }

  app_state = state;

  diode_set_state(state);
  app_handle_state(state);

  k_event_post(&state_event, 1);
}

void app_handle_state(app_state_t state) {
  switch (state) {
  case APP_STATE_ERROR:
    app_handle_error();
    break;

  case APP_STATE_STANDBY:
    event_standby();
    break;

  case APP_STATE_WAKING:
    event_wake();
    break;

  case APP_STATE_BACKGROUND:
    ble_adv_stop();
    break;

  case APP_STATE_PAIRING:
    ble_adv_start();
    break;

  case APP_STATE_PAIRED:
    ble_adv_stop();
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
  app_set_state(APP_STATE_WAKING);

  while (1) {
    switch (app_get_state()) {
    case APP_STATE_BACKGROUND:
      event_background();
      k_event_wait(&state_event, 1, true, K_MSEC(200));
      break;

    case APP_STATE_PAIRED:
      event_paired();
      k_event_wait(&state_event, 1, true, K_MSEC(200));
      break;

    default:
      k_event_wait(&state_event, 1, true, K_FOREVER);
      break;
    }
  }

  return 0;
}
