# firmware — Matter over Thread 디바이스 (Arduino 경로)

보더라우터/커미셔너를 **SmartThings Station**이 맡는 구성. Linux/WSL2/Pi OTBR 없이
Windows + Arduino(esp32 core 3.x 내장 Matter 라이브러리)만으로 테스트한다.

```
[ESP32-H2: MatterTemperatureLight] ~~Thread~~ [SmartThings Station (BR+컨트롤러)] ── [SmartThings 앱]
```

## MatterTemperatureLight (Color Temperature Light, ON/OFF + 밝기 + 색온도)

- 원본: esp32 core `libraries/Matter/examples/MatterTemperatureLight` (수정 없음)
- H2는 Wi-Fi가 없어 자동으로 **Thread + CHIPoBLE(BLE 커미셔닝)** 모드
- LED: DevKitM-1 내장 RGB(GPIO8) 자동 사용, BOOT 버튼 = 토글 / 5초 길게 = 커미셔닝 해제

### 빌드 (arduino-cli, IDE 내장 바이너리 사용)

```powershell
$cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
& $cli compile --fqbn "esp32:esp32:esp32h2:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=huge_app" `
    --build-path .\build --export-binaries .\MatterTemperatureLight
```

- `PartitionScheme=huge_app` (3MB APP) — Matter 앱이 기본 1.2MB 파티션에 안 들어감
- `CDCOnBoot=cdc` — DevKit USB 포트로 Serial 로그 출력

### 플래시 + 모니터

```powershell
& $cli upload --fqbn "esp32:esp32:esp32h2:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=huge_app" `
    -p COMx --input-dir .\build .\MatterTemperatureLight
& $cli monitor -p COMx -c baudrate=115200
```

### 커미셔닝 (SmartThings)

1. 시리얼 로그에 `Manual pairing code: 34970112332` / `QR code URL: ...` 출력 확인
2. SmartThings 앱 → `+` → 기기 추가 → **QR 코드 스캔** (URL 열면 QR 이미지) 또는 수동 코드 입력
3. "미인증 기기" 경고 → 계속 (테스트 VID 0xFFF1)
4. 앱에서 ON/OFF·밝기·색온도 조작 → 시리얼에 `Light OnOff changed to ...` 로그 확인

### 트러블슈팅

| 증상 | 확인 |
|---|---|
| 앱에서 기기 못 찾음 | 폰 BLE 켜짐, 기기가 `not commissioned yet` 상태(이미 등록됐으면 BOOT 5초로 해제) |
| 커미셔닝 중 Thread 단계 실패 | Station 펌웨어 최신, 앱 → 허브 정보에 Thread 표시 확인 |
| 등록 후 제어 안 됨 | Station과 폰이 같은 LAN, 기기 전원/거리 |
