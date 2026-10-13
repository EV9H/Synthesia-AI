#!/usr/bin/env bash
# Print loudness and spectral mix stats for a bounce (wav/mp3).
# Usage: Tools/analyze_mix.sh clip.wav [clip2.wav ...]
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <audio-file> [more files...]" >&2
    exit 1
fi

measure_rms() {
    local file="$1"
    local filter="$2"
    ffmpeg -hide_banner -nostats -i "$file" -af "${filter}astats=measure_overall=RMS_level:measure_perchannel=0" \
        -f null - 2>&1 | awk '/RMS level dB/ { print $NF; exit }'
}

analyze_one() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        echo "missing: $file" >&2
        return 1
    fi

    local ebu
    ebu=$(ffmpeg -hide_banner -nostats -i "$file" -af ebur128=peak=true -f null - 2>&1)

    local summary
    summary=$(printf '%s\n' "$ebu" | awk '/Summary:/,0')

    local i_lufs lra tp
    i_lufs=$(printf '%s\n' "$summary" | awk '$1=="I:" { print $2; exit }')
    lra=$(printf '%s\n' "$summary" | awk '$1=="LRA:" { print $2; exit }')
    tp=$(printf '%s\n' "$summary" | awk '$1=="Peak:" { print $2; exit }')
    if [[ -z "$i_lufs" ]]; then
        i_lufs=$(printf '%s\n' "$ebu" | awk '/I:/{ print $2; exit }')
    fi
    if [[ -z "$lra" ]]; then
        lra=$(printf '%s\n' "$ebu" | awk '/LRA:/{ print $2; exit }')
    fi
    if [[ -z "$tp" ]]; then
        tp=$(printf '%s\n' "$ebu" | awk '/True peak:/{ getline; print $2; exit }')
    fi

    local rms_all rms_120 rms_400 rms_25k rms_8k rms_side
    rms_all=$(measure_rms "$file" "")
    rms_120=$(measure_rms "$file" "lowpass=f=120:poles=2,")
    rms_400=$(measure_rms "$file" "bandpass=f=400:width_type=h:w=200,")
    rms_25k=$(measure_rms "$file" "bandpass=f=2500:width_type=h:w=800,")
    rms_8k=$(measure_rms "$file" "highpass=f=8000:poles=2,")
    rms_side=$(measure_rms "$file" "pan=mono|c0=0.5*c0-0.5*c1,")

    printf '%s\n' "$file"
    printf '  LUFS_I  %s LUFS\n' "${i_lufs:--}"
    printf '  LRA     %s LU\n' "${lra:--}"
    printf '  TP      %s dBTP\n' "${tp:--}"
    printf '  RMS     %s dB\n' "${rms_all:--}"
    printf '  <120Hz  %s dB\n' "${rms_120:--}"
    printf '  400Hz   %s dB\n' "${rms_400:--}"
    printf '  2.5kHz  %s dB\n' "${rms_25k:--}"
    printf '  >8kHz   %s dB\n' "${rms_8k:--}"
    printf '  side    %s dB\n' "${rms_side:--}"
}

for f in "$@"; do
    analyze_one "$f"
    echo
done
