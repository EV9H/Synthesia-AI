#!/usr/bin/env bash
# Generate N clips per genre with SynthesiaRender, run pluginval + MIDI scoring.
#
#   Tools/eval/run_eval.sh
#   N=3 SKIP_PLUGINVAL=1 Tools/eval/run_eval.sh
#
# Writes eval-reports/<date>/report.md, report.json, clips, and 3 preview MP3s.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROOT="${SYNTHESIA_ROOT:-$ROOT}"
BUILD="${SYNTHESIA_BUILD_DIR:-$ROOT/build}"
CACHE="${SYNTHESIA_EVAL_CACHE:-$ROOT/.cache/eval}"
HOST="${SYNTHESIA_RENDER:-$BUILD/SynthesiaRender_artefacts/Release/SynthesiaRender}"
N="${N:-2}"
DATE="${EVAL_DATE:-$(date -u +%F)}"
REPORT="${EVAL_REPORT_DIR:-$ROOT/eval-reports/$DATE}"
CLIPS="$REPORT/clips"
PY="$CACHE/venv/bin/python"
export SYNTHESIA_ROOT="$ROOT"
export SYNTHESIA_EVAL_CACHE="$CACHE"
mkdir -p "$CACHE" "$CLIPS" "$REPORT"

echo "==> Eval report dir $REPORT"

if [[ ! -x "$HOST" ]]; then
    echo "==> Building SynthesiaRender"
    if [[ ! -f "$BUILD/CMakeCache.txt" ]]; then
        cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_C_COMPILER="${CC:-gcc}" -DCMAKE_CXX_COMPILER="${CXX:-g++}"
    fi
    cmake --build "$BUILD" --config Release --target SynthesiaRender -j"$(nproc 2>/dev/null || echo 4)"
fi

if [[ ! -x "$PY" ]]; then
    echo "==> Creating eval venv"
    python3 -m venv "$CACHE/venv"
    "$CACHE/venv/bin/pip" install -q -U pip
    "$CACHE/venv/bin/pip" install -q -r "$ROOT/Tools/eval/requirements.txt"
fi

VAL_JSON="$REPORT/plugin-validation.json"
if [[ "${SKIP_PLUGINVAL:-0}" == "1" ]]; then
    printf '{"pluginval":{"status":"SKIP"},"steinberg":{"status":"SKIP"}}\n' > "$VAL_JSON"
else
    set +e
    SYNTHESIA_VALIDATOR_JSON="$VAL_JSON" "$ROOT/Tools/validate_plugin.sh" | tee "$REPORT/plugin-validation.txt"
    set -e
fi

GENRES=(pop lofi house trap rnb cinematic)
# Fixed seeds so reports are comparable across commits.
SEEDS=(7 23 41 67 89 103)

echo "==> Generating $N clip(s) per genre"
for genre in "${GENRES[@]}"; do
    for i in $(seq 0 $((N - 1))); do
        seed="${SEEDS[$i]}"
        stem="${genre}_s${seed}"
        echo "  $stem"
        "$HOST" --generator genre --genre "$genre" --part all --bars 8 --form loop \
            --seed "$seed" --midi "$CLIPS/${stem}.mid" --out "$CLIPS/${stem}.wav" \
            >/dev/null
        rm -f "$CLIPS/${stem}.wav"
    done
done

# 3 listen-checks (keep wav -> mp3).
MP3_GENRES=(lofi house trap)
MP3_ARGS=()
echo "==> Rendering preview MP3s"
for genre in "${MP3_GENRES[@]}"; do
    seed="${SEEDS[0]}"
    stem="${genre}_s${seed}"
    wav="$REPORT/${stem}.wav"
    mp3="$REPORT/${stem}.mp3"
    "$HOST" --generator genre --genre "$genre" --part all --bars 8 --form loop \
        --seed "$seed" --midi "$CLIPS/${stem}.mid" --out "$wav" >/dev/null
    ffmpeg -hide_banner -loglevel error -y -i "$wav" -codec:a libmp3lame -b:a 192k "$mp3"
    rm -f "$wav"
    MP3_ARGS+=(--mp3 "${stem}.mp3")
    echo "  $mp3"
done

CRITIC_FLAG=()
if [[ -n "${GROQ_API_KEY:-}" ]]; then
    CRITIC_FLAG=(--critic)
fi

echo "==> Scoring"
"$PY" "$ROOT/Tools/eval/run_eval.py" \
    --clips "$CLIPS" \
    --report "$REPORT" \
    --pluginval-json "$VAL_JSON" \
    "${MP3_ARGS[@]}" \
    "${CRITIC_FLAG[@]}"

echo
echo "Reports: $REPORT/report.md  $REPORT/report.json"
echo "Clips:   $CLIPS"
