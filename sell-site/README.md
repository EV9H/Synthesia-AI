# Harmonia landing page

Static site for **Harmonia** — AI MIDI VST3 generator. Deploy on Cloudflare Pages; checkout via Lemon Squeezy ($49).

## Files

| Path | Role |
|------|------|
| `index.html` | Landing page |
| `styles.css` / `script.js` | Layout + brand/checkout wiring |
| `config.js` | Product name, price, Lemon checkout URL, support email |
| `demos/` | sell-q8 preview MP3s (`lofi.mp3`, `house.mp3`, `trap.mp3`) |
| `SETUP.md` | Cloudflare Pages + Lemon Squeezy + plugin stub/packaging |
| `_headers` | Cloudflare Pages security headers |

## Rename (only if you change brand later)

1. Edit `config.js` → `productName`
2. Update `<title>` / meta in `index.html` if needed
3. `rg -n 'Harmonia' .` and replace leftovers

Do **not** ship as “Synthesia” — third-party trademark. This product is not affiliated with Synthesia Learning Co.

## Local preview

Open `index.html` in a browser, or:

```bash
cd /workspace/sell-site && python3 -m http.server 8787
```
