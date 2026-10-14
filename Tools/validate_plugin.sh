#!/usr/bin/env bash
# Build the VST3 and run pluginval (strictness 5+) plus the Steinberg VST3
# validator when it is on PATH. Prints a pass/fail summary.
#
#   Tools/validate_plugin.sh [path-to.vst3]
#
# Env:
#   SYNTHESIA_ROOT          repo root (default: parent of Tools/)
#   SYNTHESIA_BUILD_DIR     CMake build dir (default: $ROOT/build)
#   SYNTHESIA_EVAL_CACHE    download cache (default: $ROOT/.cache/eval)
#   PLUGINVAL_STRICTNESS    default 5
#   PLUGINVAL_BIN           pre-installed pluginval
#   VST3_SDK_DIR            Steinberg SDK root (looks for build/bin/validator)
#   SKIP_BUILD=1            reuse an existing VST3
#   SKIP_PLUGINVAL=1
#   SKIP_STEINBERG=1
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROOT="${SYNTHESIA_ROOT:-$ROOT}"
BUILD="${SYNTHESIA_BUILD_DIR:-$ROOT/build}"
CACHE="${SYNTHESIA_EVAL_CACHE:-$ROOT/.cache/eval}"
STRICTNESS="${PLUGINVAL_STRICTNESS:-5}"
LOG_DIR="${SYNTHESIA_VALIDATOR_LOG:-$CACHE/validator-logs}"
mkdir -p "$CACHE" "$LOG_DIR"

if [[ "${STRICTNESS}" -lt 5 ]]; then
    echo "PLUGINVAL_STRICTNESS must be >= 5 (got $STRICTNESS)" >&2
    exit 2
fi

uname_s="$(uname -s)"
case "$uname_s" in
    Linux)  PV_ASSET="pluginval_Linux.zip" ;;
    Darwin) PV_ASSET="pluginval_macOS.zip" ;;
    MINGW*|MSYS*|CYGWIN*|Windows_NT) PV_ASSET="pluginval_Windows.zip" ;;
    *)      PV_ASSET="pluginval_Linux.zip" ;;
esac

