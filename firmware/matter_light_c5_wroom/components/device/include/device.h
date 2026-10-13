/* device.h - 보드 핀 정의 API (esp-matter device_hal 호환) */
#pragma once

#include <led_driver.h>
#include <iot_button.h>
#include <button_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

led_driver_config_t led_driver_get_config();
button_gpio_config_t button_driver_get_config();

#ifdef __cplusplus
}
#endif
