# Harmonia eval — 2026-10-16

- git: `85cc5fd`
- clips: 12
- overall pro-likeness: **75.1 / 100**

## Plugin validation

- pluginval: **SKIP** (strictness -)
- Steinberg validator: **SKIP**

## Pro-likeness by genre

| Genre | Overall | Chords | Bass | Melody | Piano | Drums | Lint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| pop | 73.7 | 76.6 | 86.3 | 61.7 | 61.7 | 82.2 | 0.0 |
| lofi | 72.4 | 54.1 | 76.2 | 68.2 | 81.7 | 82.0 | 0.0 |
| house | 72.4 | 63.2 | 81.8 | 71.8 | 73.4 | 71.8 | 0.0 |
| trap | 73.3 | 46.7 | 88.2 | 74.4 | 72.2 | 84.8 | 0.0 |
| rnb | 85.8 | 93.7 | 86.9 | 86.5 | 85.2 | 77.0 | 0.0 |
| cinematic | 73.1 | 64.2 | 77.4 | 72.8 | 67.0 | 83.9 | 0.0 |

## In-key / chord-tone gates

| Clip | In-key % | Out-key (not chord) % | Strong-beat chord-tone % | Chromatic pass % |
| --- | ---: | ---: | ---: | ---: |
| cinematic_s23.mid | 100.0 | 0.0 | 98.7 | 0.0 |
| cinematic_s7.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| house_s23.mid | 96.0 | 0.4 | 97.3 | 1.4 |
| house_s7.mid | 95.8 | 0.4 | 95.9 | 1.8 |
| lofi_s23.mid | 100.0 | 0.0 | 97.0 | 0.0 |
| lofi_s7.mid | 100.0 | 0.0 | 95.5 | 0.0 |
| pop_s23.mid | 100.0 | 0.0 | 100.0 | 0.0 |
| pop_s7.mid | 95.8 | 0.0 | 100.0 | 0.0 |
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
| cinematic_s23.mid | 7 | Solid foundation, but tighten harmony, enrich bass, and add melodic and rhythmic nuance. |
| cinematic_s7.mid | 6 | Solid foundation but needs more space, melodic interest, and dynamic nuance. |
| house_s23.mid | 6 | Solid foundation but needs clearer harmonic focus, tighter bass groove, and a more memorable synth line. |
| house_s7.mid | 6 | The track has solid rhythm but chord and melodic elements are overcrowded; streamline parts for a tighter house groove. |
| lofi_s23.mid | 7 | Solid lofi foundation; tighten chords, enrich bass and melody, and soften dynamics for a smoother vibe. |
| lofi_s7.mid | 7 | Solid harmonic foundation, but bass and drums need more groove and space; melody could use lofi nuances. |
| pop_s23.mid | 7 | Solid pop foundation with good chord work; bass and drums need tighter rhythm, melody could use more variation. |
| pop_s7.mid | 6 | Solid pop foundation but needs clearer harmony, stronger bass groove, defined melodic hook, and tighter piano‑chord interaction. |
| rnb_s23.mid | 6 | Decent foundation but needs clearer chord voicings, livelier bass lines, and more dynamic melodic and rhythmic elements. |
| rnb_s7.mid | 5 | The arrangement has decent pitch ranges but suffers from overcrowded chords and overly busy drums; tighten each part for a cleaner R&B feel. |
| trap_s23.mid | 6 | The track has solid trap drums but overly busy chords and generic melodic content; tighten harmony and add a catchy lead. |
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

