# Setup: Cloudflare Pages + Lemon Squeezy

Copy-paste checklist for going live. Human account steps are marked **YOU**.

The site lives at `sell-site/` in the **EV9H/Synthesia-AI** repo (shipping brand: Harmonia). Plugin license stub and unsigned VST3 zip already exist in that repo — wire them here.

---

## 0. Before you start

- [x] Product name: **Harmonia** (`config.js` → `productName`)
- [x] Demo MP3s in `demos/lofi.mp3`, `demos/house.mp3`, `demos/trap.mp3` (sell-q8 preview renders)
- [ ] Support email ready (or use a temp `@yourdomain` later)
- [x] Unsigned VST3 zip script: `Tools/package_vst3.sh` (from repo root)
- [x] Offline license stub: `HRM-XXXXXXXX-CCCC` via `Tools/make_license_key.sh`
- [ ] **YOU:** Lemon product + checkout URL pasted into `config.js`
- [ ] **YOU:** Signed Mac/Win installers (certs — see repo `SELL_CHECKLIST.md`)

---

## 1. Lemon Squeezy (checkout + licenses) — **YOU**

1. Create account: https://lemonsqueezy.com/
2. Complete store onboarding (payouts, tax — LS is merchant of record).
3. **Products → New product**
   - Name: Harmonia
   - Price: **$49** one-time
   - Type: Digital product / software
4. **Enable license keys** on the product (Variants → License keys).
5. Attach the download when ready: zip from `Tools/package_vst3.sh` (unsigned) then replace with signed Mac/Win builds.
6. **Share → Checkout overlay / Checkout link**
   - Copy the full checkout URL (`https://YOURSTORE.lemonsqueezy.com/checkout/buy/...`).
7. Paste into the site:

```js
// sell-site/config.js
lemonCheckoutUrl: "https://YOURSTORE.lemonsqueezy.com/checkout/buy/xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
```

8. Point the **plugin** at Lemon’s validate endpoint (no secrets in git):

```bash
# No Lemon API secret in the plugin. This public validate URL is enough.
export HARMONIA_LICENSE_VALIDATE_URL="https://api.lemonsqueezy.com/v1/licenses/validate"
```

   Or paste that URL in the plugin Settings → Advanced → Lemon validate URL.
   Exact 30-minute wiring: repo `SELL_CHECKLIST.md` → “Lemon Squeezy — finish in 30 minutes.”

9. Offline keys (dev / grace): `Tools/make_license_key.sh` prints `HRM-XXXXXXXX-CCCC`. Shipping builds: `cmake -DHARMONIA_LICENSE_ENFORCE=1` so Generate stays locked until a valid key. Export/drag stay unlocked. See `Source/LicenseStub.h`. **Do not commit Lemon API keys.**

10. Optional: Lemon.js overlay — add their script from LS docs; buttons already have `data-lemonsqueezy`.
11. Test with **test mode** checkout before going live.

**TODO left in HTML:** `#lemonCheckout` starts as `href="#"` until you paste the URL into `config.js` (script applies it).

---

## 2. Cloudflare Pages deploy — **YOU** connect the repo

### Option A — Connect GitHub (recommended)

1. This folder is already `sell-site/` on `main` of EV9H/Synthesia-AI.
2. **YOU:** Cloudflare dashboard → **Workers & Pages** → **Create** → **Pages** → **Connect to Git**.
3. Select `EV9H/Synthesia-AI`. Build settings:
   - **Framework preset:** None
   - **Build command:** *(leave empty)*
   - **Build output directory:** `sell-site`
4. Save and deploy. You get `https://<project>.pages.dev`.

### Option B — Direct upload (fastest smoke test)

```bash
cd sell-site
zip -r ../harmonia-pages.zip . -x '*.git*'
```

**YOU:** Pages → **Create** → **Upload assets**. Zip root must contain `index.html`.

### After deploy

- [ ] Open the live URL → demos play
- [ ] Buy button opens Lemon checkout (not the “URL not set” alert)
- [ ] Mobile layout OK

---

## 3. Custom domain — **YOU**

1. Cloudflare Pages → your project → **Custom domains** → Add.
2. Enter `www.yourdomain.com` (and apex if desired).
3. If the domain is already on Cloudflare DNS: follow the CNAME / proxied record prompts.
4. If the domain is elsewhere: transfer DNS to Cloudflare or add the CNAME they show at your registrar.
5. Wait for HTTPS (automatic). Update Lemon Squeezy / emails to use the real domain.
6. Set `supportEmail` in `config.js` to something on that domain.

---

## 4. Plugin packaging (repo root, not this folder)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target SynthesiaAI_VST3
Tools/package_vst3.sh   # unsigned zip in dist/
```

Mac notarizing and Windows Authenticode need **your** certs. Commands are printed by `package_vst3.sh`. Details: repo `SELL_CHECKLIST.md`.

---

## 5. Launch hygiene

- [x] Brand is Harmonia (keep Synthesia disclaimer in footer)
- [x] Preview demos are sell-q8 in-key renders (illustrative audio only)
- [x] Privacy / Terms stub pages (`privacy.html`, `terms.html`) — replace operator copy before charging
- [ ] Confirm LS receipt email includes license key + download
- [ ] Plugin activation: Lemon HTTP **or** offline `HRM-` stub (`LicenseStub`)

---

## Quick reference

| What | Where |
|------|--------|
| Brand / price / checkout URL | `sell-site/config.js` |
| Page copy & sections | `sell-site/index.html` |
| Visual design | `sell-site/styles.css` |
| Unsigned VST3 zip | repo `Tools/package_vst3.sh` |
| Offline license mint | repo `Tools/make_license_key.sh` |
| Plugin license stub | repo `Source/LicenseStub.h` |
| Remaining human blockers | repo `SELL_CHECKLIST.md` |

**Human-only:** Lemon Squeezy account + product URL in `config.js`, Cloudflare Pages connect (`output: sell-site`), domain DNS, LS payouts, Apple/Authenticode certs, optional trademark search on Harmonia.
