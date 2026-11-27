#!/usr/bin/env bash
# Watchdog test senaryolari (Odev Section 7)
# Kullanim: bash tests/test_scenarios.sh      (proje kokunden)
#   WATCHDOG=./watchdog  LOG=watchdog.log  ile degistirilebilir.
# Not: Linux (veya WSL) gerektirir.

WATCHDOG=${WATCHDOG:-./watchdog}
LOG=${LOG:-watchdog.log}
RESULTS=tests/results
PASS=0; FAIL=0; WD=""

pass() { echo "  [PASS] $1"; PASS=$((PASS+1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }
log_has() { grep -qiE "$1" "$LOG" 2>/dev/null; }

wait_log() {            # wait_log <regex> [saniye]
    local i
    for ((i = 0; i < ${2:-5} * 10; i++)); do
        log_has "$1" && return 0
        sleep 0.1
    done
    return 1
}

children() { pgrep -P "$WD" -x worker | sort; }

start_wd() {
    rm -f "$LOG"
    "$WATCHDOG" >/dev/null 2>&1 &
    WD=$!
    sleep 1.5
}

save_log() { mkdir -p "$RESULTS"; cp "$LOG" "$RESULTS/$1.log" 2>/dev/null; }

cleanup() {
    if [ -n "$WD" ] && kill -0 "$WD" 2>/dev/null; then
        kill -TERM "$WD" 2>/dev/null; sleep 1
        kill -KILL "$WD" 2>/dev/null
        pkill -KILL -x worker 2>/dev/null
    fi
}
trap cleanup EXIT

# Verilen worker'i SIGKILL ile oldurur, yeni bir child belirmesini bekler.
# Yeni PID'yi yazar, belirmezse bos doner.
kill_and_wait_restart() {
    local target=$1 before after new i
    before=$(children)
    kill -KILL "$target" 2>/dev/null
    for ((i = 0; i < 50; i++)); do
        sleep 0.1
        after=$(children)
        new=$(comm -13 <(echo "$before") <(echo "$after"))
        if [ -n "$new" ]; then echo "$new" | head -1; return; fi
    done
}

check_is_worker() {     # PID dogrulama (odev bunu istiyor)
    [ "$(ps -p "$1" -o comm= 2>/dev/null)" = "worker" ]
}

shutdown_test() {       # shutdown_test <SIGTERM|SIGINT> <run-adi>
    local sig=$1 name=$2 pids p alive=0 i
    start_wd
    pids=$(children)
    kill -"$sig" "$WD"
    for ((i = 0; i < 100; i++)); do
        kill -0 "$WD" 2>/dev/null || break
        sleep 0.1
    done
    kill -0 "$WD" 2>/dev/null && fail "$sig sonrasi watchdog kapanmadi" \
                              || pass "$sig sonrasi watchdog kapandi"
    wait "$WD" 2>/dev/null; echo "    (watchdog cikis kodu: $?)"
    for p in $pids; do kill -0 "$p" 2>/dev/null && alive=1; done
    [ $alive -eq 0 ] && pass "tum worker'lar sonlandirildi ve toplandi" \
                     || fail "calisan worker kaldi"
    log_has "shutdown" && pass "shutdown logda var" || fail "shutdown logu yok"
    save_log "$name"
    WD=""
}

echo "=== Senaryo 1-3: normal calisma, durum raporu, normal sonlanma ==="
start_wd
[ "$(children | wc -l)" -ge 2 ] && pass "en az 2 worker calisiyor" || fail "2'den az worker"
log_has "started" && pass "baslatma logda var" || fail "baslatma logu yok"

kill -USR1 "$WD"
wait_log "STATUS REPORT" 3 && pass "SIGUSR1 raporu logda" || fail "SIGUSR1 raporu yok"
n=$(awk '/STATUS REPORT/{f=1} f' "$LOG" | grep -c "pid=")
[ "$n" -ge 2 ] && pass "rapor >=2 worker'in PID/durumunu iceriyor" || fail "rapor eksik ($n satir)"

T=$(children | head -1)
if check_is_worker "$T"; then
    N_BEFORE=$(children | wc -l)
    kill -USR2 "$T"       # worker temiz exit(0) yapar
    wait_log "normal" 4 && pass "normal sonlanma tespit edildi" || fail "normal sonlanma logu yok"
    sleep 2
    ! kill -0 "$T" 2>/dev/null && [ "$(children | wc -l)" -eq $((N_BEFORE - 1)) ] \
        && pass "normal cikan worker yeniden baslatilmadi" \
        || fail "normal cikan worker yeniden baslatildi"
else
    fail "PID dogrulanamadi"
fi
save_log run_normal
cleanup; WD=""

echo "=== Senaryo 4-5: beklenmeyen sonlanma ve restart limiti ==="
start_wd
T=$(children | head -1)
if ! check_is_worker "$T"; then fail "PID dogrulanamadi"; else
    for attempt in 1 2 3 4; do
        NEW=$(kill_and_wait_restart "$T")
        if [ "$attempt" -le 3 ]; then
            [ -n "$NEW" ] && pass "olum #$attempt sonrasi restart edildi" \
                          || fail "olum #$attempt sonrasi restart YOK"
            T=$NEW
        else
            [ -z "$NEW" ] && pass "limit asildi, yeni restart yapilmadi" \
                          || fail "limit asildi ama yine restart edildi"
        fi
        [ -z "$T" ] && break
    done
    log_has "restart" && pass "restart logda var" || fail "restart logu yok"
    log_has "limit"   && pass "limit logda var"   || fail "limit logu yok"
fi
save_log run_restart
cleanup; WD=""

echo "=== Senaryo 6: graceful shutdown (SIGTERM) ==="
shutdown_test SIGTERM run_sigterm

echo "=== Senaryo 6b: graceful shutdown (SIGINT / Ctrl+C benzetimi) ==="
shutdown_test SIGINT run_sigint

echo "=== Senaryo 7: log dogrulama (zaman damgasi) ==="
bad=0
for f in "$RESULTS"/*.log; do
    grep -qvE '^\[[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\] ' "$f" && bad=1
done
[ $bad -eq 0 ] && pass "tum log satirlari zaman damgali" || fail "zaman damgasiz satir var"

echo
echo "SONUC: $PASS gecti, $FAIL basarisiz"
echo "Not: Ctrl+C davranisini ayrica elle de dene ve ekran goruntusu al."
[ "$FAIL" -eq 0 ]
