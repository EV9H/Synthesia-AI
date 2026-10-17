# Harmonia eval — 2026-10-17

- git: `d577f97`
- clips: 2
- overall pro-likeness: **78.6 / 100**
- metric: `loop-v1`
- legacy overall (equal-weight piano-ref percentiles): **75.3 / 100**
- arrangement: **84.4 / 100**

Headline score is loop-v1: reweighted ref percentiles (chord_vocab / transition_entropy at 0.25×) blended 78/22 with arrangement targets (hook restatement, call-response, density fit, groove fit, bass contour, section contrast). Hard in-key gates are unchanged. See `Tools/eval/SCORING.md`.

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Legacy | Arrange | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |
| lofi | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |
| house | 78.6 | 75.3 | 84.4 | 74.9 | 81.1 | 76.8 | 71.5 | 80.7 | 0.0 |
| trap | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |
| rnb | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |
| cinematic | 0.0 | 0.0 | 0.0 | — | — | — | — | — | 0.0 |

## In-key / chord-tone gates

| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |
| --- | ---: | ---: | ---: | ---: |
| house_s23.mid | 96.1 | 0.4 | 98.4 | 1.2 |
| house_s7.mid | 95.1 | 0.4 | 96.9 | 1.5 |

## Sampler pitch check

Skipped.

## Lint hits

No lint issues.

## LLM critic

Skipped (no `GROQ_API_KEY`).

## How to run

```bash
Tools/eval/run_eval.sh                  # N=2 clips/genre, pluginval, reports
N=3 Tools/eval/run_eval.sh              # more clips
SKIP_PLUGINVAL=1 Tools/eval/run_eval.sh # MIDI scoring only
Tools/validate_plugin.sh                # VST3 + pluginval only
```

Reports land in `eval-reports/<date>/` (markdown + JSON). Reference MIDI is cached in `.cache/eval/` (gitignored).

