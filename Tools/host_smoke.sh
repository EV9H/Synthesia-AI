#!/usr/bin/env bash
# Headless Harmonia smoke: instantiate, Generate, MIDI export, state round-trip.
# Optionally pluginval (strictness 5+) on the built VST3.
#
#   Tools/host_smoke.sh
#
# Env:
#   SYNTHESIA_BUILD_DIR   CMake build dir (default: <repo>/build)
#   HARMONIA_SMOKE_DIR    output dir (default: <repo>/eval-reports/smoke)
#   SKIP_BUILD=1          reuse existing binaries
#   SKIP_PLUGINVAL=1      skip pluginval (still runs SynthesiaRender --smoke)
#   PLUGINVAL_STRICTNESS  default 5
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROOT="${SYNTHESIA_ROOT:-$ROOT}"
BUILD="${SYNTHESIA_BUILD_DIR:-$ROOT/build}"
OUT="${HARMONIA_SMOKE_DIR:-$ROOT/eval-reports/smoke}"
HOST="${SYNTHESIA_RENDER:-}"
mkdir -p "$OUT"

find_vst3() {
    local cand
    for cand in \
        "$BUILD/SynthesiaAI_artefacts/Release/VST3/Harmonia.vst3" \
        "$BUILD/SynthesiaAI_artefacts/RelWithDebInfo/VST3/Harmonia.vst3" \
        "$BUILD/SynthesiaAI_artefacts/Debug/VST3/Harmonia.vst3" \
        "$BUILD/SynthesiaAI_artefacts/Release/VST3/Synthesia AI.vst3"
    do
        if [[ -e "$cand" ]]; then
            printf '%s\n' "$cand"
            return 0
        fi
    done
    return 1
}

find_host() {
    local cand
    if [[ -n "$HOST" && -x "$HOST" ]]; then
        printf '%s\n' "$HOST"
        return 0
    fi
    for cand in \
        "$BUILD/SynthesiaRender_artefacts/Release/SynthesiaRender" \
        "$BUILD/SynthesiaRender_artefacts/RelWithDebInfo/SynthesiaRender" \
        "$BUILD/SynthesiaRender_artefacts/Debug/SynthesiaRender" \
        "$BUILD/SynthesiaRender_artefacts/Release/SynthesiaRender.exe"
    do
        if [[ -x "$cand" || -f "$cand" ]]; then
            printf '%s\n' "$cand"
            return 0
        fi
    done
    return 1
}

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
    echo "==> Building SynthesiaAI_VST3 + SynthesiaRender" >&2
    if [[ ! -f "$BUILD/CMakeCache.txt" ]]; then
        cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
    fi
    cmake --build "$BUILD" --config Release --target SynthesiaAI_VST3 SynthesiaRender \
        -j"$(nproc 2>/dev/null || echo 4)"
fi

VST3="$(find_vst3)" || {
    echo "VST3 not found. Build SynthesiaAI_VST3 first." >&2
    exit 1
}
RENDER="$(find_host)" || {
    echo "SynthesiaRender not found. Build SynthesiaRender first." >&2
    exit 1
}

echo "VST3: $VST3"
echo "HOST: $RENDER"

MIDI="$OUT/harmonia-smoke.mid"
rm -f "$MIDI"

echo "==> SynthesiaRender --smoke (Generate + export + state + VST3 load)"
"$RENDER" --smoke --genre pop --seed 7 --midi "$MIDI" --vst3 "$VST3"

echo "==> verify_midi_export.py"
python3 "$ROOT/Tools/verify_midi_export.py" "$MIDI"

pv_status="SKIP"
if [[ "${SKIP_PLUGINVAL:-0}" != "1" ]]; then
    echo "==> pluginval (strictness ${PLUGINVAL_STRICTNESS:-5})"
    if SYNTHESIA_VALIDATOR_JSON="$OUT/pluginval.json" \
        "$ROOT/Tools/validate_plugin.sh" "$VST3"; then
        pv_status="PASS"
    else
        pv_status="FAIL"
        echo "pluginval failed" >&2
        exit 1
    fi
else
    echo "pluginval: SKIP"
fi

cat > "$OUT/summary.txt" <<EOF
Harmonia host smoke
  VST3: $VST3
  MIDI: $MIDI
  Generate + 960-PPQ export: PASS
  State save/restore (SynthesiaAIState): PASS
  VST3 instantiate via SynthesiaRender --vst3: PASS
  pluginval: $pv_status
Still needs a human: Drag MIDI into Ableton / FL / Reaper and load YOUR sounds.
EOF
cat "$OUT/summary.txt"
echo "SMOKE DIR $OUT"
