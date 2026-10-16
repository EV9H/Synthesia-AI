"""Compare clip features to reference distributions -> 0-100 pro-likeness."""
from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path
from typing import Any

import numpy as np

from download_refs import ensure_refs
from features import GROOVE_TARGET, LOOP_DENSITY_TARGET, clip_features
from midi_parse import load_midi
from paths import GENRES, PARTS, refs_dir

# q10 loop-oriented scoring. Headline overall mixes reweighted ref percentiles
# with arrangement targets so 8-bar in-key loops are not punished for missing
# piano-performance chord vocabulary. Hard lint gates are unchanged.
METRIC_VERSION = "loop-v1"
ARRANGEMENT_BLEND = 0.22

# Piano-performance refs structurally cap 8-bar loops on these keys.
KEY_WEIGHTS = {
    "chord_vocab": 0.25,
    "transition_entropy": 0.25,
}

# Features where being near the reference median is "pro".
COMPARE_KEYS = {
    "drums": (
        "onset_density", "syncopation", "swing", "microtiming_ms", "vel_std",
        "kick_density", "snare_density", "hat_density", "hat_ioi_cv",
    ),
    "chords": (
        "onset_density", "chord_vocab", "transition_entropy", "voice_leading",
        "clash_rate", "register_mean", "variation_8",
    ),
    "bass": (
        "onset_density", "register_mean", "kick_align", "root_conflict",
        "swing", "vel_std",
    ),
    "synth": (
        "onset_density", "syncopation", "register_mean", "repeat_4",
        "variation_8", "vel_std",
    ),
    "piano": (
        "onset_density", "register_mean", "variation_8", "vel_std",
        "clash_rate",
    ),
}

# Higher than ref is worse (penalize the high tail more).
LOWER_IS_BETTER = {"clash_rate", "root_conflict", "microtiming_ms", "repeat_4"}
HIGHER_IS_BETTER = {"kick_align", "variation_8", "transition_entropy", "chord_vocab", "hat_ioi_cv"}


def _percentile_score(value: float, ref: np.ndarray, key: str) -> float:
    ref = ref[np.isfinite(ref)]
    if ref.size < 3 or not np.isfinite(value):
        return 55.0
    p10, p25, p50, p75, p90 = np.percentile(ref, [10, 25, 50, 75, 90])
    iqr = max(p75 - p25, 1e-6)
    if p10 <= value <= p90:
        # inside typical range: 80-100, best at median
        dist = abs(value - p50) / (max(p90 - p10, iqr) * 0.5 + 1e-6)
        return float(np.clip(100.0 - 20.0 * dist, 80.0, 100.0))
    if key in LOWER_IS_BETTER:
        if value < p10:
            return float(np.clip(80.0 - (p10 - value) / iqr * 15.0, 40.0, 85.0))
        return float(np.clip(70.0 - (value - p90) / iqr * 25.0, 5.0, 75.0))
    if key in HIGHER_IS_BETTER:
        if value > p90:
            return float(np.clip(85.0 + min(10.0, (value - p90) / iqr * 5.0), 85.0, 95.0))
        return float(np.clip(70.0 - (p10 - value) / iqr * 25.0, 5.0, 75.0))
    # two-sided: far from the box is worse
    if value < p10:
        return float(np.clip(75.0 - (p10 - value) / iqr * 20.0, 8.0, 80.0))
    return float(np.clip(75.0 - (value - p90) / iqr * 20.0, 8.0, 80.0))


def _load_index() -> dict[str, Any]:
    path = refs_dir() / "index.json"
    if not path.exists():
        return ensure_refs()
    return json.loads(path.read_text())


