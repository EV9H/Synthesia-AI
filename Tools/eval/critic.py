"""Optional Groq critic. Skips cleanly when GROQ_API_KEY is unset."""
from __future__ import annotations

import json
import os
import time
import urllib.error
import urllib.request
from typing import Any

from midi_parse import Clip, drum_class
from paths import PARTS

GROQ_URL = os.environ.get("GROQ_API_BASE", "https://api.groq.com/openai/v1") + "/chat/completions"
GROQ_MODEL = os.environ.get("GROQ_MODEL", "openai/gpt-oss-120b")


def midi_to_text(clip: Clip, genre: str, bars: int | None = None) -> str:
    n_bars = int(bars or max(1, round(clip.bars())))
    lines = [
        f"GENRE {genre} TEMPO {clip.tempo:.0f} BARS {n_bars} DURATION {clip.duration:.1f}s",
    ]
    for part in PARTS:
        notes = clip.of(part)
        if not notes:
            lines.append(f"{part.upper()}: (silent)")
            continue
        pitches = [n.pitch for n in notes]
        if part == "drums":
            k = sum(1 for n in notes if drum_class(n.pitch) == "kick")
            s = sum(1 for n in notes if drum_class(n.pitch) == "snare")
            h = sum(1 for n in notes if drum_class(n.pitch) == "hat")
            lines.append(f"DRUMS: {len(notes)} hits  k={k} s={s} h={h}")
            continue
        pcs = sorted({p % 12 for p in pitches})
        names = "C C# D D# E F F# G G# A A# B".split()
        lines.append(
            f"{part.upper()}: n={len(notes)} range={min(pitches)}-{max(pitches)} "
            f"pcs={','.join(names[p] for p in pcs[:8])}"
        )
    # Compact pitch sketch per bar for bass + melody.
    beat_per_bar = clip.ts_num * 4 / max(1, clip.ts_den)
    for part in ("bass", "synth", "chords"):
        notes = clip.of(part)
        if not notes:
            continue
        sketch = []
        for bar in range(min(n_bars, 8)):
            t0 = bar * beat_per_bar * 60.0 / clip.tempo
            t1 = (bar + 1) * beat_per_bar * 60.0 / clip.tempo
            hit = [n.pitch for n in notes if t0 <= n.start < t1]
            if not hit:
                sketch.append("-")
            else:
                sketch.append(".".join(str(p) for p in hit[:6]))
        lines.append(f"{part} bars: " + " | ".join(sketch))
    return "\n".join(lines)


def critic_clip(clip: Clip, genre: str) -> dict[str, Any] | None:
    key = os.environ.get("GROQ_API_KEY", "").strip()
    if not key:
        return None
    payload = {
        "model": GROQ_MODEL,
        "temperature": 0.2,
        "messages": [
            {
                "role": "system",
                "content": (
                    "You are a session musician reviewing generated MIDI. "
                    "Score each of chords, bass, melody, piano, drums from 1-10, plus overall. "
                    "Give one short fix per part under 20 words. "
                    "Reply ONLY JSON: "
                    '{"chords":{"score":n,"fix":"..."},"bass":{"score":n,"fix":"..."},'
                    '"melody":{"score":n,"fix":"..."},"piano":{"score":n,"fix":"..."},'
                    '"drums":{"score":n,"fix":"..."},"overall":n,"summary":"..."}'
                ),
            },
            {"role": "user", "content": midi_to_text(clip, genre)},
        ],
    }
    body = None
    last_error = None
    for attempt in range(4):
        req = urllib.request.Request(
            GROQ_URL,
            data=json.dumps(payload).encode(),
            headers={
                "Authorization": f"Bearer {key}",
                "Content-Type": "application/json",
                "User-Agent": "Mozilla/5.0 (compatible; SynthesiaEval/1.0)",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=45) as resp:
                body = json.loads(resp.read().decode())
            last_error = None
            break
        except urllib.error.HTTPError as exc:
            last_error = exc
            if exc.code in {429, 503} and attempt < 3:
                time.sleep(2 ** attempt)
                continue
            return {"skipped": True, "reason": str(exc)}
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, OSError) as exc:
            return {"skipped": True, "reason": str(exc)}
    if body is None:
        return {"skipped": True, "reason": str(last_error or "empty critic response")}
    text = body.get("choices", [{}])[0].get("message", {}).get("content", "")
    text = text.strip()
    if text.startswith("```"):
        text = text.strip("`")
        text = text.split("\n", 1)[-1]
        if text.endswith("```"):
            text = text[: text.rfind("```")]
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError:
        start, end = text.find("{"), text.rfind("}")
        if start >= 0 and end > start:
            try:
                parsed = json.loads(text[start : end + 1])
            except json.JSONDecodeError:
                return {"skipped": True, "reason": "unparseable critic JSON", "raw": text[:500]}
        else:
            return {"skipped": True, "reason": "unparseable critic JSON", "raw": text[:500]}
    parsed["model"] = GROQ_MODEL
    return parsed
