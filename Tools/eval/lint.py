"""Hard MIDI lint rules (collisions, range, busy drums, machine-gun hats...)."""
from __future__ import annotations

from typing import Any

import numpy as np

from harmony import analyze_harmony
from midi_parse import Clip, drum_class
from paths import REGISTER_LIMITS
from features import _ioi_cv, bass_root_conflict, onset_density, repetition_scores

# Hard product gates: pitched notes stay in the key (or on a borrowed chord tone).
IN_KEY_MAX_OUT_PCT = 3.0
STRONG_CHORD_TONE_MIN_PCT = 80.0
CHROMATIC_PASSING_MAX_PCT = 3.5


def _overlap(a_start: float, a_end: float, b_start: float, b_end: float) -> bool:
    return a_start < b_end - 0.02 and b_start < a_end - 0.02


def lint_clip(clip: Clip) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []

    def add(rule: str, severity: str, detail: str, part: str = "") -> None:
        issues.append({"rule": rule, "severity": severity, "detail": detail, "part": part})

    # Part collisions: same pitch overlapping on two non-drum parts.
    pitched = [n for n in clip.notes if not n.is_drum]
    collisions = 0
    for i, a in enumerate(pitched):
        for b in pitched[i + 1 :]:
            if a.part == b.part:
                continue
            # Piano doubling the chord stab is expected, not a collision.
            if {a.part, b.part} == {"piano", "chords"}:
                continue
            if a.pitch == b.pitch and _overlap(a.start, a.end, b.start, b.end):
                collisions += 1
                if collisions >= 12:
                    break
        if collisions >= 12:
            break
    if collisions >= 3:
        add("part_collisions", "warn" if collisions < 8 else "error",
            f"{collisions} overlapping unisons across parts")

    conflict = bass_root_conflict(clip)
    if conflict >= 0.35:
        add("bass_root_conflict", "error" if conflict >= 0.55 else "warn",
            f"bass is outside the chord {conflict:.0%} of beats", "bass")

    drums = clip.of("drums")
    dens = onset_density(clip, drums)
    hats = [n for n in drums if drum_class(n.pitch) == "hat"]
    if dens > 6.5:
        add("overly_busy_drums", "error" if dens > 8 else "warn",
            f"drum onset density {dens:.1f} notes/beat", "drums")
    hat_dens = onset_density(clip, hats)
    if hat_dens > 4.5:
        add("overly_busy_drums", "warn", f"hat density {hat_dens:.1f} notes/beat", "drums")

    if len(hats) >= 8:
        cv = _ioi_cv(hats)
        vel = np.array([n.velocity for n in hats], dtype=float)
        if cv < 0.045 and float(np.std(vel)) < 2.5:
            add("machine_gun_hats", "error",
                f"hats nearly identical IOI (cv={cv:.3f}) and velocity", "drums")

    # Section variation: 8-bar identity of drums+bass.
    combo = drums + clip.of("bass")
    rep4, var8 = repetition_scores(clip, combo if combo else clip.notes)
    if clip.bars() >= 7.5 and var8 < 0.2 and rep4 > 0.85:
        add("no_section_variation", "warn",
            f"bars look copy-pasted (repeat_4={rep4:.2f}, unique_bar_frac={var8:.2f})")

    for part, (lo, hi) in REGISTER_LIMITS.items():
        notes = clip.of(part)
        if not notes:
            continue
        bad = [n for n in notes if n.pitch < lo or n.pitch > hi]
        if not bad:
            continue
        frac = len(bad) / len(notes)
        if frac >= 0.08:
            add("out_of_range_notes", "error" if frac >= 0.2 else "warn",
                f"{len(bad)} notes outside {lo}-{hi} ({frac:.0%})", part)

    harm = analyze_harmony(clip)
    if harm["out_of_key_not_chord_pct"] > IN_KEY_MAX_OUT_PCT:
        add("out_of_key", "error",
            f"{harm['out_of_key_not_chord_pct']:.1f}% pitched notes outside key and current chord "
            f"(in-key {harm['in_key_pct']:.1f}%)")
    if harm["strong_chord_tone_pct"] < STRONG_CHORD_TONE_MIN_PCT:
        add("strong_beat_not_chord_tone", "error",
            f"strong-beat chord-tone {harm['strong_chord_tone_pct']:.1f}% "
            f"(min {STRONG_CHORD_TONE_MIN_PCT:.0f}%)")
    if harm["chromatic_passing_pct"] > CHROMATIC_PASSING_MAX_PCT:
        add("chromatic_passing", "error",
            f"chromatic passing {harm['chromatic_passing_pct']:.1f}% "
            f"(max {CHROMATIC_PASSING_MAX_PCT:.1f}%)")

    return issues
