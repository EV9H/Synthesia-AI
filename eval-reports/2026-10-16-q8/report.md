# Harmonia eval — 2026-10-16

- git: `d53b951`
- clips: 12
- overall pro-likeness: **75.4 / 100**

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 75.7 | 76.6 | 85.9 | 71.9 | 61.8 | 82.2 | 0.0 |
| lofi | 73.4 | 54.1 | 76.2 | 73.1 | 81.7 | 82.0 | 0.0 |
| house | 72.5 | 63.2 | 80.2 | 73.5 | 73.4 | 71.8 | 0.0 |
| trap | 72.6 | 46.7 | 84.7 | 74.5 | 72.2 | 84.8 | 0.0 |
| rnb | 85.4 | 93.7 | 85.7 | 86.7 | 83.8 | 77.0 | 0.0 |
| cinematic | 73.0 | 64.2 | 77.7 | 72.6 | 67.0 | 83.9 | 0.0 |

## In-key / chord-tone gates

| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |
| --- | ---: | ---: | ---: | ---: |
| cinematic_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| cinematic_s7.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| house_s23.mid | 95.4 | 0.4 | 97.2 | 1.8 |
| house_s7.mid | 95.1 | 0.3 | 94.6 | 2.1 |
| lofi_s23.mid | 100.0 | 0.0 | 98.5 | 0.0 |
| lofi_s7.mid | 100.0 | 0.0 | 98.5 | 0.0 |
| pop_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| pop_s7.mid | 95.9 | 0.0 | 100.0 | 0.0 |
| rnb_s23.mid | 100.0 | 0.0 | 97.0 | 0.0 |
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
| cinematic_s23.mid | 7 | Solid foundation with room for more harmonic depth, rhythmic variety, and expressive nuance. |
| cinematic_s7.mid | 6 | Solid foundation but needs more harmonic motion, melodic variety, and rhythmic nuance. |
| house_s23.mid | 6 | The track has solid house foundations but needs cleaner chords, more dynamic bass, a stronger melodic hook, and less piano clutter. |
| house_s7.mid | 7 | Solid house foundation; tighten chord sustain, vary bass, streamline synth, lighten piano, and tighten hi‑hat rhythm. |
| lofi_s23.mid | 7 | Solid foundation with decent harmonic and piano work, but bass and melody need more variation; drums slightly busy. |
| lofi_s7.mid | 7 | Overall solid lofi track; tighten bass and melody for smoother flow. |
| pop_s23.mid | 7 | Solid pop foundation, but needs tighter voice leading, melodic phrasing, and rhythmic variation. |
| pop_s7.mid | 7 | Solid pop arrangement with good range; tighten chord density, enrich bass line, vary melody, brighten piano, and spice up drums. |
| rnb_s23.mid | 7 | Solid R&B foundation with good tonal balance, but needs more harmonic and rhythmic variation for interest. |
| rnb_s7.mid | 6 | Solid foundation but needs tighter harmony, stronger bass groove, a defined melody, and refined drum dynamics. |
| trap_s23.mid | 5 | The track feels cluttered and static; simplify chords, add movement to bass and melody, and spice up drums. |
| trap_s7.mid | 6 | The track has solid foundations but is overly dense in harmony and piano; streamline chords, add melodic focus, and enhance drum dynamics. |

## Preview MP3s

- `lofi_s7.mp3`
- `house_s7.mp3`
- `trap_s7.mp3`
- `pop_s7.mp3`
- `rnb_s7.mp3`
- `cinematic_s7.mp3`

## How to run

```bash
Tools/eval/run_eval.sh                  # N=2 clips/genre, pluginval, reports
N=3 Tools/eval/run_eval.sh              # more clips
SKIP_PLUGINVAL=1 Tools/eval/run_eval.sh # MIDI scoring only
Tools/validate_plugin.sh                # VST3 + pluginval only
```

Reports land in `eval-reports/<date>/` (markdown + JSON). Reference MIDI is cached in `.cache/eval/` (gitignored).

