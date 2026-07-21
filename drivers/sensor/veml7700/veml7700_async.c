#include <zephyr/drivers/sensor_clock.h>
#include <zephyr/logging/log.h>
#include <zephyr/rtio/work.h>

#include "veml7700.h"

LOG_MODULE_DECLARE(veml7700, CONFIG_SENSOR_LOG_LEVEL);

/* Blocking half of submit. Runs on the RTIO work queue */
void veml7700_submit_sync(struct rtio_iodev_sqe *iodev_sqe) {
  uint32_t min_buf_len = sizeof(struct veml7700_encoded_data);
  uint64_t cycles;
  uint8_t *buf;
  uint32_t buf_len;
  int err;

  const struct sensor_read_config *config = iodev_sqe->sqe.iodev->data;
  const struct device *dev = config->sensor;
  const struct sensor_chan_spec *const channels = config->channels;
  const size_t num_channels = config->count;

  /* Buffer large enough for the encoded sample */
  err = rtio_sqe_rx_buf(iodev_sqe, min_buf_len, min_buf_len, &buf, &buf_len);
  if (err != 0) {
    LOG_ERR("Failed to get a read buffer of size %u bytes", min_buf_len);
    rtio_iodev_sqe_err(iodev_sqe, err);
    return;
  }

  err = sensor_clock_get_cycles(&cycles);
  if (err != 0) {
    LOG_ERR("Failed to get sensor clock cycles");
    rtio_iodev_sqe_err(iodev_sqe, err);
    return;
  }

  struct veml7700_encoded_data *edata;

  edata = (struct veml7700_encoded_data *)buf;
  edata->header.timestamp = sensor_clock_cycles_to_ns(cycles);
  edata->has_lux = 0;

  for (size_t i = 0; i < num_channels; i++) {
    switch (channels[i].chan_type) {
    /* Informs decoder to emit lux */
    case SENSOR_CHAN_LIGHT:
      edata->has_lux = 1;
      break;
    default:
      continue;
    }
  }

  err = veml7700_sample_fetch_helper(dev, SENSOR_CHAN_ALL, &edata->reading);
  if (err != 0) {
    LOG_ERR("Failed to fetch sample");
    rtio_iodev_sqe_err(iodev_sqe, err);
    return;
  }

  rtio_iodev_sqe_ok(iodev_sqe, 0);
}

/*
 * Requirement: CONFIG_RTIO_WORKQ.
 */
void veml7700_submit(const struct device *dev,
                     struct rtio_iodev_sqe *iodev_sqe) {
  struct rtio_work_req *req = rtio_work_req_alloc();

  if (req == NULL) {
    LOG_ERR("RTIO work item allocation failed. Consider to increase "
            "CONFIG_RTIO_WORKQ_POOL_ITEMS.");
    rtio_iodev_sqe_err(iodev_sqe, -ENOMEM);
    return;
  }

  rtio_work_req_submit(req, iodev_sqe, veml7700_submit_sync);
}