"""Per-part MIDI descriptors used for pro-likeness scoring."""
from __future__ import annotations

from collections import Counter

import numpy as np

from midi_parse import Clip, Note, drum_class
from paths import PARTS


def _onsets(notes: list[Note]) -> np.ndarray:
    if not notes:
        return np.zeros(0)
    return np.array([n.start for n in notes], dtype=float)


def _beats(clip: Clip, times: np.ndarray) -> np.ndarray:
    return times * clip.tempo / 60.0


def onset_density(clip: Clip, notes: list[Note]) -> float:
    beats = clip.beats()
    if beats <= 0:
        return 0.0
    return len(notes) / beats


def syncopation(clip: Clip, notes: list[Note]) -> float:
    """Share of onsets on 16th off-beats, weighted toward weaker 16ths."""
    if not notes:
        return 0.0
    beats = _beats(clip, _onsets(notes))
    grid = np.round(beats * 4) / 4.0  # 16th
    frac = np.abs(beats - np.floor(beats))
    # positions 0, .25, .5, .75; off-beat 16ths are .25 and .75
    sixteenth = np.round(frac * 4) % 4
    weights = np.where(sixteenth == 0, 0.0, np.where(sixteenth == 2, 0.5, 1.0))
    off = np.abs(beats - grid) < 0.08
    if not np.any(off):
        return float(np.mean(weights))
    return float(np.mean(weights[off]))


def swing_ratio(clip: Clip, notes: list[Note]) -> float:
    """0.5 = straight 8ths, ~0.66 = triplet swing. Empty -> 0.5."""
    if len(notes) < 4:
        return 0.5
    beats = np.sort(_beats(clip, _onsets(notes)))
    # 8th-note pairs inside each beat
    offsets = []
    for b in beats:
        frac = b - np.floor(b)
        if 0.15 < frac < 0.85:
            offsets.append(frac)
    if len(offsets) < 3:
        return 0.5
    return float(np.clip(np.median(offsets), 0.35, 0.75))


def microtiming_ms(clip: Clip, notes: list[Note]) -> float:
    if not notes:
        return 0.0
    beats = _beats(clip, _onsets(notes))
    grid = np.round(beats * 4) / 4.0
    err_beats = np.abs(beats - grid)
    ms = err_beats * (60000.0 / max(clip.tempo, 1.0))
    return float(np.median(ms))


def velocity_stats(notes: list[Note]) -> tuple[float, float]:
    if not notes:
        return 0.0, 0.0
    v = np.array([n.velocity for n in notes], dtype=float)
    return float(np.mean(v)), float(np.std(v))


def register(notes: list[Note]) -> tuple[float, float, float]:
    if not notes:
        return 0.0, 0.0, 0.0
    p = np.array([n.pitch for n in notes], dtype=float)
    return float(np.min(p)), float(np.max(p)), float(np.mean(p))


def drum_pattern_stats(clip: Clip, notes: list[Note]) -> dict[str, float]:
    kicks = [n for n in notes if drum_class(n.pitch) == "kick"]
    snares = [n for n in notes if drum_class(n.pitch) == "snare"]
    hats = [n for n in notes if drum_class(n.pitch) == "hat"]
    beats = max(clip.beats(), 1e-6)
    return {
        "kick_density": len(kicks) / beats,
        "snare_density": len(snares) / beats,
        "hat_density": len(hats) / beats,
        "kick_sync": syncopation(clip, kicks),
        "hat_ioi_cv": _ioi_cv(hats),
        "hat_vel_std": velocity_stats(hats)[1],
    }


def _ioi_cv(notes: list[Note]) -> float:
    if len(notes) < 4:
        return 0.0
    t = np.sort(_onsets(notes))
    ioi = np.diff(t)
    ioi = ioi[ioi > 1e-4]
    if ioi.size < 3 or float(np.mean(ioi)) <= 0:
        return 0.0
    return float(np.std(ioi) / np.mean(ioi))


def _beat_chroma(clip: Clip, notes: list[Note]) -> list[frozenset[int]]:
    if not notes:
        return []
    n_beats = int(np.ceil(clip.beats()))
    buckets: list[set[int]] = [set() for _ in range(max(n_beats, 1))]
    for n in notes:
        b0 = int(np.floor(clip.time_to_beat(n.start)))
        b1 = int(np.floor(clip.time_to_beat(max(n.start, n.end - 1e-4))))
        for b in range(max(0, b0), min(len(buckets), b1 + 1)):
            buckets[b].add(n.pitch % 12)
    return [frozenset(s) for s in buckets if len(s) >= 3]


def chord_vocab_entropy(clip: Clip, notes: list[Note]) -> tuple[int, float]:
    frames = _beat_chroma(clip, notes)
    if len(frames) < 2:
        return len(set(frames)), 0.0
    trans = Counter(zip(frames, frames[1:]))
    total = sum(trans.values())
    probs = np.array([c / total for c in trans.values()], dtype=float)
    ent = float(-(probs * np.log2(np.clip(probs, 1e-12, 1))).sum())
    return len(set(frames)), ent


def voice_leading_distance(clip: Clip, notes: list[Note]) -> float:
    # One voicing per beat: sorted sounding pitches at beat center.
    if not notes:
        return 0.0
    n_beats = int(np.floor(clip.beats()))
    voicings: list[list[int]] = []
    for b in range(max(n_beats, 0)):
        t = (b + 0.5) * 60.0 / clip.tempo
        sounding = sorted(n.pitch for n in notes if n.start <= t < n.end)
        if len(sounding) >= 2:
            voicings.append(sounding)
    if len(voicings) < 2:
        return 0.0
    dists = []
    for a, b in zip(voicings, voicings[1:]):
        n = min(len(a), len(b))
        dists.append(float(np.mean(np.abs(np.array(a[:n]) - np.array(b[:n])))))
    return float(np.mean(dists)) if dists else 0.0


