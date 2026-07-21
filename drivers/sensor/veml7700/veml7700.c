#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>

#include "veml7700.h"

LOG_MODULE_REGISTER(veml7700, CONFIG_SENSOR_LOG_LEVEL);

struct veml7700_config {
  struct i2c_dt_spec bus;
  uint8_t psm; /* psm-mode property*/
};

static bool is_veml7700_gain_valid(uint32_t gain) {
  return (gain < VEML7700_ALS_GAIN_COUNT);
}

static bool is_veml7700_it_valid(uint32_t it) {
  return (it < VEML7700_ALS_IT_COUNT);
}

/* Single-shot mode wait for measurement completion */
static void
veml7700_sleep_by_integration_time(const struct veml7700_data *data) {
  /*
   * The integration time carries a +/-30% tolerance (datasheet page 4). 50% was
   * selected as precaution
   *
   * NOTE: In-tree driver suggests 100% increase from empirical data. Testing is
   * needed
   * .
   */
  switch (data->it) {
  case VEML7700_ALS_IT_25:
    k_msleep(38);
    break;
  case VEML7700_ALS_IT_50:
    k_msleep(75);
    break;
  case VEML7700_ALS_IT_100:
    k_msleep(150);
    break;
  case VEML7700_ALS_IT_200:
    k_msleep(300);
    break;
  case VEML7700_ALS_IT_400:
    k_msleep(600);
    break;
  case VEML7700_ALS_IT_800:
    k_msleep(1200);
    break;
  }
}

/* Reject values outside the valid enums */
static int veml7700_check_gain(const struct sensor_value *val) {
  return val->val1 >= VEML7700_ALS_GAIN_1 && val->val1 <= VEML7700_ALS_GAIN_1_4;
}

static int veml7700_check_it(const struct sensor_value *val) {
  return val->val1 >= VEML7700_ALS_IT_25 && val->val1 <= VEML7700_ALS_IT_800;
}

static int veml7700_check_pers(const struct sensor_value *val) {
  return (val->val1 >= VEML7700_ALS_PERS_1 &&
          val->val1 <= VEML7700_ALS_PERS_8) ||
         val->val1 == VEML7700_ALS_INT_DISABLED;
}

/*
 * Assemble 16-bit ALS_CONF
 * Layout:
 * 12:11  - ALS_GAIN
 * 9:6    - ALS_IT
 * 5:4    - ALS_PERS,
 * 1      - ALS_INT_EN
 * 0      - ALS_SD
 *
 * Rest reserved
 */
static int veml7700_build_als_conf_param(const struct veml7700_data *data,
                                         uint16_t *val) {
  if (!is_veml7700_gain_valid(data->gain) || !is_veml7700_it_valid(data->it)) {
    return -EINVAL;
  }
  uint16_t param = 0;

  /* raw field encoding */
  param |= data->gain << 11;

  /* convert from index to reg bit pattern */
  param |= veml7700_als_it_val[data->it] << 6;

  if (data->pers != VEML7700_ALS_INT_DISABLED) {
    param |= data->pers << 4; /* ALS_PERS */
    param |= BIT(1);          /* ALS_INT_EN */
  }

  if (data->shut_down) {
    param |= BIT(0); /* ALS_SD, 1 = shut down */
  }

  *val = param;

  return 0;
}

static inline int veml7700_bus_check(const struct device *dev) {
  const struct veml7700_config *config = dev->config;

  return i2c_is_ready_dt(&config->bus) ? 0 : -ENODEV;
}

/*
 * Read 16-bit register(LE)
 * Takes command code as arg and reads two bytes of data
 */
static inline int veml7700_reg_read(const struct device *dev, uint8_t cmd,
                                    uint16_t *val) {
  const struct veml7700_config *config = dev->config;
  uint8_t buf[2];
  int err;

  err = i2c_write_read_dt(&config->bus, &cmd, 1, buf, sizeof(buf));
  if (err != 0) {
    return err;
  }

  *val = sys_get_le16(buf);

  return 0;
}

/*
 * Write 16-bit register(LE)
 * Takes command code and writes value
 */
