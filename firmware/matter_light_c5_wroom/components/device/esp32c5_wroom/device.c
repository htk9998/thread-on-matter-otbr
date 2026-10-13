/* device.c - ESP32-C5-WROOM-1 핀 정의                                        */
/* 스트래핑 핀(GPIO25/26/27/28, GPIO7, MTMS=IO2, MTDI=IO3) 회피.                */
/* GPIO28 은 BOOT 스트래핑(풀업 기본)이라 버튼 용도로는 그대로 사용 가능.        */
#include <iot_button.h>
#include <button_gpio.h>
#include <driver/gpio.h>
#include <led_driver.h>

#define LED_GPIO_PIN    GPIO_NUM_8   /* LEDC PWM 출력 (조명 디밍) */
#define LED_CHANNEL     0            /* LEDC_CHANNEL_0 */
#define BUTTON_GPIO_PIN GPIO_NUM_28  /* BOOT 버튼, Active LOW (내부/외부 풀업) */

led_driver_config_t led_driver_get_config()
{
    led_driver_config_t config = {
        .gpio = LED_GPIO_PIN,
        .channel = LED_CHANNEL,
        .output_invert = false,      /* LOW-active 드라이버면 true */
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
