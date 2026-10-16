# Harmonia eval — 2026-10-16

- git: `b3f7946`
- clips: 12
- overall pro-likeness: **76.3 / 100**
- metric: `loop-v1`
- legacy overall (equal-weight piano-ref percentiles): **75.9 / 100**
- arrangement: **75.9 / 100**

Headline score is loop-v1: reweighted ref percentiles (chord_vocab / transition_entropy at 0.25×) blended 78/22 with arrangement targets (hook restatement, call-response, density fit, groove fit, bass contour, section contrast). Hard in-key gates are unchanged. See `Tools/eval/SCORING.md`.

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Legacy | Arrange | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 77.9 | 76.8 | 82.3 | 80.3 | 87.6 | 72.0 | 61.8 | 81.7 | 0.0 |
| lofi | 73.5 | 74.0 | 68.3 | 64.8 | 75.8 | 69.3 | 81.7 | 83.7 | 0.0 |
| house | 75.9 | 73.9 | 77.2 | 74.9 | 81.6 | 76.4 | 73.4 | 71.3 | 0.0 |
| trap | 73.7 | 72.2 | 75.7 | 58.0 | 86.7 | 63.2 | 72.2 | 85.3 | 0.0 |
| rnb | 83.1 | 85.1 | 79.2 | 92.6 | 83.6 | 83.2 | 83.8 | 77.8 | 0.0 |
| cinematic | 73.8 | 73.3 | 72.6 | 72.7 | 77.7 | 69.7 | 67.0 | 83.6 | 0.0 |

## In-key / chord-tone gates

| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |
| --- | ---: | ---: | ---: | ---: |
| cinematic_s23.mid | 100.0 | 0.0 | 98.7 | 0.0 |
| cinematic_s7.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| house_s23.mid | 95.3 | 0.4 | 97.3 | 1.4 |
| house_s7.mid | 95.2 | 0.3 | 94.7 | 1.7 |
| lofi_s23.mid | 100.0 | 0.0 | 98.5 | 0.0 |
| lofi_s7.mid | 100.0 | 0.0 | 97.0 | 0.0 |
| pop_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| pop_s7.mid | 95.9 | 0.0 | 100.0 | 0.0 |
| rnb_s23.mid | 100.0 | 0.0 | 96.7 | 0.0 |
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
| cinematic_s23.mid | 6 | Solid base but needs more dynamic variation and clearer voice leading across parts. |
| cinematic_s7.mid | 6 | Solid foundation but needs clearer spacing, fuller bass, melodic completeness, expressive piano, and more drum variation. |
| house_s23.mid | 5 | The track has a solid house foundation but needs clearer harmonic focus, more dynamic bass, and a stronger melodic hook. |
| house_s7.mid | 6 | Solid foundation but needs less clutter in chords, more melodic interest, and varied drum accents. |
| lofi_s23.mid | 7 | A solid lo‑fi foundation with good chord and drum work, but bass and melody need more movement and variation. |
| lofi_s7.mid | 6 | Decent foundation but needs cleaner voicings, tighter bass, a clear melodic hook, and a more relaxed drum groove. |
| pop_s23.mid | 6 | Solid foundation but overly busy chords and drums; streamline parts and add melodic interest. |
| pop_s7.mid | 7 | Solid pop foundation with room for tighter harmonic spacing, more bass movement, smoother melodic leaps, dynamic piano, and a drum fill. |
| rnb_s23.mid | 6 | Overall solid R&B foundation but needs more harmonic and rhythmic interest. |
| rnb_s7.mid | 6 | Solid foundation but needs less clutter and more rhythmic/melodic variation. |
| trap_s23.mid | 6 | Decent trap foundation, but chords and melody need simplification; tighten bass‑drum lock and refine piano placement. |
| trap_s7.mid | 6 | Solid trap foundation, but needs more harmonic and melodic variation. |

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

