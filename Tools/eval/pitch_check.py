"""Render a C major scale on each preview instrument and verify playback pitch.

Uses autocorrelation + FFT. Fails if any sustained note is more than 50 cents
off the MIDI pitch after the attack (so 808 pitch-env / wow is ignored).
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np

SCALE_STEPS = (0, 2, 4, 5, 7, 9, 11, 12)
NOTE_DUR = 0.80
GAP = 0.20
SLOT = NOTE_DUR + GAP
CENTS_LIMIT = 50.0
PREVIEW_GENRES = ("lofi", "house", "trap")
# Classification-friendly registers (MidiImporter roles by mean pitch / polyphony).
PARTS = (
    ("bass", 36, False),    # C2 monophonic
    ("piano", 60, False),   # C4 monophonic
    ("synth", 72, False),   # C5 monophonic
    ("chords", 55, True),   # G3 + D4 dyad → chords
)


def midi_hz(pitch: int) -> float:
    return 440.0 * (2.0 ** ((pitch - 69) / 12.0))


def cents(detected: float, expected: float) -> float:
    if detected <= 0.0 or expected <= 0.0:
        return 999.0
    return 1200.0 * np.log2(detected / expected)


def write_scale_midi(path: Path) -> list[dict]:
    import mido

    mid = mido.MidiFile(ticks_per_beat=480)
    track = mido.MidiTrack()
    mid.tracks.append(track)
    track.append(mido.MetaMessage("set_tempo", tempo=500000, time=0))
    events: list[dict] = []
    abs_tick = 0
    for part_i, (part, base, dyad) in enumerate(PARTS):
        # Importer remaps by pitch/polyphony; keep one part per time slice.
        channel = { "chords": 0, "bass": 1, "synth": 2, "piano": 3 }[part]
        for step_i, step in enumerate(SCALE_STEPS):
            pitch = base + step
            start = (part_i * 8 + step_i) * SLOT
            start_tick = int(round(start * 480 * 2))  # 120 bpm, 480 tpq
            dur_tick = int(round(NOTE_DUR * 480 * 2))
            delta = start_tick - abs_tick
            track.append(mido.Message("note_on", note=pitch, velocity=96,
                                      channel=channel, time=max(0, delta)))
            if dyad:
                fifth = pitch + 7
                track.append(mido.Message("note_on", note=fifth, velocity=80,
                                          channel=channel, time=0))
            track.append(mido.Message("note_off", note=pitch, velocity=0,
                                      channel=channel, time=dur_tick))
            if dyad:
                track.append(mido.Message("note_off", note=fifth, velocity=0,
                                          channel=channel, time=0))
            abs_tick = start_tick + dur_tick
            events.append({
                "part": part,
                "pitch": pitch,
                "start": start,
                "expected_hz": midi_hz(pitch),
            })
    mid.save(path)
    return events


def read_wav(path: Path) -> tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as handle:
        channels = handle.getnchannels()
        width = handle.getsampwidth()
        sr = handle.getframerate()
        raw = handle.readframes(handle.getnframes())
    if width == 3:
        packed = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        ints = (packed[:, 0].astype(np.int32)
                | (packed[:, 1].astype(np.int32) << 8)
                | (packed[:, 2].astype(np.int32) << 16))
        ints = np.where(ints >= 0x800000, ints - 0x1000000, ints)
        data = ints.astype(np.float64) / 8388608.0
    elif width == 2:
        data = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 4:
        # JUCE 24-bit write is 3; 32-bit may be int or float.
        as_float = np.frombuffer(raw, dtype="<f4")
        if np.isfinite(as_float).mean() > 0.9 and np.max(np.abs(as_float)) <= 8.0:
            data = as_float.astype(np.float64)
        else:
            data = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f"unsupported wav width {width}")
    if channels > 1:
        data = data.reshape(-1, channels).mean(axis=1)
    return sr, data


def _parabolic(y: np.ndarray, i: int) -> float:
    if i <= 0 or i >= len(y) - 1:
        return float(i)
    a, b, c = float(y[i - 1]), float(y[i]), float(y[i + 1])
    denom = a - 2.0 * b + c
    return float(i) + 0.5 * (a - c) / denom if abs(denom) > 1e-12 else float(i)


def detect_pitch(x: np.ndarray, sr: int, expected: float) -> float | None:
    if x.size < 64 or float(np.max(np.abs(x))) < 1e-4:
        return None
    x = x - float(np.mean(x))
    win = x * np.hanning(len(x))

    # Autocorrelation (good for bass / 808 after the drop).
    corr = np.correlate(win, win, mode="full")
    corr = corr[len(corr) // 2 :]
    fmin, fmax = 32.0, 2000.0
    min_lag = max(2, int(sr / fmax))
    max_lag = min(len(corr) - 2, int(sr / fmin))
    f_ac = None
    if max_lag > min_lag + 4:
        seg = corr[min_lag:max_lag]
        peak = int(np.argmax(seg))
        lag = _parabolic(seg, peak) + min_lag
        if lag > 1:
            f_ac = sr / lag

    # FFT: pick the bin nearest expected, or the strongest below 2 kHz.
    spec = np.abs(np.fft.rfft(win))
    freqs = np.fft.rfftfreq(len(win), 1.0 / sr)
    band = (freqs >= 32.0) & (freqs <= 2000.0)
    f_fft = None
    if np.any(band):
        # Prefer the peak closest to expected / its first harmonic.
        candidates = []
        for harm in (1.0, 2.0, 0.5):
            target = expected * harm
            if target < 32.0 or target > 2000.0:
                continue
            nearest = int(np.argmin(np.abs(freqs - target)))
            lo = max(0, nearest - 4)
            hi = min(len(spec), nearest + 5)
            local = lo + int(np.argmax(spec[lo:hi]))
            idx = _parabolic(spec, local)
            freq = float(np.interp(idx, np.arange(len(freqs)), freqs))
            if harm == 2.0:
                freq *= 0.5
            elif harm == 0.5:
                freq *= 2.0
            candidates.append((float(spec[local]), freq))
        if candidates:
            candidates.sort(reverse=True)
            f_fft = candidates[0][1]

    picks = [f for f in (f_ac, f_fft) if f]
    if not picks:
        return None
    return min(picks, key=lambda f: abs(cents(f, expected)))


def check_render(sr: int, audio: np.ndarray, events: list[dict], genre: str) -> list[dict]:
    rows = []
    for ev in events:
        start = ev["start"]
        # Skip attack (808 pitch env ~48 ms, wow is cents-level).
        skip = 0.28 if ev["part"] == "bass" and genre == "trap" else 0.12
        a0 = int((start + skip) * sr)
        a1 = int((start + NOTE_DUR - 0.06) * sr)
        a1 = min(len(audio), max(a0 + int(0.15 * sr), a1))
        if a0 >= len(audio) or a1 - a0 < 64:
            rows.append({**ev, "ok": False, "cents": 999.0, "detected_hz": None, "reason": "short"})
            continue
        detected = detect_pitch(audio[a0:a1], sr, ev["expected_hz"])
        err = 999.0 if detected is None else float(cents(detected, ev["expected_hz"]))
        rows.append({
            **ev,
            "ok": detected is not None and abs(err) <= CENTS_LIMIT,
            "cents": round(err, 1),
            "detected_hz": None if detected is None else round(detected, 2),
            "reason": "" if detected is not None and abs(err) <= CENTS_LIMIT
            else ("no pitch" if detected is None else f"{err:+.1f} cents"),
        })
    return rows


def run(host: Path, out_json: Path, work: Path) -> dict:
    midi_path = work / "c_major_scale.mid"
    events = write_scale_midi(midi_path)
    payload = {"cents_limit": CENTS_LIMIT, "genres": {}, "ok": True}
    for genre in PREVIEW_GENRES:
        wav_path = work / f"scale_{genre}.wav"
        cmd = [
            str(host),
            "--import", str(midi_path),
            "--import-mode", "off",
            "--genre", genre,
            "--tempo", "120",
            "--key", "C",
            "--scale", "major",
            "--out", str(wav_path),
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0 or not wav_path.exists():
            payload["ok"] = False
            payload["genres"][genre] = {
                "ok": False,
                "error": proc.stderr.strip() or proc.stdout.strip() or f"exit {proc.returncode}",
            }
            continue
        sr, audio = read_wav(wav_path)
        rows = check_render(sr, audio, events, genre)
        failed = [r for r in rows if not r["ok"]]
        payload["genres"][genre] = {
            "ok": not failed,
            "n": len(rows),
            "failed": len(failed),
            "notes": rows,
        }
        if failed:
            payload["ok"] = False
    out_json.parent.mkdir(parents=True, exist_ok=True)
    out_json.write_text(json.dumps(payload, indent=2))
    return payload


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--work", type=Path, default=None)
    args = ap.parse_args()
    if not args.host.exists():
        print(f"pitch_check: host not found: {args.host}", file=sys.stderr)
        return 2
    tmp = args.work
    if tmp is None:
        with tempfile.TemporaryDirectory(prefix="pitch-check-") as folder:
            payload = run(args.host, args.out, Path(folder))
    else:
        tmp.mkdir(parents=True, exist_ok=True)
        payload = run(args.host, args.out, tmp)
    for genre, row in payload.get("genres", {}).items():
        if row.get("ok"):
            print(f"  {genre}: pitch ok ({row.get('n', 0)} notes)")
        elif "error" in row:
            print(f"  {genre}: RENDER FAIL {row['error']}")
        else:
            bad = [n for n in row.get("notes", []) if not n["ok"]]
            print(f"  {genre}: {len(bad)} off-pitch notes")
            for note in bad[:8]:
                print(f"    {note['part']} {note['pitch']} {note['reason']}")
    if not payload["ok"]:
        print("SAMPLER PITCH CHECK FAILED", file=sys.stderr)
        return 1
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
