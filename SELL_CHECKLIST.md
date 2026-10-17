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
- Zip includes `INSTALL.md`, `GETTING_STARTED.txt`, `UNSIGNED_SMOKE.txt`, and `LICENSE.txt` inside a `Harmonia/` folder (`Tools/package_vst3.sh`).
- MIDI export smoke: `Tools/verify_midi_export.py` (960 PPQ, named parts, drums ch 10).
- Inspire factory seeds: combo still auto-generates. Keep at least one seed per shipping genre.
- Eval honesty: headline is `loop-v1` (legacy column still reported so q8 **75.4** stays comparable).

## Remaining sell blockers (human steps — this is what still blocks taking money)

1. **Cloudflare Pages is not live.** Site files are in `sell-site/` (Privacy, Terms, Download section). **YOU** still connect Git.
   - CF → Workers & Pages → Create → Connect Git → `EV9H/Synthesia-AI`
   - Framework: None. Build command: empty. Output directory: **`sell-site`**
   - Custom domain, then set `supportEmail` in `sell-site/config.js` (placeholder `support@harmonia.todo` — not a live inbox)

2. **Lemon Squeezy product does not exist.** Checkout button is `href="#"` until you paste a URL. Follow **Lemon Squeezy — finish in 30 minutes** below. Payouts / tax onboarding is on you. **Do not put Lemon API keys in the repo.**

3. **Installers are unsigned.** `Tools/package_vst3.sh` writes a buyer-ready *unsigned* zip. Gatekeeper / SmartScreen will scare buyers — #1 malware-refund risk until you sign (commands printed by the script).

4. **No human DAW drag proof.** Automated check: `Tools/verify_midi_export.py` (960 PPQ, named tracks, drums ch 10). **YOU** still drop Drag MIDI into Ableton / FL / Reaper and load a stock piano.

5. **Receipt email does not exist.** Lemon will send it. The body must say: MIDI for your sounds; preview is a sketch; paste the key (or tap Generate → Settings); unsigned warning until signed.

6. **Privacy / Terms are stubs.** Pages exist and are linked. They still say “replace operator name/address before charging.” Have counsel (or at least your real legal name) in those files.

7. **Bundle still says Synthesia AI.** Display name is Harmonia. Dedicated rename only (see below) — not a launch blocker if the landing page is clear.

8. **Inspire seeds** — combo still auto-generates. Smoke one seed per genre on an `ENFORCE=1` build after the key path works.

## Lemon Squeezy — finish in 30 minutes

No secrets in git. Two compatible key paths: **Lemon HTTP** (buyer pastes the Lemon key) and **offline HRM-** (you mint with `Tools/make_license_key.sh`). The plugin tries offline first, then HTTP.

### A. Product + checkout (site) — ~10 min

1. https://lemonsqueezy.com/ → create store → finish payouts/tax (MoR).
2. Products → New product
   - Name: **Harmonia**
   - Price: **$49** one-time
   - Type: digital / software
3. Variants → enable **License keys** (Lemon will email its own key format).
4. Files → attach `dist/Harmonia-*-vst3-unsigned-*.zip` from `Tools/package_vst3.sh` (replace later with signed Mac/Win zips).
5. Share → copy the checkout URL (`https://YOURSTORE.lemonsqueezy.com/checkout/buy/...`).
6. Paste into `sell-site/config.js`:

```js
lemonCheckoutUrl: "https://YOURSTORE.lemonsqueezy.com/checkout/buy/xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
supportEmail: "you@yourdomain.com",
```

7. Commit that one file (or edit it only on Pages if you prefer not to commit the live URL). Reload the site — Buy must open Lemon, not the “URL not set” alert.

### B. Plugin validate URL — ~5 min

Lemon’s public validate endpoint (no API key in the binary):

```
https://api.lemonsqueezy.com/v1/licenses/validate
```

The stub POSTs `license_key=...` (`application/x-www-form-urlencoded`) and treats HTTP 200 plus `"valid":true` / `"status":"active"` as licensed.

Ship it as a compile default **or** tell buyers to ignore it and use an offline `HRM-` key you email:

```bash
# CMake (your release machine) — still no Lemon secret
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHARMONIA_LICENSE_ENFORCE=1
# Optional: bake the validate URL into docs only; runtime override:
#   env HARMONIA_LICENSE_VALIDATE_URL=https://api.lemonsqueezy.com/v1/licenses/validate
#   or Settings → Advanced → Lemon validate URL
```

