# Synthesia AI eval — 2026-10-14

- git: `66caa71`
- clips: 12
- overall pro-likeness: **73.2 / 100**

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 73.5 | 72.0 | 63.6 | 77.5 | 75.0 | 79.1 | 0.0 |
| lofi | 69.1 | 59.7 | 63.2 | 76.1 | 63.9 | 82.6 | 0.0 |
| house | 67.4 | 59.2 | 72.6 | 65.5 | 69.7 | 70.0 | 0.0 |
| trap | 71.9 | 52.9 | 70.6 | 79.8 | 72.5 | 84.0 | 0.0 |
| rnb | 84.1 | 86.5 | 88.2 | 87.2 | 85.5 | 72.8 | 0.0 |
| cinematic | 73.2 | 64.3 | 75.3 | 73.0 | 67.0 | 86.2 | 0.0 |

## Lint hits

No lint issues.

## LLM critic

Model: `openai/gpt-oss-120b`

| Clip | Overall | Summary |
| --- | ---: | --- |
| cinematic_s23.mid | 7 | Solid foundation, but needs tighter harmonic motion, more active bass, and melodic development. |
| cinematic_s7.mid | 7 | Solid foundation but needs clearer voicing, melodic variety, and less busy percussion. |
| house_s23.mid | 7 | Solid foundation with room for tighter voice leading, melodic alignment, and rhythmic variation. |
| house_s7.mid | 6 | Solid house foundation, but needs clearer chord texture, more dynamic bass, distinct melodic hook, and piano variation. |
| lofi_s23.mid | 7 | Solid harmonic foundation and piano work; bass and melody need more focus and rhythmic interest, drums could be tighter. |
| lofi_s7.mid | 7 | Solid lofi foundation; tighten chords and piano, enrich synth melody, and add subtle drum nuances. |
| pop_s23.mid | 6 | Solid foundation but needs tighter harmonic movement, more melodic interest, and rhythmic nuance. |
| pop_s7.mid | 7 | Solid pop foundation, but melody and bass need more variation and smoother transitions. |
| rnb_s23.mid | 6 | Solid foundation but needs tighter harmonic alignment and cleaner synth/melody choices. |
| rnb_s7.mid | 6 | Solid foundation but needs more harmonic depth, melodic interest, and rhythmic nuance. |
| trap_s23.mid | 5 | The track has basic elements but needs tighter harmonic focus, clearer melodic motifs, and more rhythmic nuance. |
| trap_s7.mid | 6 | Decent trap foundation but needs tighter arrangement and less clutter. |

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

