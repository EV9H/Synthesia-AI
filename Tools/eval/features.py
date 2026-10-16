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
        token = drum_class(n.pitch) if n.is_drum else f"p{n.pitch % 12}"
        cells.append((pos, token))
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
        "onset_events": unique_onset_density(clip, notes),
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


def _beat_seconds(clip: Clip) -> float:
    return 60.0 / max(clip.tempo, 1.0)


def _interval_contour(notes: list[Note], t0: float, t1: float, max_notes: int = 6) -> tuple[int, ...]:
    seq = sorted((n for n in notes if t0 <= n.start < t1), key=lambda n: (n.start, n.pitch))
    if len(seq) < 3:
        return tuple()
    seq = seq[:max_notes]
    return tuple(seq[i].pitch - seq[i - 1].pitch for i in range(1, len(seq)))


def _contour_similarity(a: tuple[int, ...], b: tuple[int, ...]) -> float:
    if not a or not b:
        return 0.0
    n = min(len(a), len(b))
    score = 0.0
    for i in range(n):
        if a[i] == b[i]:
            score += 1.0
        elif a[i] != 0 and b[i] != 0 and (a[i] > 0) == (b[i] > 0):
            score += 0.6
        elif abs(a[i] - b[i]) == 1:
            score += 0.4
    return score / max(len(a), len(b))


def hook_memorability(clip: Clip) -> float:
    """0..1 restatement of a 2-bar melody contour four bars later."""
    melody = clip.of("synth")
    n_bars = int(np.floor(clip.bars()))
    if len(melody) < 6 or n_bars < 4:
        return 0.0
    beat_s = _beat_seconds(clip)
    windows: list[tuple[int, ...]] = []
    for bar in range(0, n_bars - 1, 2):
        contour = _interval_contour(melody, bar * 4 * beat_s, (bar + 2) * 4 * beat_s)
        if contour:
            windows.append(contour)
    if len(windows) < 2:
        return 0.0
    if len(windows) >= 3:
        return float(np.clip(_contour_similarity(windows[0], windows[2]), 0.0, 1.0))
    return float(np.clip(_contour_similarity(windows[0], windows[1]) * 0.7, 0.0, 1.0))


def call_response(clip: Clip) -> float:
    """0..1 complementary bar density between lead and piano (negative correlation)."""
    lead = clip.of("synth")
    piano = clip.of("piano")
    n_bars = int(np.floor(clip.bars()))
    if n_bars < 4 or not lead or not piano:
        return 0.0
    beat_s = _beat_seconds(clip)
    lead_d: list[float] = []
    piano_d: list[float] = []
    for bar in range(n_bars):
        t0 = bar * 4 * beat_s
        t1 = (bar + 1) * 4 * beat_s
        lead_d.append(float(sum(1 for n in lead if t0 <= n.start < t1)))
        piano_d.append(float(sum(1 for n in piano if t0 <= n.start < t1)))
    a = np.array(lead_d, dtype=float)
    b = np.array(piano_d, dtype=float)
    if float(a.std()) < 1e-6 or float(b.std()) < 1e-6:
        return 0.0
    r = float(np.corrcoef(a, b)[0, 1])
    if not np.isfinite(r):
        return 0.0
    return float(np.clip(0.5 - 0.5 * r, 0.0, 1.0))


def section_contrast(clip: Clip) -> float:
    """0..1 relative density change between first and second half (loop A/B or verse/chorus)."""
    n_bars = int(np.floor(clip.bars()))
    if n_bars < 8:
        return 0.0
    beat_s = _beat_seconds(clip)
    half = n_bars // 2
    parts = clip.of("synth") + clip.of("drums") + clip.of("piano")
    if not parts:
        return 0.0

    def dens(b0: int, b1: int) -> float:
        t0 = b0 * 4 * beat_s
        t1 = b1 * 4 * beat_s
        return sum(1 for n in parts if t0 <= n.start < t1) / max(t1 - t0, 1e-6)

    d1, d2 = dens(0, half), dens(half, n_bars)
    rel = abs(d1 - d2) / max(0.5 * (d1 + d2), 1e-6)
    return float(np.clip(rel, 0.0, 1.0))


