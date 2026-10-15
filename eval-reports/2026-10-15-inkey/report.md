# Synthesia AI eval — 2026-10-15

- git: `813de11`
- clips: 12
- overall pro-likeness: **71.6 / 100**

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 72.8 | 78.0 | 63.6 | 68.6 | 75.0 | 79.1 | 0.0 |
| lofi | 67.4 | 55.1 | 63.2 | 70.1 | 66.1 | 82.6 | 0.0 |
| house | 65.2 | 52.4 | 71.8 | 61.6 | 70.2 | 70.0 | 0.0 |
| trap | 70.8 | 52.9 | 70.6 | 73.8 | 72.5 | 84.0 | 0.0 |
| rnb | 82.8 | 88.6 | 82.5 | 84.5 | 85.3 | 72.8 | 0.0 |
| cinematic | 70.9 | 64.3 | 75.3 | 61.7 | 67.0 | 86.2 | 0.0 |

## In-key / chord-tone gates

| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |
| --- | ---: | ---: | ---: | ---: |
| cinematic_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| cinematic_s7.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| house_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| house_s7.mid | 100.0 | 0.0 | 98.8 | 0.0 |
| lofi_s23.mid | 100.0 | 0.0 | 98.3 | 0.0 |
| lofi_s7.mid | 100.0 | 0.0 | 95.5 | 0.0 |
| pop_s23.mid | 96.2 | 0.4 | 98.3 | 1.3 |
| pop_s7.mid | 94.7 | 0.4 | 100.0 | 1.2 |
| rnb_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| rnb_s7.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| trap_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| trap_s7.mid | 100.0 | 0.0 | 100.0 | 0.0 |

## Sampler pitch check

- status: **PASS** (limit 50.0 cents)
- lofi: ok (32 notes)
- house: ok (32 notes)
- trap: ok (32 notes)

## Lint hits

No lint issues.

## LLM critic

Model: `openai/gpt-oss-120b`

| Clip | Overall | Summary |
| --- | ---: | --- |
| cinematic_s23.mid | 5 | Solid foundation but arrangement feels crowded; tighten each part for cinematic impact. |
| cinematic_s7.mid | 7 | Solid foundation with room for clearer textures and tighter rhythmic focus. |
| house_s23.mid | 6 | Solid foundation but needs cleaner chords, more bass variation, and livelier melodic and piano parts. |
| house_s7.mid | 7 | Solid house foundation, but needs more variation and dynamic interest across parts. |
| lofi_s23.mid | 7 | Solid foundation with good piano and drums; bass and melody need more movement and character. |
| lofi_s7.mid | 6 | Solid lofi foundation, but needs richer harmony, clearer melody, and more rhythmic nuance. |
| pop_s23.mid | 6 | Solid foundation but needs more melodic interest, smoother chord transitions, and dynamic drum variation. |
| pop_s7.mid | 6 | The track has solid foundations but needs more melodic focus, bass movement, and refined drum groove. |
| rnb_s23.mid | 5 | The track has basic R&B elements but lacks cohesive harmony, melodic focus, and rhythmic nuance; tighten each part for a tighter groove. |
| rnb_s7.mid |  |  |
| trap_s23.mid | 6 | Solid trap foundation but chords and melody are overcrowded; tighten and add low‑end punch. |
| trap_s7.mid | 6 | Solid foundation but overly busy chords/piano; tighten melodies and add trap‑style nuances. |

## Preview MP3s

- `lofi_s7.mp3`
- `house_s7.mp3`
- `trap_s7.mp3`

## How to run

```bash
Tools/eval/run_eval.sh                  # N=2 clips/genre, pluginval, reports
N=3 Tools/eval/run_eval.sh              # more clips
SKIP_PLUGINVAL=1 Tools/eval/run_eval.sh # MIDI scoring only
Tools/validate_plugin.sh                # VST3 + pluginval only
```

Reports land in `eval-reports/<date>/` (markdown + JSON). Reference MIDI is cached in `.cache/eval/` (gitignored).

