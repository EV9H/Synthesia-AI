# Harmonia scoring (loop-v1)

Hard in-key gates in `lint.py` are unchanged and still fail the run:

- `out_of_key` (not a borrowed chord tone) ≤ 3%
- strong-beat chord-tone ≥ 80%
- chromatic passing ≤ 3.5%

## Why the headline moved

Through q8 the headline was an equal-weight percentile of generated 8-bar loops vs **piano-performance** MIDI refs. `chord_vocab` and `transition_entropy` are structurally capped on a 4-chord loop (p10 vocab on the refs is ~100+). Chasing them with chromatic junk is forbidden, so overall ~75 was a ceiling, not a quality reading.

loop-v1 (q10) does three things so arrangement work can move the number **without** out-of-key notes:

1. **Reweight** `chord_vocab` and `transition_entropy` to 0.25× inside each chord part score. Other ref keys stay 1.0×.
2. **Score loop craft against genre-loop targets** (not piano refs): hook restatement, call-response, density fit, groove fit, bass contour, section contrast.
3. **Blend** `0.78 * reweighted_ref + 0.22 * arrangement`, then subtract the same lint penalty.

`legacy_pro_likeness` is the old equal-weight formula and is always reported so q8 (75.4) stays comparable.

## Arrangement targets (0–100 band scores)

| Feature | What it measures | Good band |
| --- | --- | --- |
| `hook_memorability` | 2-bar synth interval contour restated 4 bars later | 0.35–0.88 |
| `call_response` | Negative bar-density correlation, lead vs piano | 0.48–0.85 |
| `section_contrast` | Relative density change, first half vs second | 0.10–0.48 |
| `bass_contour` | 4-beat windows that start low and then move | 0.40–0.95 |
| `density_fit` | Unique onset density vs genre-loop priors | per-genre |
| `groove_fit` | Kick / snare / hat density vs genre-loop priors | per-genre |

## Before / after on frozen q8 MIDI

Re-scored `eval-reports/2026-10-16-q8/clips` with loop-v1 (no generator change) to isolate the metric shift:

| | Overall |
| --- | ---: |
| q8 headline (legacy equal-weight) | **75.4** |
| same MIDI, loop-v1 | **76.7** |
| arrangement-only | 76.0 |

Delta is **+1.3** from downweighting vocab/entropy and adding loop targets — not from better notes. Trap/lofi/house chord parts were the ones the piano refs crushed. RnB dropped slightly (85.4 → 83.7) because arrangement targets are stricter than its inflated chord percentile.

New MIDI in q10 is judged on both columns: `legacy_pro_likeness` (same yardstick as 75.4) and headline `pro_likeness` (loop-v1).
