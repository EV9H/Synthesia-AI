#!/usr/bin/env bash
# Render demo clips through the plugin with the offline host, encode to MP3 and
# sanity-check levels (no silence, no clipping).
#
#   Tools/render_demos.sh [output-dir]      (default: build/demos)
#
# Requires: ffmpeg (with libmp3lame) and a built SynthesiaRender target:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --target SynthesiaRender
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/build/demos}"
HOST="${SYNTHESIA_RENDER:-$ROOT/build/SynthesiaRender_artefacts/Release/SynthesiaRender}"

if [[ ! -x "$HOST" ]]; then
    echo "SynthesiaRender not found at $HOST (build the SynthesiaRender target first)" >&2
    exit 1
fi

mkdir -p "$OUT"

# name | key | scale | grid | bpm | seed | style prompt
DEMOS=(
    "c-major-eighths-120bpm|C|major|1/8|120|1|bright pop melody"
    "a-minor-sixteenths-128bpm|A|minor|1/16|128|7|driving minor arpeggio"
    "d-dorian-quarters-90bpm|D|dorian|1/4|90|3|laid-back dorian groove"
    "e-pentatonic-eighths-100bpm|E|pentatonic|1/8|100|11|airy pentatonic hook"
)

failed=0

for demo in "${DEMOS[@]}"; do
    IFS='|' read -r name key scale grid bpm seed style <<< "$demo"
    wav="$OUT/$name.wav"

    "$HOST" --out "$wav" --midi "$OUT/$name.mid" \
        --key "$key" --scale "$scale" --bars 8 --quant "$grid" \
        --tempo "$bpm" --seed "$seed" --style "$style"

    ffmpeg -hide_banner -loglevel error -y -i "$wav" -codec:a libmp3lame -b:a 192k "$OUT/$name.mp3"

    stats="$(ffmpeg -hide_banner -i "$OUT/$name.mp3" -af volumedetect -f null - 2>&1)"
    max_db="$(sed -n 's/.*max_volume: \(-\?[0-9.]*\) dB.*/\1/p' <<< "$stats")"
    mean_db="$(sed -n 's/.*mean_volume: \(-\?[0-9.]*\) dB.*/\1/p' <<< "$stats")"
    gaps="$(ffmpeg -hide_banner -i "$OUT/$name.mp3" -af silencedetect=noise=-60dB:d=1 -f null - 2>&1 \
            | grep -c 'silence_start' || true)"

    status="ok"
    if awk "BEGIN { exit !($max_db >= -0.1) }"; then status="CLIPPING"; fi
    if awk "BEGIN { exit !($mean_db < -45) }" || [[ "$gaps" -gt 0 ]]; then status="SILENT"; fi
    [[ "$status" == "ok" ]] || failed=1

    printf '  mp3: max %s dB, mean %s dB, silent gaps >1s: %s -> %s\n' "$max_db" "$mean_db" "$gaps" "$status"
    rm -f "$wav"
done

exit "$failed"
