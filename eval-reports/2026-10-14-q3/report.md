# Synthesia AI eval — 2026-10-14

- git: `4975aa7`
- clips: 12
- overall pro-likeness: **65.4 / 100**

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 67.2 | 72.0 | 61.8 | 67.5 | 55.5 | 79.1 | 0.0 |
| lofi | 65.8 | 59.7 | 61.6 | 64.6 | 60.5 | 82.7 | 0.0 |
| house | 64.5 | 53.8 | 70.3 | 60.5 | 67.8 | 70.1 | 0.0 |
| trap | 60.0 | 52.0 | 67.7 | 50.9 | 44.8 | 84.6 | 0.0 |
| rnb | 72.5 | 86.5 | 52.8 | 84.8 | 65.5 | 72.8 | 0.0 |
| cinematic | 62.2 | 58.4 | 72.7 | 58.3 | 62.6 | 59.1 | 0.0 |

## Lint hits

No lint issues.

## LLM critic

Model: `openai/gpt-oss-120b`

| Clip | Overall | Summary |
| --- | ---: | --- |
| cinematic_s23.mid | 5 | The arrangement feels cluttered and rhythmically thin; streamline chords, enrich bass and melody, and add full drum pattern. |
| cinematic_s7.mid | 5 | Solid foundation but needs tighter arrangement, richer rhythm, and melodic focus. |
| house_s23.mid | 7 | Solid house foundation, but needs more harmonic and rhythmic nuance. |
| house_s7.mid | 6 | Solid house foundation, but chords and piano are overcrowded; tighten harmony and add bass variation. |
| lofi_s23.mid | 6 | Solid foundation but needs more harmonic color, melodic variety, and space for a relaxed lofi vibe. |
| lofi_s7.mid | 6 | Solid lo‑fi foundation, but needs tighter voice leading, bass movement, and more melodic nuance. |
| pop_s23.mid | 6 | Solid pop foundation, but needs clearer melody, less chord clutter, and more rhythmic interest. |
| pop_s7.mid | 7 | Solid pop foundation, but needs more dynamic variation and smoother transitions across parts. |
| rnb_s23.mid | 7 | Solid foundation with good rhythm section; improve melodic interest and harmonic flow for a tighter R&B feel. |
| rnb_s7.mid | 7 | Solid foundation but needs tighter chord voicings, fuller bass, smoother melody, coordinated piano, and richer drum texture. |
| trap_s23.mid |  |  |
| trap_s7.mid | 6 | Solid trap foundation, but chords and piano are too busy; simplify and add more groove to bass and melody. |

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

