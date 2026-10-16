# sell-q10 — discovered requirements vs shipped

## Discovered (this round)

1. **The 75.4 ceiling was a scoring lie.** 8-bar in-key loops cannot hit piano-performance `chord_vocab` / entropy. Buyers will not hear “75.” They will hear whether the loop is genre-true and drag-ready. Ship an honest loop metric or stop quoting overall as quality.
2. **Exact hook copies look like quality and score like failure.** Restating a 4-bar cell 1:1 trips `repeat_4` / `variation_8` vs piano refs (cinematic melody 43.8 on the first q10 pass). Restate with a varied ending.
3. **Refund trigger: “this is a cheap synth.”** Preview audio is a sketch. First-run must say generate → drag MIDI → load *your* piano. “Drag all” was unclear.
4. **Refund trigger: Generate dead on first launch.** `HARMONIA_LICENSE_ENFORCE=1` without an obvious HRM- paste path. Tour now warns when enforce is on.
5. **Refund trigger: unsigned VST3.** Gatekeeper / SmartScreen. Human still owns certs.
6. **Inspire seeds must actually Generate.** Combo `onChange` already does. Keep one seed per genre; a silent license-block is a refund.
7. **House hats cannot be a straight 8th grid.** Four-on-the-floor kick is right; regular 8ths killed `hat_ioi_cv` / syncopation (drums 65.1). Irregular 16ths stay.

## Shipped

| Item | Status |
| --- | --- |
| loop-v1 scoring + legacy column | yes — `Tools/eval/SCORING.md` |
| Frozen q8 MIDI on loop-v1 | 75.4 legacy / **76.7** loop-v1 |
| q10 MIDI on loop-v1 | **76.3** headline, **75.9** legacy, arrange 75.9 |
| In-key gates | green (out-of-key ≤ 0.4%, strong chord-tone ≥ 94.7%, chromatic ≤ 1.7%, pitch PASS, lint 0) |
| Genre drum templates | pop/lofi/trap/rnb ghosts; house 4otf + irregular hats; trap stutter |
| Hook head skip-guard | yes (exact A/B/A/B copies walked back) |
| Bass approach every 4 bars | yes |
| Chorus/drop crash, verse hat thinning | yes |
| 16-bar song-form lead thinning | yes (eval still 8-bar loop) |
| First-run tour + empty-state generate→drag | yes |
| Inspire seeds still generate | yes |
| Six-genre sell-q10 mp3/mid | `/opt/cursor/artifacts/sell-q10-*_s7.{mp3,mid}` |
| SELL_CHECKLIST | updated with refund risks |

## Still human

Cloudflare Pages connect, Lemon checkout URL, signed certs, real DAW drag, checkout-email copy that says “MIDI for your sounds.”
