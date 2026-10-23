/* device.c - ESP32-C5-WROOM-1 핀 정의                                        */
/* 스트래핑 핀(GPIO25/26/27/28, GPIO7, MTMS=IO2, MTDI=IO3) 회피.                */
/* GPIO28 은 BOOT 스트래핑(풀업 기본)이라 버튼 용도로는 그대로 사용 가능.        */
#include <iot_button.h>
#include <button_gpio.h>
#include <driver/gpio.h>
#include <led_driver.h>
#include <device.h>

#define LED_CW_GPIO_PIN GPIO_NUM_8   /* Cool White PWM (LEDC ch0) */
#define LED_WW_GPIO_PIN GPIO_NUM_9   /* Warm White PWM (LEDC ch1) */
#define BUTTON_GPIO_PIN GPIO_NUM_28  /* BOOT 버튼, Active LOW (내부/외부 풀업) */
#define LED_RGB_GPIO_PIN GPIO_NUM_27 /* C5-DevKitC-1 온보드 WS2812 (양산 WROOM 보드면 -1) */
#define LED_ONOFF_GPIO_PIN GPIO_NUM_10 /* EP3 On/Off Light */
#define LED_DIM_GPIO_PIN  GPIO_NUM_6   /* EP4 Dimmable Light PWM (LEDC ch2) */

/* esp-matter led_driver 호환용 (단일 채널). 본 프로젝트는 cct_light_get_config 사용 */
led_driver_config_t led_driver_get_config()
{
    led_driver_config_t config = {
        .gpio = LED_CW_GPIO_PIN,
        .channel = 0,
        .output_invert = false,
    };
    return config;
}

cct_light_config_t cct_light_get_config()
{
    cct_light_config_t config = {
        .cw_gpio = LED_CW_GPIO_PIN,
        .ww_gpio = LED_WW_GPIO_PIN,
        .rgb_gpio = LED_RGB_GPIO_PIN,
        .onoff_gpio = LED_ONOFF_GPIO_PIN,
        .dim_gpio = LED_DIM_GPIO_PIN,
    };
    return config;
}

button_gpio_config_t button_driver_get_config()
{
    button_gpio_config_t config = {
        .gpio_num = BUTTON_GPIO_PIN,
        .active_level = 0,
    };
    return config;
}