find_vst3() {
    local cand
    if [[ $# -ge 1 && -e "$1" ]]; then
        printf '%s\n' "$1"
        return 0
    fi
    for cand in \
        "$BUILD/SynthesiaAI_artefacts/Release/VST3/Synthesia AI.vst3" \
        "$BUILD/SynthesiaAI_artefacts/RelWithDebInfo/VST3/Synthesia AI.vst3" \
        "$BUILD/SynthesiaAI_artefacts/Debug/VST3/Synthesia AI.vst3"
    do
        if [[ -e "$cand" ]]; then
            printf '%s\n' "$cand"
            return 0
        fi
    done
    return 1
}

build_vst3() {
    if [[ "${SKIP_BUILD:-0}" == "1" ]]; then
        return 0
    fi
    echo "==> Building SynthesiaAI_VST3" >&2
    if [[ ! -f "$BUILD/CMakeCache.txt" ]]; then
        cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_C_COMPILER="${CC:-gcc}" -DCMAKE_CXX_COMPILER="${CXX:-g++}"
    fi
    cmake --build "$BUILD" --config Release --target SynthesiaAI_VST3 -j"$(nproc 2>/dev/null || echo 4)"
}

download_pluginval() {
    if [[ -n "${PLUGINVAL_BIN:-}" && -x "${PLUGINVAL_BIN}" ]]; then
        printf '%s\n' "$PLUGINVAL_BIN"
        return 0
    fi
    local dest="$CACHE/pluginval"
    mkdir -p "$dest"
    local bin
    bin="$(find "$dest" -type f -name 'pluginval' -perm -111 2>/dev/null | head -n 1 || true)"
    if [[ -n "$bin" ]]; then
        printf '%s\n' "$bin"
        return 0
    fi
    local zip="$CACHE/$PV_ASSET"
    local url="https://github.com/Tracktion/pluginval/releases/latest/download/${PV_ASSET}"
    echo "==> Downloading pluginval ($PV_ASSET)" >&2
    curl -fL --retry 4 --retry-delay 4 -o "$zip" "$url" >&2
    unzip -o "$zip" -d "$dest" >/dev/null
    bin="$(find "$dest" -type f \( -name 'pluginval' -o -name 'pluginval.exe' \) | head -n 1 || true)"
    if [[ -z "$bin" ]]; then
        echo "pluginval binary not found inside $PV_ASSET" >&2
        return 1
    fi
    chmod +x "$bin" || true
    printf '%s\n' "$bin"
}

find_steinberg() {
    local cand
    if [[ -n "${STEINBERG_VALIDATOR:-}" && -x "${STEINBERG_VALIDATOR}" ]]; then
        printf '%s\n' "$STEINBERG_VALIDATOR"
        return 0
    fi
    if command -v validator >/dev/null 2>&1; then
        command -v validator
        return 0
    fi
    if [[ -n "${VST3_SDK_DIR:-}" ]]; then
        for cand in \
            "$VST3_SDK_DIR/build/bin/Release/validator" \
            "$VST3_SDK_DIR/build/bin/Debug/validator" \
            "$VST3_SDK_DIR/build/bin/validator" \
            "$VST3_SDK_DIR/bin/validator"
        do
            if [[ -x "$cand" ]]; then
                printf '%s\n' "$cand"
                return 0
            fi
        done
    fi
    return 1
}

run_pluginval() {
    local vst3="$1"
    local pv="$2"
    local log="$LOG_DIR/pluginval-s${STRICTNESS}.txt"
    local args=(--skip-gui-tests --strictness-level "$STRICTNESS"
                --timeout-ms 600000 --output-dir "$LOG_DIR" --validate "$vst3")
    echo "==> pluginval strictness $STRICTNESS"
    echo "    $pv ${args[*]}"
    set +e
    "$pv" "${args[@]}" 2>&1 | tee "$log"
    local rc=${PIPESTATUS[0]}
    set -e
    if grep -Eqi 'ALL TESTS PASSED|^SUCCESS$|All tests completed successfully' "$log"; then
        echo "pluginval: PASS (strictness $STRICTNESS)"
        return 0
    fi
    if [[ $rc -eq 0 ]]; then
        echo "pluginval: PASS (strictness $STRICTNESS, exit 0)"
        return 0
    fi
    echo "pluginval: FAIL (strictness $STRICTNESS, exit $rc)  log: $log"
    return 1
}

run_steinberg() {
    local vst3="$1"
    local log="$LOG_DIR/steinberg-validator.txt"
    local val
    if ! val="$(find_steinberg)"; then
        echo "steinberg validator: SKIP (not found; set VST3_SDK_DIR or STEINBERG_VALIDATOR)"
        return 2
    fi
    echo "==> Steinberg VST3 validator"
    echo "    $val $vst3"
    set +e
    "$val" "$vst3" 2>&1 | tee "$log"
    local rc=${PIPESTATUS[0]}
    set -e
    if [[ $rc -eq 0 ]]; then
        echo "steinberg validator: PASS"
        return 0
    fi
    echo "steinberg validator: FAIL (exit $rc)  log: $log"
    return 1
}

build_vst3
VST3="$(find_vst3 "${1:-}")" || {
    echo "VST3 bundle not found. Build SynthesiaAI_VST3 or pass a path." >&2
    exit 1
}
echo "VST3: $VST3"

pv_status="SKIP"
st_status="SKIP"
rc=0

if [[ "${SKIP_PLUGINVAL:-0}" != "1" ]]; then
    PV="$(download_pluginval)"
    if run_pluginval "$VST3" "$PV"; then
        pv_status="PASS"
    else
        pv_status="FAIL"
        rc=1
    fi
else
    echo "pluginval: SKIP"
fi

if [[ "${SKIP_STEINBERG:-0}" != "1" ]]; then
    set +e
    run_steinberg "$VST3"
    st_rc=$?
    set -e
    if [[ $st_rc -eq 0 ]]; then
        st_status="PASS"
    elif [[ $st_rc -eq 2 ]]; then
        st_status="SKIP"
    else
        st_status="FAIL"
        rc=1
    fi
else
    echo "steinberg validator: SKIP"
fi

echo
echo "======== plugin validation ========"
echo "  pluginval (strictness $STRICTNESS): $pv_status"
echo "  Steinberg VST3 validator:          $st_status"
echo "  VST3: $VST3"
echo "=================================="
if [[ -n "${SYNTHESIA_VALIDATOR_JSON:-}" ]]; then
    cat > "$SYNTHESIA_VALIDATOR_JSON" <<EOF
{
  "pluginval": {"status": "$pv_status", "strictness": $STRICTNESS},
  "steinberg": {"status": "$st_status"},
  "vst3": $(python3 -c 'import json,sys; print(json.dumps(sys.argv[1]))' "$VST3"),
  "exit": $rc
}
EOF
fi
if [[ "$pv_status" == "FAIL" || "$st_status" == "FAIL" ]]; then
    echo "RESULT: FAIL"
    exit 1
fi
echo "RESULT: PASS"
exit "$rc"