def build_reference_stats(force: bool = False) -> dict[str, Any]:
    stats_path = refs_dir() / "stats.json"
    if stats_path.exists() and not force:
        return json.loads(stats_path.read_text())
    index = _load_index()
    files = index.get("files", {})
    buckets: dict[str, dict[str, dict[str, list[float]]]] = {
        g: {p: defaultdict(list) for p in PARTS} for g in GENRES
    }

    def accumulate(path: str, genre: str, prefer_drums: bool) -> None:
        clip = load_midi(path, generated=False)
        if clip is None:
            return
        feats = clip_features(clip)
        for part, vec in feats.items():
            if part.startswith("_"):
                continue
            if prefer_drums and part != "drums":
                continue
            if not prefer_drums and part == "drums":
                continue
            for k, v in vec.items():
                if isinstance(v, (int, float)) and np.isfinite(v):
                    buckets[genre][part][k].append(float(v))
                    # Piano performances also stand in for chord/melody refs.
                    if part == "piano":
                        buckets[genre]["chords"][k].append(float(v))
                        buckets[genre]["synth"][k].append(float(v))

    for genre in GENRES:
        for path in files.get(genre, {}).get("drums", []):
            accumulate(path, genre, prefer_drums=True)
        for path in files.get(genre, {}).get("harmony", []):
            accumulate(path, genre, prefer_drums=False)

    stats: dict[str, Any] = {"by_genre": {}, "global": {}}
    for genre in GENRES:
        stats["by_genre"][genre] = {}
        for part in PARTS:
            stats["by_genre"][genre][part] = {
                k: vals for k, vals in buckets[genre][part].items() if len(vals) >= 3
            }

    # Global fallback when a genre/part is thin.
    global_parts: dict[str, dict[str, list[float]]] = {p: defaultdict(list) for p in PARTS}
    for genre in GENRES:
        for part in PARTS:
            for k, vals in stats["by_genre"][genre][part].items():
                global_parts[part][k].extend(vals)
    stats["global"] = {p: dict(d) for p, d in global_parts.items()}
    stats["n_refs"] = {
        g: {
            "drums": len(files.get(g, {}).get("drums", [])),
            "harmony": len(files.get(g, {}).get("harmony", [])),
        }
        for g in GENRES
    }
    stats_path.write_text(json.dumps(stats))
    return stats


def _ref_array(stats: dict[str, Any], genre: str, part: str, key: str) -> np.ndarray:
    def grab(g: str | None, p: str) -> list | None:
        src = stats.get("by_genre", {}).get(g or "", {}).get(p, {}) if g else stats.get("global", {}).get(p, {})
        if not src:
            src = stats.get("global", {}).get(p, {})
        return src.get(key)

    vals = grab(genre, part)
    if not vals and part in {"chords", "synth"}:
        vals = grab(genre, "piano")
    if not vals:
        vals = grab(None, part)
    if not vals and part in {"chords", "synth"}:
        vals = grab(None, "piano")
    if not vals:
        return np.zeros(0)
    return np.asarray(vals, dtype=float)


def _band_score(value: float, lo: float, hi: float, ideal: float | None = None) -> float:
    """100 near ideal (or mid-band), ~82 at the edges, fall off outside. Loop targets, not refs."""
    if not np.isfinite(value):
        return 55.0
    mid = float(ideal if ideal is not None else 0.5 * (lo + hi))
    span = max(hi - lo, 1e-6)
    if lo <= value <= hi:
        dist = abs(value - mid) / (span * 0.5)
        return float(np.clip(100.0 - 18.0 * dist, 82.0, 100.0))
    if value < lo:
        return float(np.clip(80.0 - (lo - value) / span * 40.0, 18.0, 80.0))
    return float(np.clip(80.0 - (value - hi) / span * 40.0, 18.0, 80.0))


def _key_weight(key: str) -> float:
    return float(KEY_WEIGHTS.get(key, 1.0))


def score_part(feats: dict[str, float], genre: str, part: str, stats: dict[str, Any]) -> dict[str, Any]:
    keys = COMPARE_KEYS.get(part, ("onset_density",))
    per = {}
    weighted: list[tuple[float, float]] = []
    equal: list[float] = []
    for key in keys:
        if key not in feats:
            continue
        ref = _ref_array(stats, genre, part, key)
        s = _percentile_score(float(feats[key]), ref, key)
        per[key] = round(s, 1)
        w = _key_weight(key)
        weighted.append((s, w))
        equal.append(s)
    overall = float(np.average([s for s, w in weighted], weights=[w for s, w in weighted])) if weighted else 50.0
    legacy = float(np.mean(equal)) if equal else 50.0
    return {
        "score": round(overall, 1),
        "legacy_score": round(legacy, 1),
        "features": {k: round(float(feats[k]), 4) for k in feats if isinstance(feats[k], (int, float))},
        "vs_ref": per,
    }


