/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/* app_driver.cpp - 4 엔드포인트 조명 드라이버                                   */
/*   EP1 0x010C Color Temperature Light : CW/WW 2채널 LEDC PWM (GPIO8/9)         */
/*   EP2 0x010D Extended Color Light    : 온보드 WS2812 RGB (HSV / 색온도)       */
/*   EP3 0x0100 On/Off Light            : GPIO 디지털 출력                       */
/*   EP4 0x0101 Dimmable Light          : 단일 LEDC PWM                          */

#include <esp_log.h>
#include <stdlib.h>

#include <app_priv.h>
#include <esp_matter.h>

#include <button_gpio.h>
#include <device.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <led_strip.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;

#define LEDC_MODE_      LEDC_LOW_SPEED_MODE
#define LEDC_TIMER_     LEDC_TIMER_0
#define LEDC_RES_       LEDC_TIMER_13_BIT
#define LEDC_DUTY_MAX   ((1u << 13) - 1)
#define LEDC_FREQ_HZ    5000
#define LEDC_CH_CW      LEDC_CHANNEL_0
#define LEDC_CH_WW      LEDC_CHANNEL_1
#define LEDC_CH_DIM     LEDC_CHANNEL_2

/* ───── EP1 CCT (CW/WW) ───── */
typedef struct { bool power; uint8_t level; uint16_t mireds; } cct_light_t;
static cct_light_t s_cct = { DEFAULT_POWER, DEFAULT_BRIGHTNESS, DEFAULT_MIREDS };

static void cct_render(void)
{
    uint32_t total = s_cct.power ? (uint32_t)LEDC_DUTY_MAX * s_cct.level / MATTER_BRIGHTNESS : 0;
    uint16_t m = s_cct.mireds;
    if (m < CCT_MIREDS_MIN) m = CCT_MIREDS_MIN;
    if (m > CCT_MIREDS_MAX) m = CCT_MIREDS_MAX;
    uint32_t warm = total * (m - CCT_MIREDS_MIN) / (CCT_MIREDS_MAX - CCT_MIREDS_MIN);
    uint32_t cool = total - warm;
    ledc_set_duty(LEDC_MODE_, LEDC_CH_CW, cool); ledc_update_duty(LEDC_MODE_, LEDC_CH_CW);
    ledc_set_duty(LEDC_MODE_, LEDC_CH_WW, warm); ledc_update_duty(LEDC_MODE_, LEDC_CH_WW);
    ESP_LOGI(TAG, "EP1 CCT %s level=%u mireds=%u (%uK) -> CW=%lu WW=%lu",
             s_cct.power ? "ON " : "OFF", s_cct.level, s_cct.mireds,
             s_cct.mireds ? 1000000 / s_cct.mireds : 0, (unsigned long)cool, (unsigned long)warm);
}

/* ───── EP2 Extended Color (온보드 RGB) ───── */
typedef struct {
    bool power; uint8_t level;
    uint8_t mode;           /* ColorMode: 0=HS, 1=XY, 2=CT */
    uint8_t hue, sat;       /* Matter 0~254 */
    uint16_t mireds;
    uint16_t x, y;          /* CIE xy, 0~65535 */
} ext_light_t;
static ext_light_t s_ext = { DEFAULT_POWER, DEFAULT_BRIGHTNESS, 2, 0, 0, DEFAULT_MIREDS, 0, 0 };
static led_strip_handle_t s_strip = NULL;

/* HSV(0~254 스케일) → RGB(0~255) */
static void hsv_to_rgb(uint8_t h254, uint8_t s254, uint8_t v255, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t h = (uint32_t)h254 * 360 / 254;       /* 0~360 */
    uint32_t s = (uint32_t)s254 * 255 / 254;       /* 0~255 */
    uint32_t v = v255;
    uint32_t region = h / 60, rem = (h % 60) * 255 / 60;
    uint32_t p = v * (255 - s) / 255;
    uint32_t q = v * (255 - s * rem / 255) / 255;
    uint32_t t = v * (255 - s * (255 - rem) / 255) / 255;
    switch (region) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
    }
}

/* CIE xy(0~65535) → RGB(0~255), 밝기 v 적용 */
static void xy_to_rgb(uint16_t x16, uint16_t y16, uint8_t v255, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float x = x16 / 65535.0f, y = y16 / 65535.0f;
    if (y < 0.0001f) { *r = *g = *b = 0; return; }
    float z = 1.0f - x - y, Y = 1.0f, X = Y / y * x, Z = Y / y * z;
    float R = X * 1.656f - Y * 0.355f - Z * 0.255f;
    float G = -X * 0.707f + Y * 1.655f + Z * 0.036f;
    float B = X * 0.052f - Y * 0.121f + Z * 1.012f;
    float mx = R > G ? (R > B ? R : B) : (G > B ? G : B);
    if (mx < 0.0001f) mx = 1.0f;
    if (R < 0) R = 0;
    if (G < 0) G = 0;
    if (B < 0) B = 0;
    *r = (uint8_t)(R / mx * v255); *g = (uint8_t)(G / mx * v255); *b = (uint8_t)(B / mx * v255);
}

