"""Write markdown + JSON eval reports."""
from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from paths import GENRES


def write_reports(payload: dict[str, Any], out_dir: Path) -> tuple[Path, Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "report.json"
    md_path = out_dir / "report.md"
    json_path.write_text(json.dumps(payload, indent=2, default=str))
    md_path.write_text(_markdown(payload))
    return md_path, json_path


def _markdown(p: dict[str, Any]) -> str:
    when = p.get("date") or datetime.now(timezone.utc).strftime("%Y-%m-%d")
    git = p.get("git", "")
    lines = [
        f"# Harmonia eval — {when}",
        "",
        f"- git: `{git}`",
        f"- clips: {p.get('n_clips', 0)}",
        f"- overall pro-likeness: **{p.get('overall', 0):.1f} / 100**",
        "",
        "## Plugin validation",
        "",
        f"- pluginval: **{p.get('pluginval', {}).get('status', 'n/a')}**"
        f" (strictness {p.get('pluginval', {}).get('strictness', '-')})",
        f"- Steinberg validator: **{p.get('steinberg', {}).get('status', 'n/a')}**",
        "",
        "## Pro-likeness by genre",
        "",
        "| Genre | Overall | Chords | Bass | Melody | Piano | Drums | Lint |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    by = p.get("by_genre", {})
    for genre in GENRES:
        row = by.get(genre, {})
        parts = row.get("parts", {})
        def cell(name: str) -> str:
            val = parts.get(name, {}).get("score")
            return f"{val:.1f}" if isinstance(val, (int, float)) else "—"
        lines.append(
            f"| {genre} | {row.get('pro_likeness', 0):.1f} | {cell('chords')} | {cell('bass')} "
            f"| {cell('synth')} | {cell('piano')} | {cell('drums')} | {row.get('lint_penalty', 0):.1f} |"
        )
    lines += [
        "",
        "## In-key / chord-tone gates",
        "",
        "| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for clip in p.get("clips", []):
        h = clip.get("harmony") or {}
        lines.append(
            f"| {clip.get('file', '')} | {h.get('in_key_pct', 0):.1f} | "
            f"{h.get('out_of_key_not_chord_pct', 0):.1f} | "
            f"{h.get('strong_chord_tone_pct', 0):.1f} | "
            f"{h.get('chromatic_passing_pct', 0):.1f} |"
        )
    pitch = p.get("pitch_check")
    lines += ["", "## Sampler pitch check", ""]
    if not pitch:
        lines.append("Skipped.")
    else:
        status = "PASS" if pitch.get("ok") else "FAIL"
        lines.append(f"- status: **{status}** (limit {pitch.get('cents_limit', 50)} cents)")
        for genre, row in (pitch.get("genres") or {}).items():
            if row.get("ok"):
                lines.append(f"- {genre}: ok ({row.get('n', 0)} notes)")
            elif "error" in row:
                lines.append(f"- {genre}: render failed — {row['error']}")
            else:
                lines.append(f"- {genre}: {row.get('failed', 0)} off-pitch notes")
    lines += [
        "",
        "## Lint hits",
        "",
    ]
    issues = p.get("lint", [])
    if not issues:
        lines.append("No lint issues.")
    else:
        lines.append("| Clip | Rule | Severity | Part | Detail |")
        lines.append("| --- | --- | --- | --- | --- |")
        for issue in issues[:80]:
            lines.append(
                f"| {issue.get('clip', '')} | {issue.get('rule')} | {issue.get('severity')} "
                f"| {issue.get('part', '')} | {issue.get('detail', '')} |"
            )

    critic = p.get("critic")
    lines += ["", "## LLM critic", ""]
    if not critic:
        lines.append("Skipped (no `GROQ_API_KEY`).")
    else:
        lines.append(f"Model: `{critic.get('model', '')}`")
        lines.append("")
        lines.append("| Clip | Overall | Summary |")
        lines.append("| --- | ---: | --- |")
        for item in critic.get("clips", []):
            lines.append(
                f"| {item.get('clip', '')} | {item.get('overall', '')} | {item.get('summary', '')} |"
            )

    renders = p.get("mp3s") or []
    if renders:
        lines += ["", "## Preview MP3s", ""]
        for name in renders:
            lines.append(f"- `{name}`")

    lines += [
        "",
        "## How to run",
        "",
        "```bash",
        "Tools/eval/run_eval.sh                  # N=2 clips/genre, pluginval, reports",
        "N=3 Tools/eval/run_eval.sh              # more clips",
        "SKIP_PLUGINVAL=1 Tools/eval/run_eval.sh # MIDI scoring only",
        "Tools/validate_plugin.sh                # VST3 + pluginval only",
        "```",
        "",
        "Reports land in `eval-reports/<date>/` (markdown + JSON). Reference MIDI is cached in `.cache/eval/` (gitignored).",
        "",
    ]
    return "\n".join(lines) + "\n"
