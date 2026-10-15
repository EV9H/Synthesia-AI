"""Key-scale / chord-tone checks for generated clips."""
from __future__ import annotations

from collections import defaultdict
from typing import Any

from midi_parse import Clip, Note

PC_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

SCALE_INTERVALS = {
    "major": (0, 2, 4, 5, 7, 9, 11),
    "minor": (0, 2, 3, 5, 7, 8, 10),
    "dorian": (0, 2, 3, 5, 7, 9, 10),
    "mixolydian": (0, 2, 4, 5, 7, 9, 10),
    "pentatonic": (0, 2, 4, 7, 9),
}


def scale_pcs(root: int, scale: str) -> set[int]:
    intervals = SCALE_INTERVALS.get(scale, SCALE_INTERVALS["major"])
    return { (root + i) % 12 for i in intervals }


def pc_name(pitch: int) -> str:
    return f"{PC_NAMES[pitch % 12]}{pitch // 12 - 1}"


def chord_at_beat(clip: Clip, beat: float) -> frozenset[int]:
    t = beat * 60.0 / max(clip.tempo, 1.0)
    pcs = {n.pitch % 12 for n in clip.of("chords") if n.start - 0.02 <= t < n.end}
    pcs |= {n.pitch % 12 for n in clip.of("bass") if n.start - 0.02 <= t < n.end}
    if len(pcs) < 2:
        pcs |= {n.pitch % 12 for n in clip.of("piano") if n.start - 0.02 <= t < n.end and n.pitch >= 48}
    return frozenset(pcs)


def note_beat(clip: Clip, note: Note) -> float:
    return clip.time_to_beat(note.start)


def is_strong_beat(beat: float) -> bool:
    frac = beat - 4.0 * int(beat / 4.0 + 1e-9)
    return frac < 0.14 or 1.86 < frac < 2.14


def analyze_harmony(clip: Clip, root: int = 0, scale: str = "major") -> dict[str, Any]:
    key = scale_pcs(root, scale)
    by_part: dict[str, dict[str, Any]] = {}
    examples: list[dict[str, Any]] = []
    total = 0
    out_key = 0
    out_key_not_chord = 0
    strong_off_chord = 0
    strong_total = 0
    chromatic_pass = 0

    for part in ("chords", "bass", "synth", "piano"):
        notes = [n for n in clip.of(part) if not n.is_drum]
        if not notes:
            continue
        part_out_key = 0
        part_out_chord = 0
        part_strong_off = 0
        part_strong = 0
        part_examples: list[dict[str, Any]] = []
        for n in notes:
            beat = note_beat(clip, n)
            bar = int(beat / 4.0 + 1e-9)
            chord = chord_at_beat(clip, beat)
            pc = n.pitch % 12
            in_key = pc in key
            in_chord = pc in chord if chord else False
            strong = is_strong_beat(beat)
            total += 1
            if strong:
                strong_total += 1
                part_strong += 1
            if not in_key:
                part_out_key += 1
                out_key += 1
                if not in_chord:
                    out_key_not_chord += 1
            if not in_chord:
                part_out_chord += 1
            if strong and chord and not in_chord:
                strong_off_chord += 1
                part_strong_off += 1
            if not in_key and not strong and n.duration < 0.35:
                chromatic_pass += 1
            if (not in_key or (strong and chord and not in_chord)) and len(part_examples) < 8:
                row = {
                    "part": part,
                    "bar": bar,
                    "beat": round(beat, 2),
                    "note": pc_name(n.pitch),
                    "pc": PC_NAMES[pc],
                    "chord": " ".join(PC_NAMES[c] for c in sorted(chord)) or "?",
                    "in_key": in_key,
                    "in_chord": in_chord,
                    "strong": strong,
                }
                part_examples.append(row)
                examples.append(row)
        n_notes = len(notes)
        by_part[part] = {
            "n": n_notes,
            "out_of_key_pct": round(100.0 * part_out_key / n_notes, 1),
            "out_of_chord_pct": round(100.0 * part_out_chord / n_notes, 1),
            "strong_off_chord_pct": round(100.0 * part_strong_off / max(part_strong, 1), 1),
            "examples": part_examples,
        }

    n = max(total, 1)
    return {
        "key": f"{PC_NAMES[root % 12]} {scale}",
        "n_notes": total,
        "in_key_pct": round(100.0 * (total - out_key) / n, 1),
        "out_of_key_pct": round(100.0 * out_key / n, 1),
        "out_of_key_not_chord_pct": round(100.0 * out_key_not_chord / n, 1),
        "strong_chord_tone_pct": round(100.0 * (strong_total - strong_off_chord) / max(strong_total, 1), 1),
        "chromatic_passing_pct": round(100.0 * chromatic_pass / n, 1),
        "parts": by_part,
        "examples": examples[:24],
    }
