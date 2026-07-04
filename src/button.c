#include "button.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(buttons, LOG_LEVEL_INF);

#define LONGPRESS_NODE DT_PATH(longpress)

static const struct device *const btns = DEVICE_DT_GET(LONGPRESS_NODE);

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
  case INPUT_KEY_X:
    if (state == APP_STATE_STANDBY) {
      app_set_state(APP_STATE_BACKGROUND);
    } else {
      app_set_state(APP_STATE_STANDBY);
    }
    break;

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

int buttons_init(void) {
  if (!device_is_ready(btns)) {
    LOG_ERR("Button input device not ready");
    return -ENODEV;
  }

  LOG_INF("Button input ready");
  return 0;
}