# Harmonia eval — 2026-10-16

- git: `e95f51b`
- clips: 12
- overall pro-likeness: **76.8 / 100**
- metric: `loop-v1`
- legacy overall (equal-weight piano-ref percentiles): **76.3 / 100**
- arrangement: **76.1 / 100**

Headline score is loop-v1: reweighted ref percentiles (chord_vocab / transition_entropy at 0.25×) blended 78/22 with arrangement targets (hook restatement, call-response, density fit, groove fit, bass contour, section contrast). Hard in-key gates are unchanged. See `Tools/eval/SCORING.md`.

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Legacy | Arrange | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 79.5 | 78.5 | 83.3 | 80.2 | 87.6 | 73.6 | 69.2 | 81.7 | 0.0 |
| lofi | 73.5 | 74.0 | 68.3 | 64.8 | 75.8 | 69.3 | 81.7 | 83.7 | 0.0 |
| house | 75.9 | 73.9 | 77.2 | 74.9 | 81.6 | 76.4 | 73.4 | 71.3 | 0.0 |
| trap | 74.7 | 73.1 | 75.9 | 58.0 | 86.7 | 69.2 | 72.2 | 85.3 | 0.0 |
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
| pop_s7.mid | 95.3 | 0.5 | 100.0 | 0.5 |
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
| cinematic_s23.mid | 6 | The piece has solid foundations but needs tighter harmonic flow, more active bass, clearer melody, and refined textures. |
| cinematic_s7.mid | 7 | Solid cinematic foundation; tighten arrangements and add melodic development for greater impact. |
| house_s23.mid | 6 | Solid house foundation but needs more variation, groove, and melodic focus. |
| house_s7.mid | 6 | Solid foundation but needs clearer chords, more melodic motion, and rhythmic nuance. |
| lofi_s23.mid | 6 | Decent foundation but needs cleaner arrangement and more dynamic variation across parts. |
| lofi_s7.mid | 7 | Solid lofi foundation, but melody and bass need more movement. |
| pop_s23.mid | 7 | Solid pop foundation, but needs tighter bass, melodic phrasing, and more dynamic variation. |
| pop_s7.mid | 6 | A solid pop foundation, but needs tighter harmonic voicing, richer bass movement, more melodic interest, and dynamic drum patterns. |
| rnb_s23.mid | 7 | Solid foundation but needs more variation and harmonic depth. |
| rnb_s7.mid | 6 | Solid foundation but needs clearer melody, less dense chords, and more rhythmic variation. |
| trap_s23.mid | 6 | The track captures trap energy but suffers from overly busy chords and limited melodic variation; tightening harmony and adding rhythmic nuance will strengthen it. |
| trap_s7.mid | 7 | Solid trap foundation with good bass and rhythm; chords and piano need simplification and dynamic tweaks. |

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

