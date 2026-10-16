# Sell checklist (Harmonia)

Shipping brand is **Harmonia**. The GitHub repo stays `EV9H/Synthesia-AI` until a dedicated rename. MIDI quality is the product. Preview audio is secondary.

## Do not do in a quality pass

- Do not rename the CMake target, bundle id, manufacturer code, or state XML mid-quality-sprint.
- Do not loosen in-key gates to chase the scorer.
- Do not thin pads to chase critic “space” — that crushed `register_mean` and bass-root lint in q7.

## Display name (done)

User-facing strings say Harmonia (wordmark, `getName()`, About, render-host `JucePlugin_Name`). Bundle IDs / CMake target / `PRODUCT_NAME "Synthesia AI"` / state XML / `SynthesiaAI/` user data stay until a dedicated rename. See the bundle-id checklist below.

## Remaining sell blockers (human steps)

1. **Signed installers** — you must supply certs; this repo only zips unsigned VST3.
   - Linux/Windows unsigned zip: `Tools/package_vst3.sh` after a Release VST3 build.
   - **macOS (you supply):** Apple Developer Program, Developer ID Application cert (or `.p12` + password), notarytool keychain profile. Exact commands are printed by `package_vst3.sh`. Need AU + VST3 + standalone, then staple.
   - **Windows (you supply):** Authenticode `.pfx` + password; `signtool` command is in `package_vst3.sh`.
   - Run pluginval + Steinberg validator on the **signed** release binary.

2. **Lemon Squeezy product** — stub is in; live product is not.
   - Create a Lemon product and a license-validate endpoint (or use Lemon’s API).
   - Set `HARMONIA_LICENSE_VALIDATE_URL` or paste the URL in Settings → Advanced.
   - Ship the plugin with `cmake -DHARMONIA_LICENSE_ENFORCE=1` so Generate/Inspire stay locked until a valid key.
   - Offline stub format (no secret): `HRM-XXXXXXXX-CCCC`. Mint one with `Tools/make_license_key.sh`. Legacy `SYN-` keys still pass.
   - HTTP stub POSTs `{"license_key":"..."}` and accepts 200 + `"licensed":true` / `"valid":true`. **Do not put Lemon API keys in the repo.**
   - Offline grace / no nag on MIDI export: export and drag stay unlocked even in trial. Only Generate/Inspire gate when ENFORCE=1.
   - Replace the stub with a real Lemon signed payload when the product exists.

3. **Cloudflare Pages** — product site still missing.
   - Promise: “MIDI generator for your own sounds.”
   - Download the unsigned (then signed) zip, license redeem / paste-key instructions, not a synth demo page.

4. **Real DAW drag test** — still outstanding.
   - Confirm 960 PPQ SMF, tracks Chords/Bass/Synth/Piano/Drums, GM drum channel 10, in Ableton, Logic, and FL.
   - `MidiExporter` already writes this; host test is the blocker.

5. **Manual / first-run** — 30-second “generate → drag MIDI → load your piano” tour.

6. **Bundle-id rename** — only in a dedicated release. See below.

## Bundle-id / plugin rename checklist

Do these together or DAWs will see a new plugin and lose saved instances:

1. Pick final ids: e.g. `COMPANY_NAME "Harmonia"`, `PRODUCT_NAME "Harmonia"`, target `Harmonia`, `PLUGIN_MANUFACTURER_CODE Harm`, `PLUGIN_CODE Hrmn`.
2. AU bundle id / VST3 CID: JUCE derives these from company + product + codes. Changing them registers a **new** plugin.
3. Update `CMakeLists.txt` `project()`, `juce_add_plugin()`, paths in `Tools/validate_plugin.sh` and `Tools/package_vst3.sh`.
4. Rewrite state XML tags **with a reader that still accepts `SynthesiaAIState`**.
5. Move user-data `SynthesiaAI/` → `Harmonia/` and copy presets/settings on first launch.
6. Do **not** rename the GitHub repo in the same commit unless clone URLs, CI, and Pages move too.

## Quality gates (keep)

- `out_of_key` (not a borrowed chord tone) ≤ 3%
- strong-beat chord-tone ≥ 80%
- chromatic passing ≤ 3.5%
- sampler pitch check PASS
- lint errors fail the eval run

## Inspire seeds + 960 PPQ

One-click **Inspire seed…** combo still lands genre plans. Arrangement post-pass still runs only on **All** parts. Drag export is 960 PPQ; re-test in a real DAW (blocker 4).
