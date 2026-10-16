# Sell checklist (Harmonia)

Shipping brand is **Harmonia**. The GitHub repo stays `EV9H/Synthesia-AI` until a dedicated rename. MIDI quality is the product. Preview audio is secondary.

## Do not do in a quality pass

- Do not rename the CMake target, bundle id, manufacturer code, or state XML mid-quality-sprint.
- Do not loosen in-key gates to chase the scorer.
- Do not thin pads to chase critic “space” — that crushed `register_mean` and bass-root lint in q7.
- Do not treat piano-ref `chord_vocab` / entropy as a quality target. loop-v1 downweights them; see `Tools/eval/SCORING.md`.

## In-repo this round (q11)

- Marketing site: **`sell-site/`** (Cloudflare Pages static). Demos are sell-q8 lofi/house/trap MP3s; q10 six-genre MP3s live in eval-reports + artifacts.
- Unsigned VST3 zip: `Tools/package_vst3.sh`
- Offline license mint: `Tools/make_license_key.sh` → writes `HRM-XXXXXXXX-CCCC`. A local sample may exist at `Tools/sample_license.txt` (**gitignored**). Do not commit Lemon/production secrets.
- Plugin stub: Settings → License key + Validate. Shipping gate: `cmake -DHARMONIA_LICENSE_ENFORCE=1`
- First-run tour + empty-state: generate → drag MIDI into the DAW. Preview is labelled a sketch. Paid builds: Open Settings from the tour; drag still works without a key.
- Zip includes `Tools/GETTING_STARTED.txt` (unsigned-build warning + generate→drag).
- Inspire factory seeds: combo still auto-generates. Keep at least one seed per shipping genre.
- Eval honesty: headline is `loop-v1` (legacy column still reported so q8 **75.4** stays comparable).

## Remaining sell blockers (human steps)

1. **Cloudflare Pages** — site is in-repo; **YOU** still connect it.
   - CF → Workers & Pages → Create → Connect Git → `EV9H/Synthesia-AI`
   - Build output directory: **`sell-site`** (no build command)
   - Then custom domain + `supportEmail` in `sell-site/config.js`

2. **Lemon Squeezy product URL** — stub is in; live product is not.
   - Create the $49 product, enable license keys, copy checkout URL
   - Paste into `sell-site/config.js` → `lemonCheckoutUrl`
   - Set plugin `HARMONIA_LICENSE_VALIDATE_URL` or Settings → Advanced URL
   - **Do not put Lemon API keys in the repo**
   - Offline grace: export/drag stay unlocked; Generate locks only when ENFORCE=1
   - **Refund risk:** if you ship `HARMONIA_LICENSE_ENFORCE=1` without the buyer’s key paste path being obvious, Generate is dead on first launch. The first-run tour now tells them Settings → paste `HRM-` when enforce is on. Confirm that copy against the real checkout email.

3. **Signed installers** — you must supply certs.
   - Linux/Windows unsigned zip: `Tools/package_vst3.sh`
   - **macOS:** Developer ID Application + notarytool profile (commands in `package_vst3.sh`)
   - **Windows:** Authenticode `.pfx` + `signtool` (same script)
   - pluginval + Steinberg on the **signed** binary
   - **Refund risk:** unsigned VST3 trips Gatekeeper / SmartScreen. Buyers will think the plugin is malware.

4. **Real DAW drag test** — 960 PPQ, tracks Chords/Bass/Synth/Piano/Drums, GM drum ch 10, in Ableton / Logic / FL.
   - Confirm “Drag MIDI” (not the piano-roll preview) is what lands in the DAW.
   - Confirm a first-time user finds the handle without the tour (tour persists `firstRunTourSeen` in `SynthesiaAI/settings.xml`).

5. **Manual / first-run** — tour is in-plugin. Still do a 30-second human pass: generate → drag MIDI → load a stock piano. If that path is unclear, people refund because they think Harmonia is a cheap synth.

6. **Bundle-id rename** — only in a dedicated release (see below).

7. **Support / refund copy (discovered)** — landing + receipt must say “MIDI for your sounds; preview audio is a sketch.” Cheap preview vs the buyer’s piano is the #1 “this is a scam” trigger. Site hero already says it; checkout email does not exist yet.

8. **Inspire seeds still generate** — combo `onChange` fires `inspireIdeaFromSeed` + Generate. If a seed fails silently after a license gate, that is a refund. Smoke one seed per genre before a paid build.

## Bundle-id / plugin rename checklist

Do these together or DAWs will see a new plugin and lose saved instances:

1. Pick final ids: e.g. `COMPANY_NAME "Harmonia"`, `PRODUCT_NAME "Harmonia"`, target `Harmonia`, `PLUGIN_MANUFACTURER_CODE Harm`, `PLUGIN_CODE Hrmn`.
2. AU bundle id / VST3 CID change registers a **new** plugin.
3. Update `CMakeLists.txt`, `Tools/validate_plugin.sh`, `Tools/package_vst3.sh`.
4. State XML reader must still accept `SynthesiaAIState`.
5. Move user-data `SynthesiaAI/` → `Harmonia/` on first launch.
6. Do **not** rename the GitHub repo in the same commit unless clone URLs, CI, and Pages move too.

## Quality gates (keep)

