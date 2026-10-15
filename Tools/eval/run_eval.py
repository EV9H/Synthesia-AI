#!/usr/bin/env python3
"""Score generated clips against public MIDI refs, lint, optional Groq critic."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from critic import critic_clip  # noqa: E402
from download_refs import ensure_refs  # noqa: E402
from features import clip_features  # noqa: E402
from harmony import analyze_harmony  # noqa: E402
from lint import lint_clip  # noqa: E402
from midi_parse import load_midi  # noqa: E402
from paths import GENRES, PARTS, repo_root  # noqa: E402
from report import write_reports  # noqa: E402
from score import build_reference_stats, score_clip  # noqa: E402


def _git_sha() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=repo_root(), text=True
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def _mean(xs: list[float]) -> float:
    return float(np.mean(xs)) if xs else 0.0


def evaluate_dir(clips_dir: Path, report_dir: Path, pluginval: dict, steinberg: dict,
                 mp3s: list[str], critic: bool, pitch_check: dict | None = None) -> dict:
    ensure_refs()
    stats = build_reference_stats()
    midis = sorted(clips_dir.glob("*.mid"))
    # Ignore per-part split files if present (name-chords.mid).
    midis = [m for m in midis if "-" not in m.stem.split("_")[-1] or m.stem.count("_") >= 1]
    # Prefer names like pop_s7.mid
    clips = []
    all_issues = []
    critic_rows = []
    by_genre_acc: dict[str, dict] = {g: {"scores": [], "penalties": [], "parts": {p: [] for p in PARTS}} for g in GENRES}

    for path in midis:
        stem = path.stem
        genre = stem.split("_")[0]
        if genre not in GENRES:
            # allow lofi-am-... fallback
            genre = next((g for g in GENRES if stem.startswith(g)), "")
        if genre not in GENRES:
            continue
        clip = load_midi(path, generated=True)
        if clip is None:
            continue
        feats = clip_features(clip)
        issues = lint_clip(clip)
        scored = score_clip(feats, genre, stats, issues)
        harmony = analyze_harmony(clip)
        for issue in issues:
            issue = dict(issue)
            issue["clip"] = path.name
            all_issues.append(issue)
        row = {
            "file": path.name,
            "genre": genre,
            **scored,
            "lint": issues,
            "harmony": {
                "in_key_pct": harmony["in_key_pct"],
                "out_of_key_not_chord_pct": harmony["out_of_key_not_chord_pct"],
                "strong_chord_tone_pct": harmony["strong_chord_tone_pct"],
                "chromatic_passing_pct": harmony["chromatic_passing_pct"],
                "parts": {
                    p: {
                        "n": st["n"],
                        "out_of_key_pct": st["out_of_key_pct"],
                        "out_of_chord_pct": st["out_of_chord_pct"],
                        "strong_off_chord_pct": st["strong_off_chord_pct"],
                    }
                    for p, st in harmony["parts"].items()
                },
            },
        }
        clips.append(row)
        by_genre_acc[genre]["scores"].append(scored["pro_likeness"])
        by_genre_acc[genre]["penalties"].append(scored["lint_penalty"])
        for part in PARTS:
            if part in scored["parts"]:
                by_genre_acc[genre]["parts"][part].append(scored["parts"][part]["score"])
        if critic:
            c = critic_clip(clip, genre)
            if c and not c.get("skipped"):
                critic_rows.append({"clip": path.name, **c})
            elif c and c.get("skipped"):
                critic_rows.append({"clip": path.name, "skipped": True, "reason": c.get("reason")})
            time.sleep(0.7)

    by_genre = {}
    overall_scores = []
    for genre in GENRES:
        acc = by_genre_acc[genre]
        parts = {
            p: {"score": round(_mean(acc["parts"][p]), 1)}
            for p in PARTS if acc["parts"][p]
        }
        pro = round(_mean(acc["scores"]), 1)
        by_genre[genre] = {
            "pro_likeness": pro,
            "lint_penalty": round(_mean(acc["penalties"]), 1),
            "n": len(acc["scores"]),
            "parts": parts,
        }
        overall_scores.extend(acc["scores"])

    payload = {
        "date": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
        "git": _git_sha(),
        "n_clips": len(clips),
        "overall": round(_mean(overall_scores), 1),
        "pluginval": pluginval,
        "steinberg": steinberg,
        "refs": stats.get("n_refs", {}),
        "by_genre": by_genre,
        "clips": clips,
        "lint": all_issues,
        "mp3s": mp3s,
        "pitch_check": pitch_check,
        "critic": None if not critic else {
            "model": (critic_rows[0].get("model") if critic_rows else ""),
            "clips": critic_rows,
        } if critic_rows else {"skipped": True, "reason": "no GROQ_API_KEY or empty"},
    }
    if not critic:
        payload["critic"] = None
    write_reports(payload, report_dir)
    print(f"overall pro-likeness {payload['overall']:.1f}  ({len(clips)} clips)")
    in_keys = [c.get("harmony", {}).get("in_key_pct") for c in clips if c.get("harmony")]
    if in_keys:
        print(f"in-key {float(np.mean(in_keys)):.1f}%  "
              f"(range {min(in_keys):.1f}–{max(in_keys):.1f})")
    print(f"wrote {report_dir / 'report.md'}")
    return payload


def _gate_failed(payload: dict) -> bool:
    errors = [i for i in payload.get("lint", []) if i.get("severity") == "error"]
    pitch = payload.get("pitch_check") or {}
    return bool(errors) or pitch.get("ok") is False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--clips", type=Path, required=True)
    ap.add_argument("--report", type=Path, required=True)
    ap.add_argument("--pluginval-json", type=Path, default=None)
    ap.add_argument("--pitch-json", type=Path, default=None)
    ap.add_argument("--mp3", action="append", default=[])
    ap.add_argument("--critic", action="store_true")
    args = ap.parse_args()
    pluginval = {"status": "SKIP"}
    steinberg = {"status": "SKIP"}
    if args.pluginval_json and args.pluginval_json.exists():
        data = json.loads(args.pluginval_json.read_text())
        pluginval = data.get("pluginval", pluginval)
        steinberg = data.get("steinberg", steinberg)
    pitch_check = None
    if args.pitch_json and args.pitch_json.exists():
        pitch_check = json.loads(args.pitch_json.read_text())
    payload = evaluate_dir(args.clips, args.report, pluginval, steinberg,
                           args.mp3, args.critic, pitch_check)
    if _gate_failed(payload):
        errors = [i for i in payload.get("lint", []) if i.get("severity") == "error"]
        print(f"EVAL GATE FAILED ({len(errors)} lint errors, "
              f"pitch_ok={ (payload.get('pitch_check') or {}).get('ok') })",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