static void ext_render(void)
{
    if (!s_strip) return;
    uint8_t r = 0, g = 0, b = 0;
    uint8_t v = s_ext.power ? (uint8_t)((uint32_t)s_ext.level * 255 / MATTER_BRIGHTNESS) : 0;
    const char *mn = "CT";
    if (s_ext.mode == 0) {                       /* Hue/Saturation */
        hsv_to_rgb(s_ext.hue, s_ext.sat, v, &r, &g, &b); mn = "HS";
    } else if (s_ext.mode == 1) {                /* XY */
        xy_to_rgb(s_ext.x, s_ext.y, v, &r, &g, &b); mn = "XY";
    } else {                                     /* Color temperature: 쿨(201,226,255)~웜(255,147,41) */
        uint16_t m = s_ext.mireds;
        if (m < CCT_MIREDS_MIN) m = CCT_MIREDS_MIN;
        if (m > CCT_MIREDS_MAX) m = CCT_MIREDS_MAX;
        uint32_t t = CCT_MIREDS_MAX - CCT_MIREDS_MIN, w = m - CCT_MIREDS_MIN;
        r = (201 * (t - w) + 255 * w) / t * v / 255;
        g = (226 * (t - w) + 147 * w) / t * v / 255;
        b = (255 * (t - w) + 41 * w) / t * v / 255;
    }
    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
    ESP_LOGI(TAG, "EP2 EXT %s level=%u mode=%s hue=%u sat=%u mireds=%u -> RGB(%u,%u,%u)",
             s_ext.power ? "ON " : "OFF", s_ext.level, mn, s_ext.hue, s_ext.sat, s_ext.mireds, r, g, b);
}

/* ───── EP3 On/Off (GPIO) ───── */
static int  s_onoff_gpio = -1;
static bool s_onoff = false;
static void onoff_render(void)
{
    if (s_onoff_gpio >= 0) gpio_set_level((gpio_num_t)s_onoff_gpio, s_onoff ? 1 : 0);
    ESP_LOGI(TAG, "EP3 ONOFF %s -> GPIO%d", s_onoff ? "ON " : "OFF", s_onoff_gpio);
}

/* ───── EP4 Dimmable (PWM) ───── */
typedef struct { bool power; uint8_t level; } dim_light_t;
static dim_light_t s_dim = { false, DEFAULT_BRIGHTNESS };
static void dim_render(void)
{
    uint32_t duty = s_dim.power ? (uint32_t)LEDC_DUTY_MAX * s_dim.level / MATTER_BRIGHTNESS : 0;
    ledc_set_duty(LEDC_MODE_, LEDC_CH_DIM, duty); ledc_update_duty(LEDC_MODE_, LEDC_CH_DIM);
    ESP_LOGI(TAG, "EP4 DIM %s level=%u -> duty=%lu", s_dim.power ? "ON " : "OFF", s_dim.level, (unsigned long)duty);
}

/* ───── 버튼: EP1 토글 ───── */
static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    attribute_t *attribute = attribute::get(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    esp_matter_attr_val_t val;
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
}

/* 관심 없는 속성은 반드시 ESP_OK 반환 - 에러 반환 시 속성 쓰기가 거부됨 */
esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    bool is_onoff = (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id);
    bool is_level = (cluster_id == LevelControl::Id && attribute_id == LevelControl::Attributes::CurrentLevel::Id);

    if (endpoint_id == ep_cct_id) {
        if (is_onoff) { s_cct.power = val->val.b; cct_render(); }
        else if (is_level) { s_cct.level = val->val.u8; cct_render(); }
        else if (cluster_id == ColorControl::Id && attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
            s_cct.mireds = val->val.u16; cct_render();
        }
    } else if (endpoint_id == ep_ext_id) {
        if (is_onoff) { s_ext.power = val->val.b; ext_render(); }
        else if (is_level) { s_ext.level = val->val.u8; ext_render(); }
        else if (cluster_id == ColorControl::Id) {
            switch (attribute_id) {
            case ColorControl::Attributes::ColorMode::Id:                s_ext.mode = val->val.u8; ext_render(); break;
            case ColorControl::Attributes::CurrentHue::Id:               s_ext.hue = val->val.u8;  if (s_ext.mode == 0) ext_render(); break;
            case ColorControl::Attributes::CurrentSaturation::Id:        s_ext.sat = val->val.u8;  if (s_ext.mode == 0) ext_render(); break;
            case ColorControl::Attributes::ColorTemperatureMireds::Id:   s_ext.mireds = val->val.u16; if (s_ext.mode == 2) ext_render(); break;
            case ColorControl::Attributes::CurrentX::Id:                 s_ext.x = val->val.u16;   if (s_ext.mode == 1) ext_render(); break;
            case ColorControl::Attributes::CurrentY::Id:                 s_ext.y = val->val.u16;   if (s_ext.mode == 1) ext_render(); break;
            default: break;
            }
        }
    } else if (endpoint_id == ep_onoff_id) {
        if (is_onoff) { s_onoff = val->val.b; onoff_render(); }
    } else if (endpoint_id == ep_dim_id) {
        if (is_onoff) { s_dim.power = val->val.b; dim_render(); }
        else if (is_level) { s_dim.level = val->val.u8; dim_render(); }
    }
    return ESP_OK;
}

