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

Packaging/refund copy: `GETTING_STARTED.txt` inside the VST3 zip; license line states drag MIDI always works; first-run tour can open Settings. MIDI unchanged from q11 (76.8).