def clash_rate(clip: Clip, notes: list[Note]) -> float:
    if len(notes) < 2:
        return 0.0
    clashes = 0
    checks = 0
    n_beats = int(np.floor(clip.beats()))
    for b in range(max(n_beats, 0)):
        t = (b + 0.25) * 60.0 / clip.tempo
        pcs = [n.pitch for n in notes if n.start <= t < n.end]
        if len(pcs) < 2:
            continue
        checks += 1
        for i, p in enumerate(pcs):
            for q in pcs[i + 1 :]:
                if abs(p - q) % 12 == 1 and abs(p - q) != 0:
                    clashes += 1
                    break
    return clashes / checks if checks else 0.0


def bass_kick_alignment(clip: Clip) -> float:
    bass = clip.of("bass")
    kicks = [n for n in clip.of("drums") if drum_class(n.pitch) == "kick"]
    if not bass or not kicks:
        return 0.0
    bt = _onsets(bass)
    kt = _onsets(kicks)
    thresh = 0.06  # 60 ms
    hits = 0
    for t in kt:
        if np.min(np.abs(bt - t)) <= thresh:
            hits += 1
    return hits / len(kicks)


def bass_root_conflict(clip: Clip) -> float:
    bass = clip.of("bass")
    chords = clip.of("chords") or clip.of("piano")
    frames = _beat_chroma(clip, chords)
    if not bass or not frames:
        return 0.0
    n_beats = min(int(np.floor(clip.beats())), len(frames))
    bad = 0
    seen = 0
    for b in range(n_beats):
        chroma = frames[b]
        if len(chroma) < 3:
            continue
        t0 = b * 60.0 / clip.tempo
        t1 = (b + 1) * 60.0 / clip.tempo
        roots = [n.pitch % 12 for n in bass if n.start < t1 and n.end > t0]
        if not roots:
            continue
        seen += 1
        # conflict: bass pc is neither a chord tone
        if all(pc not in chroma for pc in roots):
            bad += 1
    return bad / seen if seen else 0.0


def _bar_hash(clip: Clip, notes: list[Note], bar: int) -> tuple:
    beat_per_bar = clip.ts_num * 4 / max(1, clip.ts_den)
    t0 = bar * beat_per_bar * 60.0 / clip.tempo
    t1 = (bar + 1) * beat_per_bar * 60.0 / clip.tempo
    cells = []
    for n in notes:
        if n.end <= t0 or n.start >= t1:
            continue
        pos = int(np.round((n.start - t0) / max(t1 - t0, 1e-6) * 16))
        cells.append((pos, n.pitch % 12 if not n.is_drum else drum_class(n.pitch)))
    return tuple(sorted(cells))


def repetition_scores(clip: Clip, notes: list[Note]) -> tuple[float, float]:
    """Return (repeat_4, variation_8) in 0..1. High repeat_4 = looping 4-bar cell."""
    n_bars = int(np.floor(clip.bars()))
    if n_bars < 4 or not notes:
        return 0.0, 1.0
    hashes = [_bar_hash(clip, notes, b) for b in range(n_bars)]
    same4 = 0
    tot4 = 0
    for i in range(n_bars - 4):
        tot4 += 1
        if hashes[i] == hashes[i + 4]:
            same4 += 1
    repeat_4 = same4 / tot4 if tot4 else 0.0
    unique = len(set(h for h in hashes if h))
    variation_8 = unique / max(n_bars, 1)
    return float(repeat_4), float(variation_8)


def part_features(clip: Clip, part: str) -> dict[str, float]:
    notes = clip.of(part)
    dens = onset_density(clip, notes)
    sync = syncopation(clip, notes)
    swing = swing_ratio(clip, notes)
    micro = microtiming_ms(clip, notes)
    vmean, vstd = velocity_stats(notes)
    lo, hi, mean_p = register(notes)
    rep4, var8 = repetition_scores(clip, notes)
    feats: dict[str, float] = {
        "onset_density": dens,
        "syncopation": sync,
        "swing": swing,
        "microtiming_ms": micro,
        "vel_mean": vmean,
        "vel_std": vstd,
        "register_low": lo,
        "register_high": hi,
        "register_mean": mean_p,
        "repeat_4": rep4,
        "variation_8": var8,
        "n_notes": float(len(notes)),
    }
    if part == "drums":
        feats.update(drum_pattern_stats(clip, notes))
    if part in {"chords", "piano"}:
        vocab, ent = chord_vocab_entropy(clip, notes)
        feats["chord_vocab"] = float(vocab)
        feats["transition_entropy"] = ent
        feats["voice_leading"] = voice_leading_distance(clip, notes)
        feats["clash_rate"] = clash_rate(clip, notes)
    if part == "bass":
        feats["kick_align"] = bass_kick_alignment(clip)
        feats["root_conflict"] = bass_root_conflict(clip)
    if part == "synth":
        feats["clash_rate"] = clash_rate(clip, notes + clip.of("chords"))
    return feats


def clip_features(clip: Clip) -> dict[str, dict[str, float]]:
    out = {}
    for part in PARTS:
        feats = part_features(clip, part)
        if feats.get("n_notes", 0) > 0:
            out[part] = feats
    out["_clip"] = {
        "tempo": clip.tempo,
        "duration": clip.duration,
        "beats": clip.beats(),
        "bars": clip.bars(),
        "kick_align": bass_kick_alignment(clip),
        "root_conflict": bass_root_conflict(clip),
        "clash_rate": clash_rate(clip, [n for n in clip.notes if not n.is_drum]),
    }
    return out