def score_arrangement(feats: dict[str, dict[str, float]], genre: str) -> dict[str, Any]:
    """Target-band scores for loop craft. Does not require out-of-key notes."""
    clip = feats.get("_clip", {})
    drums = feats.get("drums", {})

    hook = float(clip.get("hook_memorability", 0.0))
    call = float(clip.get("call_response", 0.0))
    contrast = float(clip.get("section_contrast", 0.0))
    bass_c = float(clip.get("bass_contour", 0.0))

    per = {
        "hook_memorability": round(_band_score(hook, 0.35, 0.88, 0.62), 1),
        "call_response": round(_band_score(call, 0.48, 0.85, 0.62), 1),
        "section_contrast": round(_band_score(contrast, 0.10, 0.48, 0.24), 1),
        "bass_contour": round(_band_score(bass_c, 0.40, 0.95, 0.72), 1),
    }

    dens_targets = LOOP_DENSITY_TARGET.get(genre, LOOP_DENSITY_TARGET["pop"])
    dens_scores = []
    for part, (ideal, half) in dens_targets.items():
        notes_feat = feats.get(part, {})
        raw = float(notes_feat.get("onset_events", notes_feat.get("onset_density", 0.0)))
        s = _band_score(raw, max(0.0, ideal - half), ideal + half, ideal)
        per[f"density_{part}"] = round(s, 1)
        dens_scores.append(s)
    per["density_fit"] = round(float(np.mean(dens_scores)) if dens_scores else 55.0, 1)

    groove_t = GROOVE_TARGET.get(genre, GROOVE_TARGET["pop"])
    groove_scores = []
    for key, (ideal, half) in groove_t.items():
        raw = float(drums.get(key, 0.0))
        s = _band_score(raw, max(0.0, ideal - half), ideal + half, ideal)
        per[f"groove_{key}"] = round(s, 1)
        groove_scores.append(s)
    per["groove_fit"] = round(float(np.mean(groove_scores)) if groove_scores else 55.0, 1)

    headline_keys = ("hook_memorability", "call_response", "section_contrast",
                     "bass_contour", "density_fit", "groove_fit")
    overall = float(np.mean([per[k] for k in headline_keys]))
    return {
        "score": round(overall, 1),
        "features": {
            "hook_memorability": round(hook, 4),
            "call_response": round(call, 4),
            "section_contrast": round(contrast, 4),
            "bass_contour": round(bass_c, 4),
        },
        "vs_target": per,
    }


def lint_penalty(issues: list[dict[str, Any]]) -> float:
    pen = 0.0
    for issue in issues:
        pen += 8.0 if issue.get("severity") == "error" else 3.0
    return min(pen, 28.0)


def score_clip(feats: dict[str, dict[str, float]], genre: str, stats: dict[str, Any],
               issues: list[dict[str, Any]]) -> dict[str, Any]:
    parts = {}
    part_scores = []
    legacy_parts = []
    for part in PARTS:
        if part not in feats:
            continue
        parts[part] = score_part(feats[part], genre, part, stats)
        part_scores.append(parts[part]["score"])
        legacy_parts.append(parts[part].get("legacy_score", parts[part]["score"]))
    raw = float(np.mean(part_scores)) if part_scores else 50.0
    legacy_raw = float(np.mean(legacy_parts)) if legacy_parts else 50.0
    arrangement = score_arrangement(feats, genre)
    blended = (1.0 - ARRANGEMENT_BLEND) * raw + ARRANGEMENT_BLEND * arrangement["score"]
    penalty = lint_penalty(issues)
    pro = float(np.clip(blended - penalty, 0.0, 100.0))
    legacy_pro = float(np.clip(legacy_raw - penalty, 0.0, 100.0))
    return {
        "pro_likeness": round(pro, 1),
        "legacy_pro_likeness": round(legacy_pro, 1),
        "raw": round(blended, 1),
        "ref_raw": round(raw, 1),
        "lint_penalty": round(penalty, 1),
        "metric_version": METRIC_VERSION,
        "arrangement": arrangement,
        "parts": parts,
    }
