# Synthesia AI eval — 2026-10-14

- git: `d246cfe`
- clips: 12
- overall pro-likeness: **53.7 / 100**

## Plugin validation

- pluginval: **PASS** (strictness 5)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 51.1 | 64.8 | 63.2 | 36.2 | 52.2 | 79.1 | 8.0 |
| lofi | 57.5 | 39.4 | 65.9 | 56.0 | 58.0 | 82.7 | 3.0 |
| house | 49.2 | 45.1 | 71.6 | 43.4 | 56.2 | 70.1 | 8.0 |
| trap | 53.1 | 26.6 | 60.4 | 55.4 | 38.8 | 84.6 | 0.0 |
| rnb | 61.8 | 86.0 | 49.4 | 85.5 | 70.5 | 72.8 | 11.0 |
| cinematic | 49.2 | 39.4 | 65.3 | 57.1 | 52.6 | 59.1 | 5.5 |

## Lint hits

| Clip | Rule | Severity | Part | Detail |
| --- | --- | --- | --- | --- |
| cinematic_s23.mid | part_collisions | error |  | 10 overlapping unisons across parts |
| cinematic_s7.mid | part_collisions | warn |  | 5 overlapping unisons across parts |
| house_s23.mid | part_collisions | error |  | 12 overlapping unisons across parts |
| house_s7.mid | part_collisions | error |  | 12 overlapping unisons across parts |
| lofi_s23.mid | part_collisions | warn |  | 3 overlapping unisons across parts |
| lofi_s23.mid | bass_root_conflict | warn | bass | bass is outside the chord 47% of beats |
| pop_s23.mid | part_collisions | error |  | 12 overlapping unisons across parts |
| pop_s7.mid | part_collisions | error |  | 12 overlapping unisons across parts |
| rnb_s23.mid | part_collisions | warn |  | 5 overlapping unisons across parts |
| rnb_s23.mid | bass_root_conflict | error | bass | bass is outside the chord 58% of beats |
| rnb_s7.mid | part_collisions | error |  | 9 overlapping unisons across parts |
| rnb_s7.mid | bass_root_conflict | warn | bass | bass is outside the chord 40% of beats |

## LLM critic

Skipped (no `GROQ_API_KEY`).

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