- `out_of_key` (not a borrowed chord tone) ≤ 3%
- strong-beat chord-tone ≥ 80%
- chromatic passing ≤ 3.5%
- sampler pitch check PASS
- lint errors fail the eval run

## sell-q9

No MIDI change. House/trap `chord_vocab` / entropy stay structurally capped vs piano-performance refs (8-bar loops cannot hit p10 vocab ~100+). Reuse sell-q8 demos as `sell-q9-*` artifacts. Last scored eval under the **legacy** metric: **75.4**, gates green (`2026-10-16-q8`).

## sell-q10

Shipped: loop-v1 scoring (`Tools/eval/SCORING.md`), genre-true drum templates (house hats kept irregular after a too-grid pass), hook-head skip guard, bass approach every 4 bars, chorus crash / verse-hat thinning, first-run tour, empty-state generate→drag copy. Hard gates green. Headline **76.3** loop-v1 (frozen q8 MIDI on loop-v1 was 76.7). Legacy **75.9** vs q8 **75.4**. Artifacts: `sell-q10-{lofi,house,trap,pop,rnb,cinematic}_s7.{mp3,mid}`.

## sell-q11

Pop broken piano raised to 62–74 (piano 61.8 → **69.2**, pop **79.5**). Trap 2-bar/sustain/fold stack failed (melody 52.6) and was walked back; trap-first-phrase skip-protect recovered melody **63.2 → 69.2**. Headline **76.8** loop-v1, legacy **76.3** (beats q8 75.4). Gates green. Artifacts: `sell-q11-*_s7.{mp3,mid}`.

## sell-q12

Packaging/refund copy: `GETTING_STARTED.txt` inside the VST3 zip; license line states drag MIDI always works; first-run tour can open Settings. MIDI unchanged from q11 (76.8). Artifacts copied as `sell-q12-*`.

## sell-q13

House/lo-fi chorus crash skipped (house drums 71.3 → **71.8**, matching q8). Cinematic piano arp 62–76 (piano 67.0 → **71.6**). Headline **76.8** loop-v1, legacy **76.5**. Gates green. Artifacts: `sell-q13-*_s7.{mp3,mid}`.

## sell-q14

Lo-fi arrangement: complementary piano (not unison comps), verse-only-X, hook restate on contour A (not inverted B), sustained two-hit pads, one inner-voice step. Arrange **68.3 → 77.4**. Chords **64.8 → 64.2** (held; short stabs walked back — bass_root error). Piano 81.7 → **84.6**, bass 75.8 → **80.9**, melody 69.3 → 66.6. Headline **76.8** loop-v1, legacy **76.2**. Hard gates green (two bass_root *warns*, no errors). Other genres unchanged. Artifacts: `sell-q14-*_s7.{mp3,mid}`.

## sell-q15

Cinematic: one pad stack per chord (not per-beat retrigger), ghost-free snares, one inner-voice step. Arrange **69.8 → 77.0**. Overall 74.0 → **74.2**. Chords 72.7 → 66.1, drums 83.6 → 73.0 (loop groove vs piano-ref trade), melody 70.5 → **76.8**, piano held 71.6 (verse arp skips walked back). Headline **76.8** loop-v1, legacy **75.9**, arrange **78.3**. Gates green. Artifacts: `sell-q15-*_s7.{mp3,mid}`.

## sell-q16

Product friction (headline stuck at 76.8; arrange already 78.3). GETTING_STARTED: greyed-out Generate → Settings paste path; Drag MIDI handle named. First-run tour + Generate tooltip say drag always works. Sell-site: license/Gatekeeper FAQs, “drag before key” on the price card, demos refreshed to q15 lofi/house/trap. MIDI unchanged. Artifacts copied as `sell-q16-*`.

## sell-q17

House drums: irregular 16th off-beat hats (not 8th-grid), leftover fill-path chorus crash skipped, off-beat fill claps. Drums **71.8 → 81.0**. House **75.9 → 77.5**. Headline **77.0** loop-v1, legacy **76.2**, arrange **78.4**. Gates green. Other genres held (lofi drums 83.7 from the shared crash skip). Artifacts: `sell-q17-*_s7.{mp3,mid}`.

## sell-q18

Tried lofi melody (syncopate restated A, fold 60–69) — melody **66.6 → 56.3**, walked back. Tried pop piano velocity 108–32 — piano **69.2 → 61.2** (vel_std 24 → 34), walked back. MIDI matches q17. Headline **77.0**, house drums **81.0**, gates green. Artifacts: `sell-q18-*_s7.{mp3,mid}`.

## sell-q19

Tried lo-fi bass_root (snap approaches; force pad root after invert). Lint stayed 38–42% and lofi dipped 73.3 → 72.8 — walked back. Product: GETTING_STARTED names GM channels (1–4 + ch 10 drums); sell-site FAQ “preview piano is a sketch.” MIDI = q17/q18. Headline **77.0**. Artifacts: `sell-q19-*_s7.{mp3,mid}`.

## Next loop (q20+) if unblocked

MIDI: lofi melody 66.6, pop piano 69.2, cinematic drums 73.0, lofi bass_root warns (do not chase the same snap/root-insert). Do not retouch lofi lead rhythm A / fold-69. Do not widen pop piano to 32–108. Trap vocab / trap 2-bar-sustain / short lofi holds / per-beat cinematic pads / house 8th hats still forbidden. Human-only: CF Pages, Lemon URL, certs, real DAW drag.