Offline grace is unchanged: **Drag MIDI / export never lock.** Generate locks only when `HARMONIA_LICENSE_ENFORCE=1`.

### C. Receipt copy — ~5 min

Lemon → Settings → Emails. Receipt / license mail must include:

- “Harmonia writes MIDI. Preview piano is a sketch. Load yours.”
- The license key and the zip
- “Tap Generate → Settings → paste the key (or Paste). Drag MIDI works before that.”
- “macOS/Windows may warn on an unsigned build. See UNSIGNED_SMOKE.txt.”

### D. Webhook (optional — skip if Lemon emails keys)

Only if you want to mint **your** `HRM-` keys on each sale instead of Lemon’s:

1. Lemon → Settings → Webhooks → URL on **your** server (not this repo).
2. Events: `order_created` and/or `license_key_created`.
3. Signing secret stays on that server. **Never commit it.**
4. Handler: verify signature → run `Tools/make_license_key.sh` → email `HRM-XXXXXXXX-CCCC` to the buyer.
5. Plugin then unlocks offline with no HTTP.

You do **not** need a webhook to take money. Path A+B is enough.

### E. 5-minute smoke after A–C

1. Test-mode checkout → you receive a key + zip.
2. Unzip `Harmonia/` → `INSTALL.md`, `GETTING_STARTED.txt`, `UNSIGNED_SMOKE.txt`, `LICENSE.txt`, `Synthesia AI.vst3`.
3. Load in a DAW (or Standalone) → tap Generate → paste key → Validate.
4. Drag MIDI → confirm five tracks and drums on ch 10. Or: `Tools/verify_midi_export.py some.mid`

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

## sell-q20

Tried cinematic one-ghost-per-backbeat snares. Drums **73.0 → 74.8** but arrange **77.0 → 75.4** and cinematic **74.2 → 74.1** — walked back (do not retry sparse cinematic ghosts). MIDI = q17/q18/q19. Headline **77.0**, arrange **78.4**, gates green. Product: visible license paste + sanitise/auto-Validate; `UNSIGNED_SMOKE.txt` in the VST3 zip; first-run status “Generate then drag.” Artifacts: `sell-q20-*_s7.{mp3,mid}`.

## sell-q21

Tried pop broken piano 102/74/46 (not the banned 108/32 stack). Piano **69.2 → 66.9** (vel_std 24 → 28, score 57 → 45). Pop 79.5 → 79.2. Walked back — refs p90 is 19.9, so 24 is already high. MIDI = q17. Headline **77.0**. Artifacts: `sell-q21-*_s7.{mp3,mid}`.

## sell-q22

Pop broken piano compressed to 92/74/56 with humanise-only (no downbeat accents). Piano **69.2 → 76.6** (vel_std 24 → 15, score 57 → 98). Pop **79.5 → 80.5**. Headline **77.2** loop-v1, legacy **76.5**, arrange **78.3**. Gates green. Other genres held. Artifacts: `sell-q22-*_s7.{mp3,mid}`.

## sell-q23

Tried lo-fi lead onset-fill (density 0.60, keep weak notes, drop the second-bar skip). Melody **66.6 → 72.1** but arrange **77.3 → 73.5**; genre held 73.3, headline held 77.2. Walked back — loop synth band 0.70±0.40 vs piano-ref p10 1.74. MIDI = q22. Headline **77.2**. Artifacts: `sell-q23-*_s7.{mp3,mid}`.

## sell-q24

Tried cinematic anticipated backbeats (same snare count, not ghosts). Sync rose but swing scored 8.0; drums **73.0 → 70.3**, cinematic **74.2 → 73.7**, headline **77.2 → 77.1**. Walked back. MIDI = q22. Headline **77.2**. Artifacts: `sell-q24-*_s7.{mp3,mid}`.

## sell-q25

MIDI stalled (lofi onset-fill + cinematic anticipate both walked back). Product: Generate / Inspire stay clickable on paid builds and open Settings; license Paste-from-clipboard; tour + GETTING_STARTED + FAQ copy. MIDI = q22. Headline **77.2**, arrange **78.3**, gates green. Artifacts: `sell-q25-*_s7.{mp3,mid}`.

## sell-q26