static inline int veml7700_reg_write(const struct device *dev, uint8_t cmd,
                                     uint16_t val) {
  const struct veml7700_config *config = dev->config;
  uint8_t buf[3];

  buf[0] = cmd;
  sys_put_le16(val, &buf[1]);

  return i2c_write_dt(&config->bus, buf, sizeof(buf));
}

static int veml7700_write_als_config(const struct device *dev) {
  const struct veml7700_data *data = dev->data;
  uint16_t param;
  int err;

  err = veml7700_build_als_conf_param(data, &param);
  if (err != 0) {
    return err;
  }

  return veml7700_reg_write(dev, VEML7700_ALS_CONF_0, param);
}

static uint16_t veml7700_write_psm_param(const struct veml7700_config *config) {
  return config->psm;
}

/*
 * Converts lux threshold from application into raw counts the threshold
 * registers expect
 */
static int veml7700_lux_to_counts(const struct veml7700_data *data,
                                  uint32_t lux, uint16_t *counts) {
  if (!is_veml7700_gain_valid(data->gain) || !is_veml7700_it_valid(data->it)) {
    return -EINVAL;
  }

  *counts = lux / veml7700_resolution[data->gain][data->it];

  return 0;
}

static int veml7700_write_thresh_high(const struct device *dev) {
  const struct veml7700_data *data = dev->data;

  return veml7700_reg_write(dev, VEML7700_ALS_WH, data->thresh_high);
}

static int veml7700_write_thresh_low(const struct device *dev) {
  const struct veml7700_data *data = dev->data;

  return veml7700_reg_write(dev, VEML7700_ALS_WL, data->thresh_low);
}

static int veml7700_write_psm(const struct device *dev) {
  uint16_t param = veml7700_write_psm_param(dev->config);

  return veml7700_reg_write(dev, VEML7700_ALS_PSM, param);
}

#ifdef CONFIG_PM_DEVICE
/*
 * Set the ALS_SD bit and write it out. The shut_down flag lives inside the
 * ALS_CONF register, so this rewrites the whole register. Restore the old
 * flag if the write fails so state stays in sync with the hardware.
 */
static int veml7700_set_shutdown_flag(const struct device *dev,
                                      uint8_t new_val) {
  struct veml7700_data *data = dev->data;
  uint8_t prev_sd;
  int ret;

  prev_sd = data->shut_down;
  data->shut_down = new_val;

  ret = veml7700_write_als_config(dev);
  if (ret < 0) {
    data->shut_down = prev_sd;
  }
  return ret;
}
#endif

/* Read the interrupt flags
 *
 * Note: reading register 06h clears the flags.
 */
static int veml7700_fetch_int(const struct device *dev) {
  struct veml7700_data *data = dev->data;
  uint16_t val = 0;

  int err = veml7700_reg_read(dev, VEML7700_ALS_INT, &val);
  if (err != 0) {
    return err;
  }

  data->int_flags = val & VEML7700_ALS_INT_HIGH_MASK;

  return 0;
}

/*
 * Reading of both channels into `reading`
 *
 * PSM disabled mode: Sensor shutdown between fetches which requires waking,
 * waiting for measurement, reading and shutdown
 *
 * PSM continuous mode: wake/wait/shutdown skipped and latest sample is read
 * directly
 *
 * Snapshot of gain and it for later lux conversion in case of attr changes
 */
int veml7700_sample_fetch_helper(const struct device *dev,
                                 enum sensor_channel chan,
                                 struct veml7700_reading *reading) {
  struct veml7700_data *data = dev->data;
  bool is_shut_down = false;
  int err;

  if (data->shut_down) {
    data->shut_down = 0;
    err = veml7700_write_als_config(dev);
    if (err) {
      return err;
    }

    is_shut_down = true;

    /*
     * Wait >=2.5 ms after clearing ALS_SD for oscillator and signal processor
     * to start
     */
    k_msleep(5);
    veml7700_sleep_by_integration_time(data);
  }

  err = veml7700_reg_read(dev, VEML7700_ALS, &reading->als_counts);
  if (err) {
    return err;
  }

  err = veml7700_reg_read(dev, VEML7700_ALS_WHITE, &reading->white_counts);
  if (err) {
    return err;
  }

  reading->gain = data->gain;
  reading->it = data->it;

  /* shutdown state if we woke the sensor. */
  if (is_shut_down) {
    data->shut_down = 1;
    err = veml7700_write_als_config(dev);
    if (err) {
      return err;
    }
  }

  return 0;
}

