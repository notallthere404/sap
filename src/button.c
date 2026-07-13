#include "button.h"
#include "app_state.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(buttons, LOG_LEVEL_INF);

#define LONGPRESS_NODE DT_PATH(longpress)

/* Pointer to button input device reference */
static const struct device *const btns = DEVICE_DT_GET(LONGPRESS_NODE);

/*
    Maps button presses to app state transitions
*/
static void input_cb(struct input_event *evt, void *user_data) {
  ARG_UNUSED(user_data);

  if (!evt->sync) {
    return;
  }

  /* Ignore non key events */
  if (evt->type != INPUT_EV_KEY) {
    return;
  }

  /* Ignore release values */
  if (evt->value == 0) {
    return;
  }

  app_state_t state = app_get_state();

  switch (evt->code) {
  /* Toggles between standby and active operation */
  case INPUT_KEY_X:
    if (state == APP_STATE_STANDBY) {
      app_set_state(APP_STATE_BACKGROUND);
    } else {
      app_set_state(APP_STATE_STANDBY);
    }
    break;

  /* Toggles pairing mode while active */
  case INPUT_KEY_Y:
    if (state == APP_STATE_BACKGROUND) {
      app_set_state(APP_STATE_PAIRING);
    } else if (state == APP_STATE_PAIRING) {
      app_set_state(APP_STATE_BACKGROUND);
    }
    break;

  default:
    break;
  }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(LONGPRESS_NODE), input_cb, NULL);

/*
    Ensure button input device is operational during startup
*/
int buttons_init(void) {
  if (!device_is_ready(btns)) {
    LOG_ERR("Button input device not ready");
    return -ENODEV;
  }

  LOG_INF("Button input ready");
  return 0;
}