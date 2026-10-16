# Harmonia eval — 2026-10-16

- git: `691e6d8`
- clips: 12
- overall pro-likeness: **77.0 / 100**
- metric: `loop-v1`
- legacy overall (equal-weight piano-ref percentiles): **76.2 / 100**
- arrangement: **78.4 / 100**

Headline score is loop-v1: reweighted ref percentiles (chord_vocab / transition_entropy at 0.25×) blended 78/22 with arrangement targets (hook restatement, call-response, density fit, groove fit, bass contour, section contrast). Hard in-key gates are unchanged. See `Tools/eval/SCORING.md`.

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Legacy | Arrange | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 79.5 | 78.5 | 83.3 | 80.2 | 87.6 | 73.6 | 69.2 | 81.7 | 0.0 |
| lofi | 73.3 | 72.0 | 77.3 | 64.2 | 80.9 | 66.6 | 84.6 | 83.7 | 3.0 |
| house | 77.5 | 75.8 | 77.5 | 74.9 | 81.6 | 76.4 | 73.4 | 81.0 | 0.0 |
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
| cinematic_s23.mid | 6 | Decent foundation but needs clearer harmonic focus, lighter textures, and more melodic balance. |
| cinematic_s7.mid | 7 | Solid foundation but needs clearer spacing and more rhythmic variety across parts. |
| house_s23.mid | 6 | Solid house foundation, but chords and bass need less clutter and more variation; melody and drums are the strongest parts. |
| house_s7.mid | 6 | The track captures house energy but feels repetitive; tighten harmonic movement, add rhythmic variation, and refine instrument balance. |
| lofi_s23.mid | 6 | Solid lofi foundation but needs clearer space and more melodic interest. |
| lofi_s7.mid | 7 | Solid foundation with room for tighter harmonic focus and more melodic interest. |
| pop_s23.mid | 7 | Solid pop foundation, but needs more melodic interest, dynamic nuance, and rhythmic variety. |
| pop_s7.mid | 7 | Solid pop foundation, but bass and melody need more movement; drums are lively, chords and piano are decent. |
| rnb_s23.mid | 6 | Solid foundation with room for harmonic depth, bass movement, and melodic phrasing improvements. |
| rnb_s7.mid | 6 | Solid foundation but dense chords and flat melodic range limit impact; modest rhythmic tweaks will lift the track. |
| trap_s23.mid | 6 | The track has a solid trap foundation but is cluttered harmonically; streamline chords, tighten bass‑drum lock, and clarify the melodic lead. |
| trap_s7.mid | 5 | The track has solid trap energy but suffers from overcrowded harmony and melodic clutter; streamline each part for clearer groove. |

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

