#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_state.h"
#include "ble.h"
#include "button.h"
#include "diode.h"
#include "probe.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static app_state_t app_state = APP_STATE_STANDBY;

void app_set_state(app_state_t state) {
  if (state == app_state) {
    return;
  }

  app_state = state;
  LOG_INF("App state changed: %d", state);

  diode_set_state(state);
}

app_state_t app_get_state(void) { return app_state; }

int main(void) {
  if (buttons_init() != 0) {
    return 1;
  }

  if (diodes_init() != 0) {
    return 1;
  }

  if (probes_init() != 0) {
    return 1;
  }

  while (1) {
    reading_t r = {0};
    if (read_air_probe(&r) != 0 || read_light_probe(&r) != 0) {
      LOG_WRN("read failed, skipping cycle");
    } else {
      LOG_INF("Temp: %.2f C | Pressure: %.2f kPa | Humidity: %.2f %% | Light: "
              "%u lx",
              (double)r.temp, (double)r.pres, (double)r.humi, r.lux);
    }
    k_sleep(K_MSEC(5000));
  }

  return 0;
}
