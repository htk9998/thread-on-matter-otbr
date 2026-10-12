#!/usr/bin/env bash
# OTBR Docker 실행 (PocketTerm35 / Raspberry Pi)
# 사용법: sudo ./run_otbr.sh [RCP 포트 (기본 /dev/ttyUSB0)] [백본 IF (기본 wlan0)]
set -e

RCP_PORT="${1:-/dev/ttyUSB0}"
BACKBONE_IF="${2:-wlan0}"

if [ ! -e "$RCP_PORT" ]; then
    echo "RCP 포트 $RCP_PORT 없음 — H2(ot_rcp) USB 연결 확인" >&2
    exit 1
fi

# IPv6 포워딩 (OTBR 필수)
sysctl -w net.ipv6.conf.all.forwarding=1 >/dev/null
sysctl -w net.ipv4.ip_forward=1 >/dev/null

docker rm -f otbr 2>/dev/null || true
docker run -d --name otbr --restart unless-stopped \
    --network host --privileged \
    -v "$RCP_PORT":/dev/radio \
    openthread/otbr \
    --radio-url "spinel+hdlc+uart:///dev/radio?uart-baudrate=460800" \
    --backbone-interface "$BACKBONE_IF"

echo "OTBR 실행됨 — 웹 UI: http://$(hostname -I | awk '{print $1}')/"
echo "로그: docker logs -f otbr"
