# Harmonia eval — 2026-10-16

- git: `38bcf41`
- clips: 12
- overall pro-likeness: **76.8 / 100**
- metric: `loop-v1`
- legacy overall (equal-weight piano-ref percentiles): **75.9 / 100**
- arrangement: **78.3 / 100**

Headline score is loop-v1: reweighted ref percentiles (chord_vocab / transition_entropy at 0.25×) blended 78/22 with arrangement targets (hook restatement, call-response, density fit, groove fit, bass contour, section contrast). Hard in-key gates are unchanged. See `Tools/eval/SCORING.md`.

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Legacy | Arrange | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 79.5 | 78.5 | 83.3 | 80.2 | 87.6 | 73.6 | 69.2 | 81.7 | 0.0 |
| lofi | 73.3 | 72.0 | 77.4 | 64.2 | 80.9 | 66.6 | 84.6 | 83.5 | 3.0 |
| house | 75.9 | 74.0 | 77.2 | 74.9 | 81.6 | 76.4 | 73.4 | 71.8 | 0.0 |
| trap | 74.7 | 73.1 | 75.9 | 58.0 | 86.7 | 69.2 | 72.2 | 85.3 | 0.0 |
| rnb | 83.1 | 85.1 | 79.2 | 92.6 | 83.6 | 83.2 | 83.8 | 77.8 | 0.0 |
| cinematic | 74.2 | 72.9 | 77.0 | 66.1 | 79.3 | 76.8 | 71.6 | 73.0 | 0.0 |

## In-key / chord-tone gates

| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |
| --- | ---: | ---: | ---: | ---: |
| cinematic_s23.mid | 100.0 | 0.0 | 95.7 | 0.0 |
| cinematic_s7.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| house_s23.mid | 95.3 | 0.4 | 97.3 | 1.4 |
| house_s7.mid | 95.2 | 0.3 | 94.7 | 1.7 |
| lofi_s23.mid | 100.0 | 0.0 | 99.0 | 0.0 |
| lofi_s7.mid | 100.0 | 0.0 | 98.7 | 0.0 |
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

| Clip | Rule | Severity | Part | Detail |
| --- | --- | --- | --- | --- |
| lofi_s23.mid | bass_root_conflict | warn | bass | bass is outside the chord 38% of beats |
| lofi_s7.mid | bass_root_conflict | warn | bass | bass is outside the chord 46% of beats |

## LLM critic

Model: `openai/gpt-oss-120b`

| Clip | Overall | Summary |
| --- | ---: | --- |
| cinematic_s23.mid | 7 | Solid foundation with good harmonic content; tighten bass movement, diversify melody rhythm, and streamline drums for a cleaner cinematic feel. |
| cinematic_s7.mid | 7 | Solid foundation but needs clearer voicing, tighter rhythm, and more melodic smoothness. |
| house_s23.mid | 6 | The track has a solid house foundation but needs more harmonic depth, melodic interest, and rhythmic variation to feel fully engaging. |
| house_s7.mid | 6 | The track has solid house fundamentals but suffers from cluttered chords and piano; tighten rhythm and simplify harmony for a tighter groove. |
| lofi_s23.mid | 6 | Solid lo‑fi foundation, but needs more dynamic variation and space across parts. |
| lofi_s7.mid | 7 | Solid lofi foundation with good harmonic content, but tighten arrangement and add rhythmic nuance for a smoother flow. |
| pop_s23.mid | 6 | Solid pop foundation but needs clearer chords, richer bass, more melodic variation, and dynamic piano and drum details. |
| pop_s7.mid | 7 | Solid pop foundation with decent drums and chords; bass and melody need more motion and variation. |
| rnb_s23.mid | 6 | Solid foundation but needs tighter voice leading, groove, and melodic focus for a polished R&B feel. |
| rnb_s7.mid | 6 | Solid foundation but needs clearer harmony, stronger melodic hooks, and more dynamic variation across parts. |
| trap_s23.mid | 6 | Decent foundation but needs tighter harmony, simpler melody, and more genre‑specific drum patterns. |
| trap_s7.mid |  |  |

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