static bool     get_b (uint16_t ep, uint32_t cl, uint32_t at) { esp_matter_attr_val_t v; attribute::get_val(attribute::get(ep, cl, at), &v); return v.val.b; }
static uint8_t  get_u8(uint16_t ep, uint32_t cl, uint32_t at) { esp_matter_attr_val_t v; attribute::get_val(attribute::get(ep, cl, at), &v); return v.val.u8; }
static uint16_t get_u16(uint16_t ep, uint32_t cl, uint32_t at){ esp_matter_attr_val_t v; attribute::get_val(attribute::get(ep, cl, at), &v); return v.val.u16; }

/* 데이터 모델의 현재값(NVS 복원 포함)으로 4개 EP 전부 초기 렌더링 */
esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    s_cct.power  = get_b  (ep_cct_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    s_cct.level  = get_u8 (ep_cct_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    s_cct.mireds = get_u16(ep_cct_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
    cct_render();

    s_ext.power  = get_b  (ep_ext_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    s_ext.level  = get_u8 (ep_ext_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    s_ext.mode   = get_u8 (ep_ext_id, ColorControl::Id, ColorControl::Attributes::ColorMode::Id);
    s_ext.hue    = get_u8 (ep_ext_id, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id);
    s_ext.sat    = get_u8 (ep_ext_id, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id);
    s_ext.mireds = get_u16(ep_ext_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
    s_ext.x      = get_u16(ep_ext_id, ColorControl::Id, ColorControl::Attributes::CurrentX::Id);
    s_ext.y      = get_u16(ep_ext_id, ColorControl::Id, ColorControl::Attributes::CurrentY::Id);
    ext_render();

    s_onoff = get_b(ep_onoff_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    onoff_render();

    s_dim.power = get_b (ep_dim_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    s_dim.level = get_u8(ep_dim_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    dim_render();
    return ESP_OK;
}

app_driver_handle_t app_driver_light_init()
{
    cct_light_config_t pins = cct_light_get_config();

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE_, .duty_resolution = LEDC_RES_, .timer_num = LEDC_TIMER_,
        .freq_hz = LEDC_FREQ_HZ, .clk_cfg = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&timer) != ESP_OK) { ESP_LOGE(TAG, "ledc timer config failed"); return NULL; }

    const int gpios[3] = { pins.cw_gpio, pins.ww_gpio, pins.dim_gpio };
    const ledc_channel_t chs[3] = { LEDC_CH_CW, LEDC_CH_WW, LEDC_CH_DIM };
    for (int i = 0; i < 3; i++) {
        ledc_channel_config_t ch = {
            .gpio_num = gpios[i], .speed_mode = LEDC_MODE_, .channel = chs[i],
            .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER_, .duty = 0, .hpoint = 0,
        };
        if (ledc_channel_config(&ch) != ESP_OK) { ESP_LOGE(TAG, "ledc ch%d (gpio %d) failed", i, gpios[i]); return NULL; }
    }

    s_onoff_gpio = pins.onoff_gpio;
    if (s_onoff_gpio >= 0) {
        gpio_config_t io = { .pin_bit_mask = 1ULL << s_onoff_gpio, .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE };
        gpio_config(&io);
        gpio_set_level((gpio_num_t)s_onoff_gpio, 0);
    }

    if (pins.rgb_gpio >= 0) {
        led_strip_config_t strip_cfg = { .strip_gpio_num = pins.rgb_gpio, .max_leds = 1 };
        strip_cfg.led_model = LED_MODEL_WS2812;
        led_strip_rmt_config_t rmt_cfg = {};
        rmt_cfg.resolution_hz = 10 * 1000 * 1000;
        if (led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip) == ESP_OK) {
            led_strip_clear(s_strip);
            ESP_LOGI(TAG, "onboard RGB init: GPIO%d (WS2812)", pins.rgb_gpio);
        } else { ESP_LOGW(TAG, "onboard RGB init failed (GPIO%d)", pins.rgb_gpio); s_strip = NULL; }
    }
    ESP_LOGI(TAG, "init: EP1 CW=GPIO%d WW=GPIO%d | EP2 RGB=GPIO%d | EP3 ONOFF=GPIO%d | EP4 DIM=GPIO%d",
             pins.cw_gpio, pins.ww_gpio, pins.rgb_gpio, pins.onoff_gpio, pins.dim_gpio);
    return (app_driver_handle_t)&s_cct;
}

app_driver_handle_t app_driver_button_init()
{
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