static int veml7700_sample_fetch(const struct device *dev,
                                 enum sensor_channel chan) {
  struct veml7700_data *data = dev->data;

  if ((enum sensor_channel_veml7700)chan == SENSOR_CHAN_VEML7700_INTERRUPT) {
    /* reading ALS_INT clears the interrupt flags */
    return veml7700_fetch_int(dev);
  }

  return veml7700_sample_fetch_helper(dev, chan, &data->reading);
}

static int veml7700_channel_get(const struct device *dev,
                                enum sensor_channel chan,
                                struct sensor_value *val) {
  const struct veml7700_data *data = dev->data;
  const struct veml7700_reading *reading = &data->reading;

  /* Lux from counts, using the gain and it captured with the sample. */
  if (chan == SENSOR_CHAN_LIGHT) {
    double lux =
        reading->als_counts * veml7700_resolution[reading->gain][reading->it];
    return sensor_value_from_double(val, lux);
  }

  if ((enum sensor_channel_veml7700)chan == SENSOR_CHAN_VEML7700_RAW_COUNTS) {
    val->val1 = reading->als_counts;
  } else if ((enum sensor_channel_veml7700)chan ==
             SENSOR_CHAN_VEML7700_WHITE_RAW_COUNTS) {
    val->val1 = reading->white_counts;
  } else if ((enum sensor_channel_veml7700)chan ==
             SENSOR_CHAN_VEML7700_INTERRUPT) {
    val->val1 = data->int_flags;
  } else {
    return -ENOTSUP;
  }

  val->val2 = 0;

  return 0;
}

/*
 * Thresholds are given in lux and converted to counts, gain, integration time,
 * and interrupt persistence
 */
static int veml7700_attr_set(const struct device *dev, enum sensor_channel chan,
                             enum sensor_attribute attr,
                             const struct sensor_value *val) {
  if (chan != SENSOR_CHAN_LIGHT) {
    return -ENOTSUP;
  }

  struct veml7700_data *data = dev->data;
  int ret = 0;

  if (attr == SENSOR_ATTR_LOWER_THRESH) {
    ret = veml7700_lux_to_counts(data, val->val1, &data->thresh_low);
    if (ret < 0) {
      return ret;
    }
    return veml7700_write_thresh_low(dev);
  } else if (attr == SENSOR_ATTR_UPPER_THRESH) {
    ret = veml7700_lux_to_counts(data, val->val1, &data->thresh_high);
    if (ret < 0) {
      return ret;
    }
    return veml7700_write_thresh_high(dev);
  } else if ((enum sensor_attribute_veml7700)attr ==
             SENSOR_ATTR_VEML7700_GAIN) {
    if (veml7700_check_gain(val)) {
      data->gain = (enum veml7700_als_gain)val->val1;
      return veml7700_write_als_config(dev);
    } else {
      return -EINVAL;
    }
  } else if ((enum sensor_attribute_veml7700)attr ==
             SENSOR_ATTR_VEML7700_ITIME) {
    if (veml7700_check_it(val)) {
      data->it = (enum veml7700_als_it)val->val1;
      return veml7700_write_als_config(dev);
    } else {
      return -EINVAL;
    }
  } else if ((enum sensor_attribute_veml7700)attr ==
             SENSOR_ATTR_VEML7700_INT_MODE) {
    if (veml7700_check_pers(val)) {
      data->pers = (enum veml7700_als_pers)val->val1;
      return veml7700_write_als_config(dev);
    } else {
      return -EINVAL;
    }
  } else {
    return -ENOTSUP;
  }
}

