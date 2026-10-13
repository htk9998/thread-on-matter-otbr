/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/* app_driver.cpp - CW/WW 2채널 LEDC PWM 조명 드라이버 (Color Temperature Light)  */
/*   OnOff / CurrentLevel / ColorTemperatureMireds → CW·WW duty 로 렌더링          */
/*   Mired 가 클수록(저 K) Warm 비율↑. CW+WW 합 = 밝기 → 색온도 변화 시 광량 일정  */

#include <esp_log.h>
#include <stdlib.h>

#include <app_priv.h>
#include <esp_matter.h>

#include <button_gpio.h>
#include <device.h>
#include <driver/ledc.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;

#define CCT_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define CCT_LEDC_TIMER      LEDC_TIMER_0
#define CCT_LEDC_RES        LEDC_TIMER_13_BIT
#define CCT_LEDC_DUTY_MAX   ((1u << 13) - 1)
#define CCT_LEDC_FREQ_HZ    5000
#define CCT_LEDC_CH_CW      LEDC_CHANNEL_0
#define CCT_LEDC_CH_WW      LEDC_CHANNEL_1

typedef struct {
    bool     power;
    uint8_t  level;     /* Matter CurrentLevel 1~254 */
    uint16_t mireds;    /* Matter ColorTemperatureMireds */
} cct_light_t;

static cct_light_t s_light = {
    .power  = DEFAULT_POWER,
    .level  = DEFAULT_BRIGHTNESS,
    .mireds = DEFAULT_MIREDS,
};

static void cct_render(void)
{
    uint32_t total = 0;
    if (s_light.power) {
        total = (uint32_t)CCT_LEDC_DUTY_MAX * s_light.level / MATTER_BRIGHTNESS;
    }

    uint16_t m = s_light.mireds;
    if (m < CCT_MIREDS_MIN) m = CCT_MIREDS_MIN;
    if (m > CCT_MIREDS_MAX) m = CCT_MIREDS_MAX;
    uint32_t warm = total * (m - CCT_MIREDS_MIN) / (CCT_MIREDS_MAX - CCT_MIREDS_MIN);
    uint32_t cool = total - warm;

    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CH_CW, cool);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CH_CW);
    ledc_set_duty(CCT_LEDC_MODE, CCT_LEDC_CH_WW, warm);
    ledc_update_duty(CCT_LEDC_MODE, CCT_LEDC_CH_WW);

    ESP_LOGI(TAG, "light %s level=%u mireds=%u (%uK) -> CW=%lu WW=%lu",
             s_light.power ? "ON " : "OFF", s_light.level, s_light.mireds,
             s_light.mireds ? 1000000 / s_light.mireds : 0,
             (unsigned long)cool, (unsigned long)warm);
}

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);

    esp_matter_attr_val_t val;
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

/* 관심 없는 속성(ColorMode 등)은 반드시 ESP_OK 반환 - 에러 반환 시 속성 쓰기가 거부됨 */
esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    if (endpoint_id != light_endpoint_id) {
        return ESP_OK;
    }
    if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        s_light.power = val->val.b;
        cct_render();
    } else if (cluster_id == LevelControl::Id && attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
        s_light.level = val->val.u8;
        cct_render();
    } else if (cluster_id == ColorControl::Id &&
               attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
        s_light.mireds = val->val.u16;
        cct_render();
    }
    return ESP_OK;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_matter_attr_val_t val;
    attribute_t *attribute;

    attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    s_light.power = val.val.b;

    attribute = attribute::get(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    s_light.level = val.val.u8;

    attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
    attribute::get_val(attribute, &val);
    s_light.mireds = val.val.u16;

    cct_render();
    return ESP_OK;
}

app_driver_handle_t app_driver_light_init()
{
    cct_light_config_t pins = cct_light_get_config();

    ledc_timer_config_t timer = {
        .speed_mode      = CCT_LEDC_MODE,
        .duty_resolution = CCT_LEDC_RES,
        .timer_num       = CCT_LEDC_TIMER,
        .freq_hz         = CCT_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&timer) != ESP_OK) {
        ESP_LOGE(TAG, "ledc timer config failed");
        return NULL;
    }

    const int gpios[2]           = { pins.cw_gpio, pins.ww_gpio };
    const ledc_channel_t chs[2]  = { CCT_LEDC_CH_CW, CCT_LEDC_CH_WW };
    for (int i = 0; i < 2; i++) {
        ledc_channel_config_t ch = {
            .gpio_num   = gpios[i],
            .speed_mode = CCT_LEDC_MODE,
            .channel    = chs[i],
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = CCT_LEDC_TIMER,
            .duty       = 0,
            .hpoint     = 0,
        };
        if (ledc_channel_config(&ch) != ESP_OK) {
            ESP_LOGE(TAG, "ledc channel %d config failed (gpio %d)", i, gpios[i]);
            return NULL;
        }
    }
    ESP_LOGI(TAG, "CW/WW PWM init: CW=GPIO%d WW=GPIO%d, %dHz 13bit",
             pins.cw_gpio, pins.ww_gpio, CCT_LEDC_FREQ_HZ);
    return (app_driver_handle_t)&s_light;
}

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = button_driver_get_config();

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}
