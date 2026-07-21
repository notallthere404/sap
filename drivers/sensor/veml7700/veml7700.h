#ifndef CUSTOM_DRIVERS_SENSOR_VEML7700_H_
#define CUSTOM_DRIVERS_SENSOR_VEML7700_H_

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/types.h>

#define DT_DRV_COMPAT custom_veml7700

#define VEML7700_BUS_I2C DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c)

/* Bit 15 is set when a reading crossed the low threshold (ALS_WL) */
#define VEML7700_ALS_INT_LOW_MASK BIT(15)

/* Bit 14 is set when a reading crossed the high threshold (ALS_WH) */
#define VEML7700_ALS_INT_HIGH_MASK BIT(14)

/* Command codes */
#define VEML7700_ALS_CONF_0 0x00
#define VEML7700_ALS_WH 0x01
#define VEML7700_ALS_WL 0x02
#define VEML7700_ALS_PSM 0x03
#define VEML7700_ALS 0x04
#define VEML7700_ALS_WHITE 0x05
#define VEML7700_ALS_INT 0x06

/* psm-mode devicetree value */
#define VEML7700_PSM_DISABLED 0x00

#define VEML7700_ALS_GAIN_COUNT 4

/*
 * ALS_GAIN encodings:
 * b00 = 1
 * b01 = 2
 * b10 = 1/8
 * b11 = 1/4
 */
static const uint8_t veml7700_als_gain_val[VEML7700_ALS_GAIN_COUNT] = {
    0x00,
    0x01,
    0x02,
    0x03,
};

enum veml7700_als_gain {
  VEML7700_ALS_GAIN_1 = 0x00,
  VEML7700_ALS_GAIN_2 = 0x01,
  VEML7700_ALS_GAIN_1_8 = 0x02,
  VEML7700_ALS_GAIN_1_4 = 0x03,
};

#define VEML7700_ALS_IT_COUNT 6

/*
 * ALS_IT(non-contiguous)
 * b1100 = 25ms
 * b1000 = 50ms
 * b0000 = 100ms
 * b0001 = 200ms
 * b0010 = 400ms
 * b0011 = 800ms
 *
 * Maps the dense enum index below to the register bit pattern
 * */
static const uint8_t veml7700_als_it_val[VEML7700_ALS_IT_COUNT] = {
    0x0C, 0x08, 0x00, 0x01, 0x02, 0x03,
};

/* Used to index veml7700_als_it_val (not the raw register value) */
enum veml7700_als_it {
  VEML7700_ALS_IT_25,
  VEML7700_ALS_IT_50,
  VEML7700_ALS_IT_100,
  VEML7700_ALS_IT_200,
  VEML7700_ALS_IT_400,
  VEML7700_ALS_IT_800,
};

/*
 * Lux per count for each gain and integration time (see datasheet p.5)
 */
static const float
    veml7700_resolution[VEML7700_ALS_GAIN_COUNT][VEML7700_ALS_IT_COUNT] = {
        /* 25ms     50ms     100ms    200ms    400ms    800ms */
        {0.2688f, 0.1344f, 0.0672f, 0.0336f, 0.0168f, 0.0084f}, /* x1 */
        {0.1344f, 0.0672f, 0.0336f, 0.0168f, 0.0084f, 0.0042f}, /* x2 */
        {2.1504f, 1.0752f, 0.5376f, 0.2688f, 0.1344f, 0.0672f}, /* x1/8 */
        {1.0752f, 0.5376f, 0.2688f, 0.1344f, 0.0672f, 0.0336f}, /* x1/4 */
};

/*
 * ALS_PERS field
 * Number of consecutive readings needed before interrupt flag set:
 * b00 = 1
 * b01 = 2
 * b10 = 4
 * b11 = 8
 *
 * VEML7700_ALS_INT_DISABLED means interrupts off
 */
enum veml7700_als_pers {
  VEML7700_ALS_INT_DISABLED = 0xFF,
  VEML7700_ALS_PERS_1 = 0x00,
  VEML7700_ALS_PERS_2 = 0x01,
  VEML7700_ALS_PERS_4 = 0x02,
  VEML7700_ALS_PERS_8 = 0x03,
};

/* Private attributes for runtime tuning via attr_set/attr_get */
enum sensor_attribute_veml7700 {
  SENSOR_ATTR_VEML7700_GAIN = SENSOR_ATTR_PRIV_START,
  SENSOR_ATTR_VEML7700_ITIME,
  SENSOR_ATTR_VEML7700_INT_MODE,
};

/*
 * Private channels exposing underlying values instead:
 * raw ALS counts
 * raw white counts
 * interrupt flags
 */
enum sensor_channel_veml7700 {
  SENSOR_CHAN_VEML7700_RAW_COUNTS = SENSOR_CHAN_PRIV_START,
  SENSOR_CHAN_VEML7700_WHITE_RAW_COUNTS,
  SENSOR_CHAN_VEML7700_INTERRUPT,
};

/* Single raw sample. gain/it snapshotted during capture */
struct veml7700_reading {
  uint16_t als_counts;
  uint16_t white_counts;
  uint8_t gain;
  uint8_t it;
};

/* Runtime driver state */
struct veml7700_data {
  uint8_t shut_down; /* ALS_SD bit */
  enum veml7700_als_gain gain;
  enum veml7700_als_it it;
  enum veml7700_als_pers pers;
  uint16_t thresh_high; /* ALS_WH */
  uint16_t thresh_low;  /* ALS_WL */
  struct veml7700_reading reading;
  uint32_t int_flags; /* ALS_INT flags */
};

/*
 * submit() writes this into the RTIO buffer
 * decode() reads it back later
 * Sensor has no FIFO, so one sample per buffer
 */
struct veml7700_decoder_header {
  uint64_t timestamp;
} __attribute__((__packed__));

struct veml7700_encoded_data {
  struct veml7700_decoder_header header;
  struct {
    uint8_t has_lux : 1; /* set when lux requested */
  } __attribute__((__packed__));
  struct veml7700_reading reading;
};

int veml7700_get_decoder(const struct device *dev,
                         const struct sensor_decoder_api **decoder);

void veml7700_submit(const struct device *dev,
                     struct rtio_iodev_sqe *iodev_sqe);

int veml7700_sample_fetch_helper(const struct device *dev,
                                 enum sensor_channel chan,
                                 struct veml7700_reading *reading);

#endif /* CUSTOM_DRIVERS_SENSOR_VEML7700_H_ */