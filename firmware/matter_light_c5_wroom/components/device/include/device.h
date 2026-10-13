/* device.h - 보드 핀 정의 API (esp-matter device_hal 호환 + CW/WW 2채널 확장) */
#pragma once

#include <led_driver.h>
#include <iot_button.h>
#include <button_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CW/WW 2채널 PWM 조명 핀 (Color Temperature Light) */
typedef struct {
    int cw_gpio;    /* Cool White PWM 출력 */
    int ww_gpio;    /* Warm White PWM 출력 */
} cct_light_config_t;

led_driver_config_t  led_driver_get_config();
button_gpio_config_t button_driver_get_config();
cct_light_config_t   cct_light_get_config();

#ifdef __cplusplus
}
#endif
