#!/usr/bin/env bash
# chip-tool로 Matter over Thread 기기 커미셔닝 + 기본 제어 확인
# 사용법: ./commission.sh <DATASET_HEX> [NODE_ID (기본 0x7283)]
#   DATASET_HEX 는 form_network.sh 출력의 'dataset active -x' 값
set -e

DATASET="$1"
NODE_ID="${2:-0x7283}"
PASSCODE=20202021       # esp-matter 예제 기본값 (개발 전용)
DISCRIMINATOR=3840

if [ -z "$DATASET" ]; then
    echo "사용법: $0 <DATASET_HEX> [NODE_ID]" >&2
    exit 1
fi

CHIP_TOOL="${CHIP_TOOL:-chip-tool}"

echo "── 커미셔닝 (BLE → Thread) ──"
"$CHIP_TOOL" pairing ble-thread "$NODE_ID" hex:"$DATASET" "$PASSCODE" "$DISCRIMINATOR"

echo "── 제어 확인 ──"
"$CHIP_TOOL" onoff toggle "$NODE_ID" 1
sleep 2
"$CHIP_TOOL" levelcontrol move-to-level 128 0 0 0 "$NODE_ID" 1   # DIM 50%
sleep 2
"$CHIP_TOOL" colorcontrol move-to-color-temperature 250 0 0 0 "$NODE_ID" 1  # 4000K
echo "완료 — 기기 로그에서 attribute 변경 확인"
