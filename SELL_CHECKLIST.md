# Sell checklist (Harmonia)

Shipping brand is **Harmonia**. The GitHub repo stays `EV9H/Synthesia-AI` until a dedicated rename. MIDI quality is the product. Preview audio is secondary.

## Do not do in a quality pass

- Do not rename the CMake target, bundle id, manufacturer code, or state XML mid-quality-sprint.
- Do not loosen in-key gates to chase the scorer.
- Do not thin pads to chase critic “space” — that crushed `register_mean` and bass-root lint in q7.

## In-repo this round

- Marketing site: **`sell-site/`** (Cloudflare Pages static). Demos are sell-q8 lofi/house/trap MP3s.
- Unsigned VST3 zip: `Tools/package_vst3.sh`
- Offline license mint: `Tools/make_license_key.sh` → writes `HRM-XXXXXXXX-CCCC`. A local sample may exist at `Tools/sample_license.txt` (**gitignored**). Do not commit Lemon/production secrets.
- Plugin stub: Settings → License key + Validate. Shipping gate: `cmake -DHARMONIA_LICENSE_ENFORCE=1`

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

3. **Signed installers** — you must supply certs.
   - Linux/Windows unsigned zip: `Tools/package_vst3.sh`
   - **macOS:** Developer ID Application + notarytool profile (commands in `package_vst3.sh`)
   - **Windows:** Authenticode `.pfx` + `signtool` (same script)
   - pluginval + Steinberg on the **signed** binary

4. **Real DAW drag test** — 960 PPQ, tracks Chords/Bass/Synth/Piano/Drums, GM drum ch 10, in Ableton / Logic / FL.

5. **Manual / first-run** — 30-second “generate → drag MIDI → load your piano” tour.

6. **Bundle-id rename** — only in a dedicated release (see below).

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

No MIDI change. House/trap `chord_vocab` / entropy stay structurally capped vs piano-performance refs (8-bar loops cannot hit p10 vocab ~100+). Reuse sell-q8 demos as `sell-q9-*` artifacts. Last scored eval: **75.4**, gates green (`2026-10-16-q8`).
