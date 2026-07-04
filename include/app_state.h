#ifndef APP_STATE_H
#define APP_STATE_H

/**
 * Inactive mode
 * Restoring sensors, ble, timers
 * Running normally unpaired
 * Advertising/pairing mode
 * Running normally paired
 * Fault state
 */
typedef enum {
  APP_STATE_STANDBY,
  APP_STATE_WAKING,
  APP_STATE_BACKGROUND,
  APP_STATE_PAIRING,
  APP_STATE_PAIRED,
  APP_STATE_ERROR,
} app_state_t;

void app_set_state(app_state_t state);
app_state_t app_get_state(void);

#endif