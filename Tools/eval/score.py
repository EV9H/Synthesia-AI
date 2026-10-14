"""Compare clip features to reference distributions -> 0-100 pro-likeness."""
from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path
from typing import Any

import numpy as np

from download_refs import ensure_refs
from features import clip_features
from midi_parse import load_midi
from paths import GENRES, PARTS, refs_dir

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
    vals = stats.get("by_genre", {}).get(genre, {}).get(part, {}).get(key)
    if not vals:
        vals = stats.get("global", {}).get(part, {}).get(key)
    if not vals:
        return np.zeros(0)
    return np.asarray(vals, dtype=float)


def score_part(feats: dict[str, float], genre: str, part: str, stats: dict[str, Any]) -> dict[str, Any]:
    keys = COMPARE_KEYS.get(part, ("onset_density",))
    per = {}
    scores = []
    for key in keys:
        if key not in feats:
            continue
        ref = _ref_array(stats, genre, part, key)
        s = _percentile_score(float(feats[key]), ref, key)
        per[key] = round(s, 1)
        scores.append(s)
    overall = float(np.mean(scores)) if scores else 50.0
    return {"score": round(overall, 1), "features": {k: round(float(feats[k]), 4) for k in feats}, "vs_ref": per}


def lint_penalty(issues: list[dict[str, Any]]) -> float:
    pen = 0.0
    for issue in issues:
        pen += 8.0 if issue.get("severity") == "error" else 3.0
    return min(pen, 28.0)


def score_clip(feats: dict[str, dict[str, float]], genre: str, stats: dict[str, Any],
               issues: list[dict[str, Any]]) -> dict[str, Any]:
    parts = {}
    part_scores = []
    for part in PARTS:
        if part not in feats:
            continue
        parts[part] = score_part(feats[part], genre, part, stats)
        part_scores.append(parts[part]["score"])
    raw = float(np.mean(part_scores)) if part_scores else 50.0
    penalty = lint_penalty(issues)
    pro = float(np.clip(raw - penalty, 0.0, 100.0))
    return {
        "pro_likeness": round(pro, 1),
        "raw": round(raw, 1),
        "lint_penalty": round(penalty, 1),
        "parts": parts,
    }
