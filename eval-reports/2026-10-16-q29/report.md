# Harmonia eval — 2026-10-17

- git: `686ef54`
- clips: 2
- overall pro-likeness: **75.3 / 100**
- metric: `loop-v1`
- legacy overall (equal-weight piano-ref percentiles): **73.9 / 100**
- arrangement: **76.6 / 100**

Headline score is loop-v1: reweighted ref percentiles (chord_vocab / transition_entropy at 0.25×) blended 78/22 with arrangement targets (hook restatement, call-response, density fit, groove fit, bass contour, section contrast). Hard in-key gates are unchanged. See `Tools/eval/SCORING.md`.

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Legacy | Arrange | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |
| lofi | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |
| house | 75.3 | 73.9 | 76.6 | 63.8 | 82.7 | 72.3 | 74.7 | 81.0 | 0.0 |
| trap | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |
| rnb | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |
| cinematic | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |

## In-key / chord-tone gates

| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |
| --- | ---: | ---: | ---: | ---: |
| house_s23.mid | 95.2 | 0.4 | 95.8 | 1.8 |
| house_s7.mid | 94.3 | 0.0 | 97.3 | 2.1 |

## Sampler pitch check

Skipped.

## Lint hits

No lint issues.

## LLM critic

Skipped (no `GROQ_API_KEY`).

## Preview MP3s

- `house_s7.mp3`

## How to run

```bash
Tools/eval/run_eval.sh                  # N=2 clips/genre, pluginval, reports
N=3 Tools/eval/run_eval.sh              # more clips
SKIP_PLUGINVAL=1 Tools/eval/run_eval.sh # MIDI scoring only
Tools/validate_plugin.sh                # VST3 + pluginval only
```

Reports land in `eval-reports/<date>/` (markdown + JSON). Reference MIDI is cached in `.cache/eval/` (gitignored).

