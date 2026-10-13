cmake_minimum_required(VERSION 3.5)
if (NOT ("${IDF_TARGET}" STREQUAL "esp32c5" ))
    message(FATAL_ERROR "please set esp32c5 as the IDF_TARGET using 'idf.py set-target esp32c5'")
endif()

# ESP32-C5-WROOM-1 커스텀 보드
#   led_type  gpio : LEDC PWM 단일 채널 (밝기 제어, 색상/색온도 함수는 컴파일 제외)
#   button_type iot: iot_button GPIO
SET(device_type     esp32c5_wroom)
SET(led_type        gpio)
SET(button_type     iot)

SET(extra_components_dirs_append "$ENV{ESP_MATTER_PATH}/device_hal/led_driver"
                                 "$ENV{ESP_MATTER_PATH}/device_hal/button_driver/iot_button")
