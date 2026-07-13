#ifndef APP_ERROR_H
#define APP_ERROR_H

typedef enum {
  APP_ERROR_NONE,
  APP_ERROR_SENSOR_INIT,
  APP_ERROR_SENSOR_READ,
  APP_ERROR_BLE_INIT,
  APP_ERROR_BLE_ADVERTISING,
  APP_ERROR_BLE_CONNECTION,
  APP_ERROR_FATAL,
} app_error_t;

void app_set_error(app_error_t err);
app_error_t app_get_error(void);

#endif