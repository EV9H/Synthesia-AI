# q30 — house only

q22 baseline: house **77.5** / chords **74.9** / arrange **77.5** / drums **81.0**. Gates green. Do not retry q29 inversion locks.

| Lever | Result | Decision |
| --- | --- | --- |
| Odd-bar pad-bed skip | House **77.5 → 76.9**, chords **74.9 → 72.8**, arrange 76.0. Gates green. | **Reverted** |
| Bass octave/fifth bounce, `bassHigh` 48 | House **77.5 → 77.7**, chords **74.9** held, arrange **79.0**, bass 81.6 → 81.1. Gates = q22. | **Kept** |
| Open / drop-2 voicing (`open=true`) | Overall held 77.7, chords **74.9 → 71.4**, arrange 80.0. | **Reverted** |
| In-place 2+2 / 1+3 stab patterns | House 77.5, chords **70.3**. | **Reverted** |
| 8-bar Intro(2)+Drop(6) | House **75.3**, melody **66.2**, piano **65.8**. | **Reverted** |
| Night House seed retune (`67 4 1 5D`, density 0.58) | Inspire-only. Default Generate s7/s23 unchanged. | **Kept** |

Shipped house = bass bounce + Night House seed. Headline stays ~**77.2** (full suite not re-run). House is still structurally capped vs piano-ref `chord_vocab` / 8-bar loop entropy; rhythm and inversion levers keep hitting the chords cliff.
