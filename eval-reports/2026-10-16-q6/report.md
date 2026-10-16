# Synthesia AI eval — 2026-10-16

- git: `468e359`
- clips: 12
- overall pro-likeness: **75.2 / 100**

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 74.8 | 76.6 | 86.3 | 67.6 | 61.5 | 82.2 | 0.0 |
| lofi | 73.4 | 55.3 | 76.2 | 72.2 | 81.6 | 82.0 | 0.0 |
| house | 72.6 | 63.2 | 81.8 | 71.9 | 74.0 | 71.8 | 0.0 |
| trap | 71.0 | 51.0 | 82.7 | 71.2 | 65.6 | 84.8 | 0.0 |
| rnb | 85.9 | 93.7 | 86.9 | 86.5 | 85.2 | 77.0 | 0.0 |
| cinematic | 73.3 | 64.2 | 77.4 | 74.0 | 67.0 | 83.9 | 0.0 |

## In-key / chord-tone gates

| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |
| --- | ---: | ---: | ---: | ---: |
| cinematic_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| cinematic_s7.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| house_s23.mid | 96.2 | 0.3 | 97.3 | 1.4 |
| house_s7.mid | 95.9 | 0.3 | 95.9 | 1.7 |
| lofi_s23.mid | 100.0 | 0.0 | 95.6 | 0.0 |
| lofi_s7.mid | 100.0 | 0.0 | 95.8 | 0.0 |
| pop_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| pop_s7.mid | 96.0 | 0.0 | 100.0 | 0.0 |
| rnb_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| rnb_s7.mid | 100.0 | 0.0 | 98.5 | 0.0 |
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
| cinematic_s23.mid | 7 | Solid foundation, but needs more movement and variation across parts. |
| cinematic_s7.mid | 7 | Solid foundation, but chords and melody need more space and variation for cinematic impact. |
| house_s23.mid | 7 | Solid house foundation, but chords and melody need simplification for stronger groove. |
| house_s7.mid | 7 | Solid house foundation, but chords and melody need tightening; bass and drums are functional with minor tweaks. |
| lofi_s23.mid | 7 | Solid foundation but chord and piano parts are overcrowded; simplify for a cleaner lofi vibe. |
| lofi_s7.mid | 6 | Decent foundation but chords and piano are overly dense; tighten harmony and simplify piano for a smoother lo‑fi vibe. |
| pop_s23.mid | 6 | Solid foundation but needs clearer melody, sparser chords, and more rhythmic interest. |
| pop_s7.mid | 7 | Solid pop foundation but needs more melodic interest and rhythmic variation. |
| rnb_s23.mid | 7 | Solid R&B foundation with good groove, but needs less chord clutter, richer melodic movement, and more dynamic variation. |
| rnb_s7.mid | 6 | Solid foundation but needs tighter groove, smoother melodic motion, richer piano voicings, and cleaner drum programming. |
| trap_s23.mid |  |  |
| trap_s7.mid | 6 | Decent trap foundation, but chords and melody need simplification; bass and drums are solid, piano could be tighter. |

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

