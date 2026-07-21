#include "veml7700.h"
#include <math.h>
#include <zephyr/drivers/sensor_data_types.h>

/* Report how many frames of a channel are in the buffer */
static int veml7700_decoder_get_frame_count(const uint8_t *buf,
                                            struct sensor_chan_spec chan_spec,
                                            uint16_t *frame_count) {
  const struct veml7700_encoded_data *edata =
      (const struct veml7700_encoded_data *)buf;
  int err = -ENOTSUP;

  if (chan_spec.chan_idx != 0) {
    return err;
  }

  if (chan_spec.chan_type == SENSOR_CHAN_LIGHT) {
    *frame_count = edata->has_lux ? 1 : 0;
  } else {
    return err;
  }

  if (*frame_count > 0) {
    err = 0;
  }

  return err;
}

/* Report the decoded output size for a channel */
static int veml7700_decoder_get_size_info(struct sensor_chan_spec chan_spec,
                                          size_t *base_size,
                                          size_t *frame_size) {
  if (chan_spec.chan_type == SENSOR_CHAN_LIGHT) {
    *base_size = sizeof(struct sensor_q31_sample_data);
    *frame_size = sizeof(struct sensor_q31_sample_data);
    return 0;
  }

  return -ENOTSUP;
}

/*
 * q31 output scaling. sensor_q31 value is a signed fixed-point number
 * where the real value is value * 2^shift / 2^31. Shift 18 gives a range up
 * to 2^18 = 262144 lux, which covers the ~140 klx maximum, and leaves 13
 * fractional bits (about 0.0001 lux resolution).
 */
#define VEML7700_LUX_SHIFT 18

/*
 * Convert the encoded sample to a lux q31 frame
 * Counts become lux with the gain and it captured in the buffer
 */
static int veml7700_decoder_decode(const uint8_t *buf,
                                   struct sensor_chan_spec chan_spec,
                                   uint32_t *fit, uint16_t max_count,
                                   void *data_out) {
  const struct veml7700_encoded_data *edata =
      (const struct veml7700_encoded_data *)buf;

  if (*fit != 0 || max_count == 0) {
    return 0;
  }

  if (chan_spec.chan_type != SENSOR_CHAN_LIGHT) {
    return -EINVAL;
  }

  if (!edata->has_lux) {
    return -ENODATA;
  }

  struct sensor_q31_data *out = data_out;

  out->header.base_timestamp_ns = edata->header.timestamp;
  out->header.reading_count = 1;

  double lux = edata->reading.als_counts *
               veml7700_resolution[edata->reading.gain][edata->reading.it];

  out->shift = VEML7700_LUX_SHIFT;
  out->readings[0].timestamp_delta = 0;
  out->readings[0].value =
      (q31_t)(lux * (double)((int64_t)1 << (31 - VEML7700_LUX_SHIFT)));

  *fit = 1;

  return 1;
}

SENSOR_DECODER_API_DT_DEFINE() = {
    .get_frame_count = veml7700_decoder_get_frame_count,
    .get_size_info = veml7700_decoder_get_size_info,
    .decode = veml7700_decoder_decode,
};

int veml7700_get_decoder(const struct device *dev,
                         const struct sensor_decoder_api **decoder) {
  ARG_UNUSED(dev);
  *decoder = &SENSOR_DECODER_NAME();

  return 0;
}