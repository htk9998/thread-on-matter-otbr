# thread-on-matter-otbr

PocketTerm35(Raspberry Pi 5 핸드헬드, [PocketTerm35 repo](https://github.com/htk9998/PocketTerm35))를
**Matter over Thread 보더라우터(OTBR) + 커미셔너**로 활용하는 프로젝트.
ESP32-H2를 RCP(Radio Co-Processor) 동글로 붙여 Thread 메시를 형성하고,
chip-tool로 Matter 기기(ESP32-C6 등)를 커미셔닝/제어한다.

## 구성

```
[Matter 기기: ESP32-C6 Light] ~~Thread(802.15.4)~~ [ESP32-H2: ot_rcp]
                                                        │ USB (Spinel)
                                              [PocketTerm35 / Pi 5]
                                              otbr-agent + chip-tool
                                                        │ Wi-Fi/Ethernet
                                                  폰 · PC · 생태계 허브
```

| 경로 | 역할 |
|---|---|
| `docs/Matter_Study_Guide.html` | Matter 전체 사양·아키텍처·커미셔닝·보안·Thread/OTBR·esp-matter 스터디 가이드 |
| `scripts/run_otbr.sh` | Pi에서 OTBR Docker 컨테이너 실행 (RCP 포트 지정) |
| `scripts/form_network.sh` | Thread 네트워크 생성 + Active Dataset 추출 |
| `scripts/commission.sh` | chip-tool로 Matter 기기 BLE-Thread 커미셔닝 + 제어 예시 |

## 준비물

- PocketTerm35 (Raspberry Pi 5, Raspberry Pi OS 64-bit)
- ESP32-H2 보드 1개 — RCP 동글 (USB 상주)
- ESP32-C6 보드 — Matter over Thread 엔드 디바이스 (light 예제)
- esp-matter 빌드 호스트 (Linux/macOS/WSL2 — Windows 네이티브 미지원)

## 셋업 순서

### 1. RCP 동글 (ESP32-H2)

ESP-IDF 예제를 그대로 사용 (어느 OS에서든 빌드 가능):

```bash
cd $IDF_PATH/examples/openthread/ot_rcp
idf.py set-target esp32h2
idf.py -p <PORT> flash
```

플래시 후 H2 보드를 PocketTerm35 USB에 연결 → `/dev/ttyUSB0` (또는 `ttyACM0`) 확인.

### 2. OTBR (PocketTerm35)

```bash
sudo ./scripts/run_otbr.sh /dev/ttyUSB0      # Docker로 otbr 실행
./scripts/form_network.sh                     # Thread 네트워크 생성 + dataset 출력
```

웹 UI `http://<pi-ip>:80` 에서도 네트워크 상태 확인 가능.

### 3. Matter 기기 (ESP32-C6, WSL2에서 빌드)

```bash
cd esp-matter/examples/light
idf.py set-target esp32c6
# menuconfig: OPENTHREAD_ENABLED=y / ENABLE_WIFI_STATION=n / USE_MINIMAL_MDNS=n
idf.py build
# .bin은 Windows esptool로 플래시해도 됨
```

### 4. 커미셔닝 + 제어 (PocketTerm35)

```bash
./scripts/commission.sh <DATASET_HEX>
# 내부적으로:
#   chip-tool pairing ble-thread 0x7283 hex:<dataset> 20202021 3840
#   chip-tool onoff toggle 0x7283 1
```

## 진행 상황

- [x] Matter/Thread 스터디 가이드 (docs)
- [ ] H2 ot_rcp 플래시 + PocketTerm35 연결
- [ ] OTBR Docker 기동 + Thread 네트워크 형성
- [ ] C6 light 예제 커미셔닝 (ble-thread)
- [ ] onoff/level/colortemp 제어 검증
- [ ] attribute 콜백 → UART 출력 PoC (게이트웨이 연동)
- [ ] PocketTerm35 게이트웨이 앱(FastAPI)에 Thread 기기 상태 표시 통합

## 참고

- esp-matter: https://docs.espressif.com/projects/esp-matter/en/latest/esp32c6/
- OTBR: https://openthread.io/guides/border-router
- OTBR Docker: https://hub.docker.com/r/openthread/otbr
