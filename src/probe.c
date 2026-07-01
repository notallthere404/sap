#include "probe.h"

#include <errno.h>
#include <math.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensors, LOG_LEVEL_INF);

#define AIR_NODE DT_NODELABEL(bme280)
#define LIGHT_NODE DT_NODELABEL(veml7700)

#define AIR_BUFFER 128

const struct device *const dev_air = DEVICE_DT_GET(AIR_NODE);
const struct device *const dev_light = DEVICE_DT_GET(LIGHT_NODE);

// Defines sensor read instance
SENSOR_DT_READ_IODEV(iodev_air, AIR_NODE, {SENSOR_CHAN_AMBIENT_TEMP, 0},
                     {SENSOR_CHAN_PRESS, 0}, {SENSOR_CHAN_HUMIDITY, 0});

RTIO_DEFINE(ctx, 1, 1);

static bool device_check(const struct device *dev) {
  if (!device_is_ready(dev)) {
    LOG_ERR("Device %s not ready", dev->name);
    return false;
  }
  LOG_INF("Found device %s", dev->name);
  return true;
}

int sensors_init(void) {
  if (!device_check(dev_air) || !device_check(dev_light)) {
    return -ENODEV;
  }
  return 0;
}

static float decode_channel(const struct sensor_decoder_api *decoder,
                            const uint8_t *buf, enum sensor_channel ch) {
  uint32_t fit = 0;
  struct sensor_q31_data data = {0};

  int n =
      decoder->decode(buf, (struct sensor_chan_spec){ch, 0}, &fit, 1, &data);
  if (n < 1) {
    LOG_WRN("no frame for chan %d", ch);
    return 0.0f;
  }
  return ldexpf((float)data.readings[0].value, data.shift - 31);
}

int read_air_probe(reading *out) {
  uint8_t buf[AIR_BUFFER];

  int rc = sensor_read(&iodev_air, &ctx, buf, AIR_BUFFER);
  if (rc != 0) {
    LOG_ERR("Device: %s read failed", dev_air->name);
    return rc;
  }

  const struct sensor_decoder_api *decoder;
  rc = sensor_get_decoder(dev_air, &decoder);
  if (rc != 0) {
    LOG_ERR("Device: %s get decoder failed", dev_air->name);
    return rc;
  }

  out->temp = decode_channel(decoder, buf, SENSOR_CHAN_AMBIENT_TEMP);
  out->pres = decode_channel(decoder, buf, SENSOR_CHAN_PRESS);
  out->humi = decode_channel(decoder, buf, SENSOR_CHAN_HUMIDITY);
  return 0;
}

int read_light_probe(reading *out) {
  struct sensor_value data;

  int rc = sensor_sample_fetch_chan(dev_light, SENSOR_CHAN_LIGHT);
  if (rc != 0) {
    LOG_ERR("Device: %s fetch failed", dev_light->name);
    return rc;
  }
  rc = sensor_channel_get(dev_light, SENSOR_CHAN_LIGHT, &data);
  if (rc != 0) {
    LOG_ERR("Device: %s get channel failed. Errno: %d", dev_light->name, rc);
    return rc;
  }
  out->lux = (uint32_t)data.val1;
  return 0;
}
