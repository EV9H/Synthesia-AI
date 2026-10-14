"""Load MIDI into a part-bucketed note list (seconds + beats)."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import pretty_midi as pm

from paths import GM_HAT, GM_KICK, GM_SNARE


@dataclass
class Note:
    start: float
    end: float
    pitch: int
    velocity: int
    part: str
    is_drum: bool = False

    @property
    def duration(self) -> float:
        return max(0.0, self.end - self.start)


@dataclass
class Clip:
    path: str
    tempo: float
    duration: float
    notes: list[Note] = field(default_factory=list)
    ts_num: int = 4
    ts_den: int = 4

    def beats(self) -> float:
        if self.tempo <= 1:
            return 0.0
        return self.duration * self.tempo / 60.0

    def bars(self) -> float:
        beat_per_bar = self.ts_num * 4 / max(1, self.ts_den)
        b = self.beats()
        return b / beat_per_bar if beat_per_bar else 0.0

    def of(self, part: str) -> list[Note]:
        return [n for n in self.notes if n.part == part]

    def time_to_beat(self, t: float) -> float:
        return t * self.tempo / 60.0


def _tempo(midi: pm.PrettyMIDI) -> float:
    try:
        times, tempi = midi.get_tempo_changes()
        if len(tempi):
            return float(tempi[0])
    except Exception:
        pass
    return 120.0


def _classify_instrument(inst: pm.Instrument, generated_layout: bool) -> str:
    name = (inst.name or "").strip().lower()
    if inst.is_drum or "drum" in name or "kit" in name:
        return "drums"
    if generated_layout:
        mapping = {
            "chords": "chords",
            "bass": "bass",
            "synth": "synth",
            "melody": "synth",
            "piano": "piano",
            "drums": "drums",
        }
        for key, part in mapping.items():
            if key in name:
                return part
    prog = int(getattr(inst, "program", 0) or 0)
    if 32 <= prog <= 39:
        return "bass"
    if 0 <= prog <= 7 or 16 <= prog <= 23:
        # Polyphonic piano/keys -> chords if stacked, else piano. Decided later.
        return "piano"
    if 80 <= prog <= 103 or 40 <= prog <= 51:
        return "synth"
    if 88 <= prog <= 95:
        return "chords"
    return "synth"


def _promote_piano_chords(notes: list[Note]) -> None:
    piano = [n for n in notes if n.part == "piano"]
    if len(piano) < 8:
        return
    # If many simultaneous attacks, treat as chords.
    starts = np.round([n.start for n in piano], 2)
    if starts.size == 0:
        return
    _, counts = np.unique(starts, return_counts=True)
    if float(np.mean(counts >= 3)) > 0.25:
        for n in piano:
            n.part = "chords"


def load_midi(path: str | Path, generated: bool = False) -> Clip | None:
    path = Path(path)
    try:
        midi = pm.PrettyMIDI(str(path))
    except Exception as exc:
        print(f"  skip {path.name}: {exc}")
        return None
    tempo = _tempo(midi)
    duration = float(midi.get_end_time() or 0.0)
    ts_num, ts_den = 4, 4
    try:
        if midi.time_signature_changes:
            ts_num = int(midi.time_signature_changes[0].numerator)
            ts_den = int(midi.time_signature_changes[0].denominator)
    except Exception:
        pass

    notes: list[Note] = []
    generated_layout = generated or any(
        (inst.name or "").lower() in {"chords", "bass", "synth", "piano", "drums", "melody"}
        for inst in midi.instruments
    )
    for inst in midi.instruments:
        part = _classify_instrument(inst, generated_layout)
        is_drum = part == "drums" or inst.is_drum
        if is_drum:
            part = "drums"
        for n in inst.notes:
            if n.end <= n.start:
                continue
            notes.append(
                Note(
                    start=float(n.start),
                    end=float(n.end),
                    pitch=int(n.pitch),
                    velocity=int(n.velocity),
                    part=part,
                    is_drum=is_drum,
                )
            )
    if not generated_layout:
        _promote_piano_chords(notes)
        # Split low piano to bass if no bass track.
        has_bass = any(n.part == "bass" for n in notes)
        if not has_bass:
            for n in notes:
                if n.part in {"piano", "chords"} and n.pitch <= 48:
                    n.part = "bass"
    if not notes:
        return None
    return Clip(path=str(path), tempo=tempo, duration=duration, notes=notes, ts_num=ts_num, ts_den=ts_den)


def drum_class(pitch: int) -> str:
    if pitch in GM_KICK:
        return "kick"
    if pitch in GM_SNARE:
        return "snare"
    if pitch in GM_HAT:
        return "hat"
    return "other"
