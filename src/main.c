#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(Sensor_Array_Plants, LOG_LEVEL_INF);

#define AIR_NODE DT_NODELABEL(bme280)
#define LIGHT_NODE DT_NODELABEL(veml7700)

const struct device* const dev_air = DEVICE_DT_GET(AIR_NODE);
const struct device* const dev_light = DEVICE_DT_GET(LIGHT_NODE);

/*
        Defines sensor read instances
*/
SENSOR_DT_READ_IODEV(iodev_air, AIR_NODE, {SENSOR_CHAN_AMBIENT_TEMP, 0},
                     {SENSOR_CHAN_PRESS, 0}, {SENSOR_CHAN_HUMIDITY, 0});

SENSOR_DT_READ_IODEV(iodev_light, LIGHT_NODE, {SENSOR_CHAN_LIGHT, 0});

RTIO_DEFINE(ctx, 1, 1);

static bool device_check(const struct device* dev) {
    if (!device_is_ready(dev)) {
        LOG_ERR("Device %s not ready", dev->name);
        return false;
    }
    LOG_INF("Found device %s", dev->name);
    return true;
}

int main(void) {
    if (!device_check(dev_air) || !device_check(dev_light)) {
        return 1;
    }
    return 0;
}
