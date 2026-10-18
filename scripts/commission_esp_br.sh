#!/usr/bin/env bash
# ESP Thread BR + chip-tool(Linux) 로 Matter over Thread 기기 커미셔닝 + 제어 검증
# 사용법: ./commission_esp_br.sh <DATASET_HEX> [NODE_ID=0x7283] [IF=wlp61s0]
#
# 회사/게스트 Wi-Fi 처럼 AP 가 Router Advertisement 를 막는(RA guard) 환경에서는
# 리눅스가 BR 의 on-link prefix 주소와 Thread OMR 경로를 자동으로 못 받으므로 수동 설정한다.
#   BR 콘솔: `ot br onlinkprefix` → ONLINK, `ot br omrprefix` → OMR, BR Wi-Fi MAC → EUI-64 link-local
set -e
DS="$1"; NODE="${2:-0x7283}"; IF="${3:-wlp61s0}"
ONLINK="${ONLINK:-fd93:a5ec:eae2:33b4::/64}"      # ot br onlinkprefix
OMR="${OMR:-fdd4:73a1:6be4::/48}"                 # ot br omrprefix (/48 로 넉넉히)
BR_LL="${BR_LL:-fe80::b63a:45ff:fe18:22b4}"       # BR Wi-Fi MAC b4:3a:45:18:22:b4 의 EUI-64
[ -z "$DS" ] && { echo "사용법: $0 <DATASET_HEX> [NODE_ID] [IF]"; exit 1; }

echo "== 1) 리눅스 IPv6 수동 설정 (RA guard 우회) =="
sudo sysctl -q -w net.ipv6.conf.$IF.accept_ra=2 net.ipv6.conf.$IF.accept_ra_rt_info_max_plen=64
sudo ip -6 addr replace "${ONLINK%/*}100/64" dev "$IF"
sudo ip -6 route replace "$OMR" via "$BR_LL" dev "$IF"
ping6 -c1 -W2 "$BR_LL%$IF" >/dev/null && echo "BR link-local 응답 OK"

echo "== 2) 커미셔닝 (BLE → Thread) =="
chip-tool pairing ble-thread "$NODE" hex:"$DS" 20202021 3840 | grep -E 'commissioning completed|Failure' | sed 's/\x1b\[[0-9;]*m//g'

echo "== 3) 제어 검증 =="
chip-tool onoff on "$NODE" 1 >/dev/null
chip-tool levelcontrol move-to-level-with-on-off 200 0 0 0 "$NODE" 1 >/dev/null
chip-tool colorcontrol move-to-color-temperature 370 0 0 0 "$NODE" 1 >/dev/null   # 2700K (OnOff=on 일 때만 적용)
chip-tool colorcontrol read color-temperature-mireds "$NODE" 1 | sed 's/\x1b\[[0-9;]*m//g' | grep -E 'Mireds|Data ='
echo "완료 — 기기 시리얼의 app_driver 로그에서 CW/WW duty 변화 확인"
