#!/usr/bin/env bash
# Thread 네트워크 생성 + Active Dataset(hex) 출력
# OTBR 컨테이너(run_otbr.sh)가 떠 있어야 함
set -e

OT="docker exec otbr ot-ctl"

$OT dataset init new
$OT dataset commit active
$OT ifconfig up
$OT thread start

sleep 3
echo "── Thread 상태 ──"
$OT state
echo "── Active Dataset (chip-tool pairing ble-thread 에 hex: 로 사용) ──"
$OT dataset active -x
