#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "probe.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void) {
  if (sensors_init() != 0) {
    return 1;
  }

  int rc = 0;

  while (1) {
    reading r = {0};
    if (read_air_probe(&r) != 0 || read_light_probe(&r) != 0) {
      LOG_WRN("read failed, skipping cycle");
    } else {
      LOG_INF("T %.2f  P %.2f  H %.2f  L %u", (double)r.temp, (double)r.pres,
              (double)r.humi, r.lux);
    }
    k_sleep(K_MSEC(2000));
  }

  return 0;
}
