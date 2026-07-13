#ifndef BLE_H
#define BLE_H

int ble_init(void);
int ble_set_reading(const char *str);
int ble_adv_start(void);
int ble_adv_stop(void);

#endif