static int veml7700_attr_get(const struct device *dev, enum sensor_channel chan,
                             enum sensor_attribute attr,
                             struct sensor_value *val) {
  if (chan != SENSOR_CHAN_LIGHT) {
    return -ENOTSUP;
  }

  struct veml7700_data *data = dev->data;

  if (attr == SENSOR_ATTR_LOWER_THRESH) {
    val->val1 = data->thresh_low;
  } else if (attr == SENSOR_ATTR_UPPER_THRESH) {
    val->val1 = data->thresh_high;
  } else if ((enum sensor_attribute_veml7700)attr ==
             SENSOR_ATTR_VEML7700_GAIN) {
    val->val1 = data->gain;
  } else if ((enum sensor_attribute_veml7700)attr ==
             SENSOR_ATTR_VEML7700_ITIME) {
    val->val1 = data->it;
  } else if ((enum sensor_attribute_veml7700)attr ==
             SENSOR_ATTR_VEML7700_INT_MODE) {
    val->val1 = data->pers;
  } else {
    return -ENOTSUP;
  }

  val->val2 = 0;

  return 0;
}

static DEVICE_API(sensor, veml7700_api_funcs) = {
    .sample_fetch = veml7700_sample_fetch,
    .channel_get = veml7700_channel_get,
    .attr_set = veml7700_attr_set,
    .attr_get = veml7700_attr_get,
#ifdef CONFIG_SENSOR_ASYNC_API
    .submit = veml7700_submit,
    .get_decoder = veml7700_get_decoder,
#endif
};

static int veml7700_init(const struct device *dev) {
  const struct veml7700_config *config = dev->config;
  struct veml7700_data *data = dev->data;
  int err;

  err = veml7700_bus_check(dev);
  if (err != 0) {
    LOG_DBG("bus check failed %d", err);
    return err;
  }

  /* Recommended defaults */
  data->gain = VEML7700_ALS_GAIN_1_4;
  data->it = VEML7700_ALS_IT_100;
  data->pers = VEML7700_ALS_INT_DISABLED;
  data->thresh_low = 0;
  data->thresh_high = 0xFFFF;
  data->shut_down = (config->psm == VEML7700_PSM_DISABLED) ? 1 : 0;

  err = veml7700_write_als_config(dev);
  if (err != 0) {
    return err;
  }

  return veml7700_write_psm(dev);
}

#ifdef CONFIG_PM_DEVICE
static int veml7700_pm_action(const struct device *dev,
                              enum pm_device_action action) {
  const struct veml7700_config *config = dev->config;

  switch (action) {
  case PM_DEVICE_ACTION_SUSPEND:
    /* force the sensor into shutdown for lowest power draw */
    return veml7700_set_shutdown_flag(dev, 1);
  case PM_DEVICE_ACTION_RESUME:
    /* restore the resting state: single-shot mode rests in shutdown
     * between fetches, PSM continuous mode runs powered on. Registers
     * survive shutdown (VDD stays on) so re-writing ALS_CONF is enough. */
    return veml7700_set_shutdown_flag(
        dev, (config->psm == VEML7700_PSM_DISABLED) ? 1 : 0);
  default:
    return -ENOTSUP;
  }
}
#endif

/* Instantiate one driver instance per enabled custom,veml7700 dt node */
/* clang-format off */
#define VEML7700_DEFINE(inst)                                     \
  static struct veml7700_data veml7700_data_##inst;               \
  static const struct veml7700_config veml7700_config_##inst = {  \
      .bus = I2C_DT_SPEC_INST_GET(inst),                          \
      .psm = DT_INST_PROP(inst, psm_mode)                         \
  };                                                              \
                                                                  \
  PM_DEVICE_DT_INST_DEFINE(inst, veml7700_pm_action);             \
                                                                  \
  SENSOR_DEVICE_DT_INST_DEFINE(inst,                              \
                veml7700_init,                                    \
                PM_DEVICE_DT_INST_GET(inst),                      \
                &veml7700_data_##inst,                            \
                &veml7700_config_##inst,				                  \
			          POST_KERNEL,					                            \
			          CONFIG_SENSOR_INIT_PRIORITY,			                \
			          &veml7700_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(VEML7700_DEFINE)