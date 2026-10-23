# matter_light_c5_wroom — ESP32-C5-WROOM-1 Matter over Thread 조명 예제

esp-matter `examples/light` 기반. **Thread 전용**(Wi-Fi 비활성), 커미셔너/보더라우터는
SmartThings Station. 빌드는 리눅스 빌드 서버(ESP-IDF v6.0.2 + esp-matter main + Python 3.11).

```
[C5-WROOM: 이 펌웨어] ~~Thread~~ [SmartThings Station] ── [SmartThings 앱]
          │ BLE (커미셔닝 시에만)
```

## 보드 핀 (components/device/esp32c5_wroom/device.c)

| 기능 | GPIO | 비고 |
|---|---|---|
| EP1 CW PWM | **GPIO8** | 0x010C Color Temperature Light, LEDC ch0 |
| EP1 WW PWM | **GPIO9** | LEDC ch1 |
| EP2 온보드 RGB (WS2812) | **GPIO27** | 0x010D Extended Color Light — HSV/XY 풀컬러 + 색온도 모드. 양산 WROOM 보드는 -1 |
| EP3 On/Off 출력 | **GPIO10** | 0x0100 On/Off Light, 디지털 |
| EP4 Dimmable PWM | **GPIO6** | 0x0101 Dimmable Light, LEDC ch2 |
| 버튼 | **GPIO28** | BOOT 핀 겸용, Active LOW. 짧게=토글, 5초=공장초기화 |
| UART0 | GPIO11(TX)/GPIO12(RX) | 다운로드·콘솔 |
| USB | GPIO13/14 | USB-Serial-JTAG (선택) |

핀 변경은 `device.c`의 `LED_CW_GPIO_PIN` / `LED_WW_GPIO_PIN` / `BUTTON_GPIO_PIN`만 수정.

**C5 스트래핑 핀 회피 목록**: GPIO25, 26, 27, 28(BOOT), 7(JTAG), IO2(MTMS), IO3(MTDI) —
이 핀들에 리셋 시 레벨을 바꾸는 회로를 달지 말 것. GPIO16~22는 내부 플래시용(WROOM 미인출).

## 데이터 모델 — 복합 기기 (엔드포인트 4개)

| EP | Device Type | 클러스터 | 출력 |
|---|---|---|---|
| 1 | 0x010C Color Temperature Light | OnOff + Level + ColorControl(CT) | CW/WW PWM |
| 2 | 0x010D Extended Color Light | OnOff + Level + ColorControl(HS/XY/CT) | 온보드 RGB |
| 3 | 0x0100 On/Off Light | OnOff | GPIO10 |
| 4 | 0x0101 Dimmable Light | OnOff + Level | GPIO6 PWM |

`CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT=5`. 생태계 앱에는 한 기기 안의 조명 4개(컴포넌트)로 표시된다.
엔드포인트 구성을 바꾼 뒤에는 기존 등록(Fabric)에서 새 EP가 바로 안 보일 수 있으므로 공장초기화 후 재커미셔닝 권장.

### EP1 상세 — Color Temperature Light (0x010C)

On/Off + Level Control + Color Control(CT 기능) — ON/OFF·밝기·색온도 조명 제품과 동일한 모델.
`main/app_driver.cpp`가 세 속성을 CW/WW duty로 렌더링한다:

- 색온도 범위: **153~370 Mired (6500K~2700K)** — `app_priv.h`의 `CCT_MIREDS_MIN/MAX`, 기구 스펙에 맞게 수정
- Warm 비율 = (mireds − 153) / (370 − 153), CW+WW 합 = 밝기 → 색온도를 바꿔도 광량 일정
- 시리얼 로그: `light ON level=200 mireds=250 (4000K) -> CW=3545 WW=2905`

esp-matter 기본 `led_driver`(단일 PWM)는 색온도 setter가 `ESP_ERR_NOT_SUPPORTED`를 반환해 속성 쓰기가
거부되므로 사용하지 않고, LEDC 2채널을 직접 구동한다. UART로 외부 제어기에 넘기는 구조로 바꾸려면
`cct_render()`만 교체하면 된다.

## 빌드 (리눅스 빌드 서버)

```bash
export PATH=~/esp/py311/bin:$PATH
. ~/esp/esp-idf-v6.0.2/export.sh
. ~/esp/esp-matter/export.sh
cd ~/esp/projects/matter_light_c5_wroom
idf.py set-target esp32c5
idf.py build
```

산출물: `build/light.bin`, `build/bootloader/bootloader.bin`, `build/partition_table/partition-table.bin`,
`build/ota_data_initial.bin`, `build/light.ota.bin`(Matter OTA 이미지).

## 플래시 (Windows, COM 포트 연결된 노트북)

```powershell
# 리눅스에서 산출물 회수 후
python -m esptool --chip esp32c5 -p COM26 -b 460800 write_flash `
  0x2000 bootloader.bin 0xc000 partition-table.bin 0x1d000 ota_data_initial.bin 0x20000 light.bin
```
(정확한 오프셋은 빌드 로그 마지막의 esptool 명령을 그대로 사용)

## 커미셔닝 (SmartThings)

1. `idf.py monitor` 또는 시리얼 터미널(115200) → 부팅 로그의 QR URL / 수동 페어링 코드 확인
   - 콘솔에서 `matter onboardingcodes ble` 로 재출력 가능
2. SmartThings 앱 → `+` → 기기 추가 → QR 스캔 (기본값: passcode 20202021 / discriminator 3840)
3. "미인증 기기" 경고 → 계속 (테스트 VID 0xFFF1)
4. 앱에서 ON/OFF·밝기 조작 → 콘솔에 `app_driver` 로그 + GPIO8 PWM 변화 확인

## 파티션 (4MB)

`partitions.csv`: esp_secure_cert / nvs / otadata / phy / **ota_0, ota_1 (각 1.875MB)** / fctry.
Wi-Fi 포함 기본 빌드는 1.78MB(여유 5%)였으므로, 이 Thread 전용 빌드 크기를 보고 4MB/8MB 결정.
