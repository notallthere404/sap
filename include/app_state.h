#ifndef APP_STATE_H
#define APP_STATE_H

/*
  Central app state that drives
*/
typedef enum {
  /* Shutdown mode */
  APP_STATE_INVALID = -1,
  /* Inactive mode */
  APP_STATE_STANDBY,
  /* Restoring sensors, ble, timers */
  APP_STATE_WAKING,
  /* Running normally unpaired */
  APP_STATE_BACKGROUND,
  /* Advertising/pairing mode */
  APP_STATE_PAIRING,
  /* Running normally paired */
  APP_STATE_PAIRED,
  /* Fault State */
  APP_STATE_ERROR,
} app_state_t;

void app_set_state(app_state_t state);
app_state_t app_get_state(void);

#endif