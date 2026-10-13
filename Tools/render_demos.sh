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

# Mix (now including drums) plus a drums-solo for the main genres. Same seed
# so the solo matches the drums track inside the mix. These 8-bar loops use
# the embedded one-shot kits (909 / 808 / dusty acoustic).
#
# name | genre | key | scale | bpm | seed
DEMOS=(
    "lofi-am-84bpm|lofi|A|minor|84|7"
    "house-am-124bpm|house|A|minor|124|7"
    "trap-am-140bpm|trap|A|minor|140|7"
)

SONGS=(
    "lofi-song-24bar|lofi|A|minor|84|11|24"
    "house-song-32bar|house|A|minor|124|11|32"
    "trap-song-32bar|trap|A|minor|140|11|32"
)

# Extra full-arrangement mixes (with drums) so every genre has a listen-check.
MIXES=(
    "pop-am-110bpm|pop|A|minor|110|7"
    "rnb-am-88bpm|rnb|A|minor|88|7"
    "cinematic-am-80bpm|cinematic|A|minor|80|7"
)

PARTS=(drums)

failed=0

check_mp3() {
    local mp3="$1"
    local stats max_db mean_db gaps status
    stats="$(ffmpeg -hide_banner -i "$mp3" -af volumedetect -f null - 2>&1)"
    max_db="$(sed -n 's/.*max_volume: \(-\?[0-9.]*\) dB.*/\1/p' <<< "$stats")"
    mean_db="$(sed -n 's/.*mean_volume: \(-\?[0-9.]*\) dB.*/\1/p' <<< "$stats")"
    # Sparse jazz/R&B comping legitimately rests for more than a second; treat
    # a file as silent only if the whole clip is quiet, or a gap lasts 3s+.
    gaps="$(ffmpeg -hide_banner -i "$mp3" -af silencedetect=noise=-60dB:d=3 -f null - 2>&1 \
            | grep -c 'silence_start' || true)"

    status="ok"
    if awk "BEGIN { exit !($max_db >= -0.1) }"; then status="CLIPPING"; fi
    if awk "BEGIN { exit !($mean_db < -45) }" || [[ "$gaps" -gt 0 ]]; then status="SILENT"; fi
    [[ "$status" == "ok" ]] || failed=1

    printf '  mp3: max %s dB, mean %s dB, silent gaps >3s: %s -> %s\n' "$max_db" "$mean_db" "$gaps" "$status"
}

encode_wav() {
    local wav="$1"
    local mp3="${wav%.wav}.mp3"
    ffmpeg -hide_banner -loglevel error -y -i "$wav" -codec:a libmp3lame -b:a 192k "$mp3"
    check_mp3 "$mp3"
    rm -f "$wav"
}

render() {
    local name="$1" genre="$2" key="$3" scale="$4" bpm="$5" seed="$6" part="$7"
    local extra=("${@:8}")
    local wav="$OUT/${name}.wav"
    local bars_args=(--bars 8)
    for arg in "${extra[@]}"; do
        if [[ "$arg" == "--bars" ]]; then bars_args=(); break; fi
    done

    "$HOST" --out "$wav" --midi "$OUT/${name}.mid" \
        --generator genre --genre "$genre" --part "$part" \
        --key "$key" --scale "$scale" "${bars_args[@]}" \
        --tempo "$bpm" --seed "$seed" "${extra[@]}"

    encode_wav "$wav"
}

for demo in "${DEMOS[@]}"; do
    IFS='|' read -r name genre key scale bpm seed <<< "$demo"

    echo "== $name mix (chords+bass+synth+piano+drums) =="
    render "${name}-mix" "$genre" "$key" "$scale" "$bpm" "$seed" all

    for part in "${PARTS[@]}"; do
        echo "== $name $part solo =="
        render "${name}-${part}" "$genre" "$key" "$scale" "$bpm" "$seed" "$part"
    done
done

for demo in "${MIXES[@]}"; do
    IFS='|' read -r name genre key scale bpm seed <<< "$demo"
    echo "== $name mix (bonus) =="
    render "${name}-mix" "$genre" "$key" "$scale" "$bpm" "$seed" all
done

for demo in "${SONGS[@]}"; do
    IFS='|' read -r name genre key scale bpm seed bars <<< "$demo"
    echo "== $name song-form mix =="
    extra=(--form song --bars "$bars")
    render "${name}-mix" "$genre" "$key" "$scale" "$bpm" "$seed" all "${extra[@]}"
done

echo "== locked custom progression (lofi, 16 bars) =="
render "lofi-locked-am9-mix" lofi A minor 84 13 all \
    --form song --bars 16 --chords "Am9 Fmaj9 Dm7 E7b9" --lock-chords

exit "$failed"
