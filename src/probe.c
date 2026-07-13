#include "probe.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensors, LOG_LEVEL_INF);

/* Air sensor buffer size value */
#define BUFFER_SIZE 128

#define AIR_NODE DT_NODELABEL(bme280)
#define LIGHT_NODE DT_NODELABEL(veml7700)

/*
  Instatiate air sensor read instance for air channels
*/
SENSOR_DT_READ_IODEV(iodev_air, AIR_NODE, {SENSOR_CHAN_AMBIENT_TEMP, 0},
                     {SENSOR_CHAN_PRESS, 0}, {SENSOR_CHAN_HUMIDITY, 0});

RTIO_DEFINE(ctx, 1, 1);

/* Pointer to device reference */
const struct device *const dev_air = DEVICE_DT_GET(AIR_NODE);
const struct device *const dev_light = DEVICE_DT_GET(LIGHT_NODE);

static bool device_check(const struct device *dev) {
  if (!device_is_ready(dev)) {
    LOG_ERR("Device %s not ready", dev->name);
    return false;
  }
  LOG_INF("Found device %s", dev->name);
  return true;
}

/*
    Ensure both sensors are operational during startup
*/
int probes_init(void) {
  if (!device_check(dev_air) || !device_check(dev_light)) {
    return -ENODEV;
  }
  return 0;
}

/*
    Decodes single channel from sensor buffer and converts value to float
*/
static int decode_channel(const struct sensor_decoder_api *decoder,
                          const uint8_t *buf, enum sensor_channel ch,
                          float *out) {
  /*
    * Frame iterator reset to first frame

    * Union of sensor data initialized

    * Channel specification defined (index 0 for single axis measurements)
  */
  uint32_t frame = 0;
  struct sensor_q31_data data = {0};
  struct sensor_chan_spec spec = {
      .chan_type = ch,
      .chan_idx = 0,
  };

  int n = decoder->decode(buf, spec, &frame, 1, &data);

  if (n < 0) {
    LOG_ERR("decode failed for channel %d: %d", ch, n);
    return n;
  }

  if (n == 0) {
    LOG_WRN("no frame for channel %d", ch);
    return -ENODATA;
  }

  /* Fixed to floating point conversion */
  *out = ldexpf((float)data.readings[0].value, data.shift - 31);

  return 0;
}

/*
    Reads and decodes a single sample from the bme280 sensor
*/
int read_air_probe(reading_t *out) {
  uint8_t buf[BUFFER_SIZE];

  /* Sensor read into buffer */
  int err = sensor_read(&iodev_air, &ctx, buf, BUFFER_SIZE);
  if (err != 0) {
    LOG_ERR("Device: %s read failed", dev_air->name);
    return err;
  }

  /* Fetching of decoder api */
  const struct sensor_decoder_api *decoder;
  err = sensor_get_decoder(dev_air, &decoder);
  if (err != 0) {
    LOG_ERR("Device: %s get decoder failed", dev_air->name);
    return err;
  }

  reading_t tmp = {0};
  /*
    Sensor channel read and decoded. Assigned to reading_t fields in temporary
    struct
  */
  err = decode_channel(decoder, buf, SENSOR_CHAN_AMBIENT_TEMP, &tmp.temp);
  if (err != 0) {
    LOG_ERR("Device %s temperature decode failed: %d", dev_air->name, err);
    return err;
  }

  err = decode_channel(decoder, buf, SENSOR_CHAN_PRESS, &tmp.pres);
  if (err != 0) {
    LOG_ERR("Device %s pressure decode failed: %d", dev_air->name, err);
    return err;
  }

  err = decode_channel(decoder, buf, SENSOR_CHAN_HUMIDITY, &tmp.humi);
  if (err != 0) {
    LOG_ERR("Device %s humidity decode failed: %d", dev_air->name, err);
    return err;
  }

  *out = tmp;

  return 0;
}

/*
    TODO: Add 'Read and Decode' support to veml7700 driver? (research)
*/
int read_light_probe(reading_t *out) {
  struct sensor_value data;

  int err = sensor_sample_fetch_chan(dev_light, SENSOR_CHAN_LIGHT);
  if (err != 0) {
    LOG_ERR("Device: %s fetch failed", dev_light->name);
    return err;
  }

  err = sensor_channel_get(dev_light, SENSOR_CHAN_LIGHT, &data);
  if (err != 0) {
    LOG_ERR("Device: %s get channel failed. Errno: %d", dev_light->name, err);
    return err;
  }

  out->lux = (uint32_t)data.val1;

  return 0;
}