def bass_contour(clip: Clip) -> float:
    """0..1 fraction of 4-beat windows whose bass starts low and then moves."""
    bass = clip.of("bass")
    if len(bass) < 4:
        return 0.0
    beat_s = _beat_seconds(clip)
    n_beats = int(np.floor(clip.beats()))
    good = 0.0
    seen = 0
    for b in range(0, max(n_beats, 4), 4):
        t0 = b * beat_s
        t1 = (b + 4) * beat_s
        hits = sorted((n for n in bass if t0 <= n.start < t1), key=lambda n: n.start)
        if len(hits) < 2:
            continue
        seen += 1
        lowest = min(n.pitch for n in hits)
        rootish = hits[0].pitch <= lowest + 2
        moved = hits[-1].pitch != hits[0].pitch
        if rootish and moved:
            good += 1.0
        elif moved or rootish:
            good += 0.45
    return good / seen if seen else 0.0


def unique_onset_density(clip: Clip, notes: list[Note]) -> float:
    beats = clip.beats()
    if beats <= 0 or not notes:
        return 0.0
    times = {round(n.start, 3) for n in notes}
    return len(times) / beats


# Genre-loop onset targets (notes/beat). Half-width is the "good" band around the target.
# These are 8-bar DAW-loop priors, not piano-performance refs.
LOOP_DENSITY_TARGET = {
    "pop":        {"drums": (1.7, 0.7), "chords": (0.45, 0.35), "bass": (0.50, 0.28),
                   "synth": (0.90, 0.40), "piano": (1.10, 0.50)},
    "lofi":       {"drums": (1.4, 0.6), "chords": (0.40, 0.30), "bass": (0.65, 0.35),
                   "synth": (0.70, 0.40), "piano": (0.80, 0.45)},
    "house":      {"drums": (2.4, 0.8), "chords": (0.70, 0.45), "bass": (0.70, 0.35),
                   "synth": (0.80, 0.40), "piano": (0.70, 0.40)},
    "trap":       {"drums": (2.2, 0.9), "chords": (0.50, 0.40), "bass": (0.45, 0.28),
                   "synth": (0.90, 0.45), "piano": (1.20, 0.55)},
    "rnb":        {"drums": (1.8, 0.7), "chords": (0.35, 0.28), "bass": (0.55, 0.30),
                   "synth": (0.70, 0.40), "piano": (0.70, 0.40)},
    "cinematic":  {"drums": (1.3, 0.6), "chords": (0.55, 0.40), "bass": (0.30, 0.22),
                   "synth": (0.70, 0.40), "piano": (1.40, 0.60)},
}

GROOVE_TARGET = {
    "pop":        {"kick_density": (0.45, 0.20), "snare_density": (0.45, 0.18), "hat_density": (0.90, 0.50)},
    "lofi":       {"kick_density": (0.35, 0.18), "snare_density": (0.45, 0.18), "hat_density": (0.70, 0.40)},
    "house":      {"kick_density": (1.00, 0.18), "snare_density": (0.50, 0.16), "hat_density": (1.20, 0.70)},
    "trap":       {"kick_density": (0.35, 0.22), "snare_density": (0.28, 0.14), "hat_density": (2.00, 0.90)},
    "rnb":        {"kick_density": (0.45, 0.22), "snare_density": (0.45, 0.18), "hat_density": (1.10, 0.55)},
    "cinematic":  {"kick_density": (0.45, 0.22), "snare_density": (0.40, 0.22), "hat_density": (0.55, 0.35)},
}


def groove_fit_vector(clip: Clip, genre: str) -> dict[str, float]:
    drums = clip.of("drums")
    stats = drum_pattern_stats(clip, drums) if drums else {}
    targets = GROOVE_TARGET.get(genre, GROOVE_TARGET["pop"])
    out: dict[str, float] = {}
    for key, (ideal, half) in targets.items():
        out[key] = float(stats.get(key, 0.0))
        out[f"{key}_target"] = float(ideal)
        out[f"{key}_half"] = float(half)
    return out


def density_fit_parts(clip: Clip, genre: str) -> dict[str, float]:
    targets = LOOP_DENSITY_TARGET.get(genre, LOOP_DENSITY_TARGET["pop"])
    out: dict[str, float] = {}
    for part, (ideal, half) in targets.items():
        notes = clip.of(part)
        dens = unique_onset_density(clip, notes) if part == "chords" else onset_density(clip, notes)
        out[part] = dens
        out[f"{part}_target"] = float(ideal)
        out[f"{part}_half"] = float(half)
    return out


def clip_features(clip: Clip, genre: str = "") -> dict[str, dict[str, float]]:
    _ = genre
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
        "hook_memorability": hook_memorability(clip),
        "call_response": call_response(clip),
        "section_contrast": section_contrast(clip),
        "bass_contour": bass_contour(clip),
    }
    return out