Productize (no MIDI). Sell-site: `privacy.html` + `terms.html` stubs, Download/requirements section (VST3, DAW list), pricing FAQs ($49 / seats / refunds). Footer links. MIDI = q22. Headline **77.2**. Artifacts: `sell-q26-*` (q22 demos).

## sell-q27

Buyer zip: `package_vst3.sh` now stages `Harmonia/{Synthesia AI.vst3,GETTING_STARTED.txt,UNSIGNED_SMOKE.txt,LICENSE.txt}`. Added `Tools/LICENSE_NOTE.txt` and `Tools/verify_midi_export.py` (960 PPQ, named Chords/Bass/Synth/Piano/Drums, drums ch 10). MIDI = q22.

## sell-q28

Lemon “finish in 30 minutes” wired in this file (checkout URL, `https://api.lemonsqueezy.com/v1/licenses/validate`, optional webhook). Stub POSTs form-encoded `license_key` and accepts `"valid":true` / `"status":"active"`. First-run tour is a 4-step checklist; empty state names the five tracks + ch 10. MIDI = q22. Headline **77.2**, arrange **78.3**, gates green. Artifacts: `sell-q28-*`.

## sell-q29

Productize (no full-score chase). Zip docs: one-page `INSTALL.md` (Gatekeeper / SmartScreen + Live/FL/Reaper/Cubase/Studio One / Logic-via-wrapper) always copied next to the plugin. Tour CTA is “Generate, then drag MIDI”; empty state names five tracks + ch 10. Sell-site hero/FAQ: MIDI for YOUR DAW sounds, not a synth; unsigned warning; $49 one-time; refunds via Lemon once live. Support placeholder `support@harmonia.todo` (not a live inbox). Privacy/Terms stay stubs with an “Operator details TBA before launch” banner.

House MIDI add-on (user: house too blocky): tried smooth 1st/2nd inversions (per-bar lock, then per-hit VL pick). Gates stayed green, but house **77.5 → 76.1 / 75.3** and chords **74.9 → 63.8**. **Walked back** — do not retry forced house inversions. Landed instead: Inspire house seeds Night / **Warehouse** (seed 23, D minor) / **Rolling** (seed 41, F minor) / Anthem; factory house seeds set `grooveIndex` from the seed. Shipped house MIDI = q22. Headline **77.2**, arrange **78.3**.

### Code-side done vs human-blocked

| Item | Status |
| --- | --- |
| Buyer zip `Harmonia/{plugin, INSTALL.md, GETTING_STARTED, UNSIGNED_SMOKE, LICENSE}` | **Code done** |
| Per-DAW VST3 drop paths + Gatekeeper / SmartScreen copy | **Code done** |
| First-run 4-step tour + empty-state five tracks / ch 10 + Generate-then-drag CTA | **Code done** |
| License paste UI; offline `HRM-` works; Lemon validate optional | **Code done** |
| Sell-site hero/FAQ (not a synth, $49, unsigned, Lemon refunds) | **Code done** (not deployed) |
| Support email | **Human** — `support@harmonia.todo` is a marked placeholder |
| Privacy / Terms operator name + address | **Human** — stubs + TBA banner only |
| Cloudflare Pages connect Git / custom domain | **Human-blocked** |
| Lemon product + checkout URL in `config.js` | **Human-blocked** |
| Lemon receipt copy | **Human-blocked** |
| Apple / Authenticode signing | **Human-blocked** |
| Real DAW drag smoke (Live / FL / Reaper) | **Human-blocked** (`verify_midi_export.py` is the automated stand-in) |
| Bundle rename Synthesia AI → Harmonia | Not a launch blocker (display name is Harmonia) |

Human 30-min path (unchanged, still required to take money):

1. Cloudflare Pages → Connect Git → output `sell-site`
2. Lemon product $49 one-time + license keys → paste checkout URL into `config.js`
3. Replace `support@harmonia.todo` with a real inbox
4. Operator legal name/address on Privacy + Terms
5. Sign Mac/Win builds (commands in `package_vst3.sh`)
6. DAW smoke: right-click Open / SmartScreen → rescan → Generate → drag MIDI → load YOUR sounds

## Next loop (q30+) if unblocked

Do not chase banned MIDI levers. Remaining pure-code productize is thin (preset names, drag-handle affordance). Default: **code-side sellable; waiting on human blockers** above. MIDI headline remains **77.2** unless the house-only eval moved house.
