#!/bin/bash
# 두 서버 실행 파일을 번갈아 실행하며 wrk로 측정하는 스크립트
# 사용법: ./scripts/bench_compare.sh <전 실행파일> <후 실행파일> [반복 횟수] [URL]
# 예시:   ./scripts/bench_compare.sh server_before server_after 5 http://127.0.0.1:8080/
# 반드시 프로젝트 루트(www 폴더가 있는 곳)에서 실행할 것

BEFORE=$1
AFTER=$2
ROUNDS=${3:-5}
URL=${4:-http://127.0.0.1:8080/}

if [ -z "$BEFORE" ] || [ -z "$AFTER" ]; then
    echo "사용법: $0 <전 실행파일> <후 실행파일> [반복 횟수] [URL]"
    exit 1
fi

if pgrep -x server > /dev/null; then
    echo "8080 포트를 쓰는 server가 이미 실행 중입니다. 먼저 종료하세요 (pkill -x server)."
    exit 1
fi

mkdir -p bench_results
OUT="bench_results/compare_$(date +%m%d_%H%M).txt"
echo "# $BEFORE vs $AFTER, $ROUNDS rounds, $URL, wrk -t2 -c100 -d10s" | tee "$OUT"
echo "# round binary requests_per_sec latency_avg" | tee -a "$OUT"

for i in $(seq 1 "$ROUNDS"); do
    for BIN in "$BEFORE" "$AFTER"; do
        ./"$BIN" > /dev/null 2>&1 &
        PID=$!
        sleep 1                                   # 서버가 뜰 시간

        RESULT=$(wrk -t2 -c100 -d10s "$URL")
        RPS=$(echo "$RESULT" | awk '/^Requests\/sec/ {print $2}')
        LAT=$(echo "$RESULT" | awk '/Latency/ {print $2; exit}')
        ERR=$(echo "$RESULT" | grep -E "Socket errors|Non-2xx")

        echo "$i $BIN $RPS $LAT" | tee -a "$OUT"
        if [ -n "$ERR" ]; then echo "  경고: $ERR" | tee -a "$OUT"; fi

        kill "$PID"
        wait "$PID" 2> /dev/null
        sleep 1                                   # 포트가 풀릴 시간
    done
done

echo "" | tee -a "$OUT"
awk '!/^#/ && NF >= 3 {sum[$2] += $3; n[$2]++}
     END {for (b in sum) printf "%s 평균: %.0f req/s (%d회)\n", b, sum[b]/n[b], n[b]}' "$OUT" | tee -a "$OUT"
echo "결과 저장: $OUT"