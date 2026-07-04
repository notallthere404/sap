#include "diode.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(diodes, LOG_LEVEL_INF);

#define PWM_PERIOD PWM_USEC(1000) /* 1 kHz */

#define RED_DIODE DT_NODELABEL(ext_red_led)
#define YELLOW_DIODE DT_NODELABEL(ext_yellow_led)
#define GREEN_DIODE DT_NODELABEL(ext_green_led)

static const struct pwm_dt_spec pwm_red = PWM_DT_SPEC_GET(RED_DIODE);
static const struct pwm_dt_spec pwm_yel = PWM_DT_SPEC_GET(YELLOW_DIODE);
static const struct pwm_dt_spec pwm_gre = PWM_DT_SPEC_GET(GREEN_DIODE);

static app_state_t diode_state = APP_STATE_STANDBY;
static bool blink_state;

static void diode_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(diode_work, diode_work_handler);

static int diode_set_percent(const struct pwm_dt_spec *pwm, uint8_t pct) {
  if (pct > 100) {
    pct = 100;
  }

  uint32_t pulse = (PWM_PERIOD * pct) / 100;

  int err = pwm_set_dt(pwm, PWM_PERIOD, pulse);
  if (err) {
    LOG_ERR("PWM set failed for %s channel %u: %d", pwm->dev->name,
            pwm->channel, err);
    return err;
  }

  return 0;
}

static int light_diode(const struct pwm_dt_spec *pwm) {
  return diode_set_percent(pwm, 100);
}

/* Unused for now

static int dim_diode(const struct pwm_dt_spec *pwm) {
  return diode_set_percent(pwm, 20);
}
*/

static int halt_diode(const struct pwm_dt_spec *pwm) {
  return diode_set_percent(pwm, 0);
}

static void all_off(void) {
  halt_diode(&pwm_red);
  halt_diode(&pwm_yel);
  halt_diode(&pwm_gre);
}

static bool pwm_check(const struct pwm_dt_spec *pwm) {
  if (!pwm_is_ready_dt(pwm)) {
    LOG_ERR("PWM device %s not ready", pwm->dev->name);
    return false;
  }

  LOG_INF("Found PWM device %s channel %u", pwm->dev->name, pwm->channel);
  return true;
}

int diodes_init(void) {
  if (!pwm_check(&pwm_red) || !pwm_check(&pwm_yel) || !pwm_check(&pwm_gre)) {
    return -ENODEV;
  }

  all_off();
  diode_set_state(APP_STATE_BACKGROUND);

  return 0;
}

void diode_set_state(app_state_t state) {
  diode_state = state;
  blink_state = false;

  /*
    Reset blinking state
  */
  k_work_cancel_delayable(&diode_work);

  switch (state) {
  case APP_STATE_STANDBY:
    light_diode(&pwm_red);
    halt_diode(&pwm_yel);
    halt_diode(&pwm_gre);
    break;

  case APP_STATE_WAKING:
    all_off();
    k_work_schedule(&diode_work, K_NO_WAIT);
    break;

  case APP_STATE_BACKGROUND:
    halt_diode(&pwm_red);
    light_diode(&pwm_yel);
    halt_diode(&pwm_gre);
    break;

  case APP_STATE_PAIRING:
    all_off();
    k_work_schedule(&diode_work, K_NO_WAIT);
    break;

  case APP_STATE_PAIRED:
    halt_diode(&pwm_red);
    halt_diode(&pwm_yel);
    light_diode(&pwm_gre);
    break;

  case APP_STATE_ERROR:
    all_off();
    k_work_schedule(&diode_work, K_NO_WAIT);
    break;

  default:
    all_off();
    break;
  }
}

static void diode_work_handler(struct k_work *work) {
  ARG_UNUSED(work);

  blink_state = !blink_state;

  switch (diode_state) {
  case APP_STATE_WAKING:
    halt_diode(&pwm_red);
    diode_set_percent(&pwm_yel, blink_state ? 100 : 0);
    halt_diode(&pwm_gre);
    break;

  case APP_STATE_PAIRING:
    halt_diode(&pwm_red);
    halt_diode(&pwm_yel);
    diode_set_percent(&pwm_gre, blink_state ? 100 : 0);
    break;

  case APP_STATE_ERROR:
    diode_set_percent(&pwm_red, blink_state ? 100 : 0);
    halt_diode(&pwm_yel);
    halt_diode(&pwm_gre);
    break;

  default:
    all_off();
    return;
  }

  k_work_schedule(&diode_work, K_MSEC(250));
}