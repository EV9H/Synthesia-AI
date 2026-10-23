# Harmonia — install (one page)

Display name in the DAW: **Harmonia**. The VST3 folder on disk is
`Harmonia.vst3`. This zip is **unsigned** — Gatekeeper / SmartScreen will
warn. That is expected, not malware.

## Unzip

You should see a `Harmonia/` folder:

- `README.txt` — this is Harmonia
- `Harmonia.vst3` — the plugin
- `GETTING_STARTED.txt` — 30-second generate → drag
- `UNSIGNED_SMOKE.txt` — Gatekeeper / SmartScreen
- `LICENSE.txt` — trial vs HRM- key
- `INSTALL.md` — this page

This unsigned zip is a **trial**: Generate and Drag MIDI work with no key
and no timer. A paid key is optional until Lemon checkout exists.

## macOS (Gatekeeper)

1. Right-click `Harmonia.vst3` → **Open**. Confirm the dialog.
   Or: System Settings → Privacy & Security → **Open Anyway**.
2. Copy `Harmonia.vst3` to `~/Library/Audio/Plug-Ins/VST3/`
   (create the folder if it is missing).
3. Rescan plugins in your DAW. Look for **Harmonia**.

## Windows (SmartScreen)

1. If SmartScreen appears: **More info → Run anyway**.
2. Copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\`
3. Rescan. Look for **Harmonia**.

## Where to drop the .vst3 (per DAW)

| DAW | VST3 folder | Notes |
| --- | --- | --- |
| Ableton Live | macOS: `~/Library/Audio/Plug-Ins/VST3/` · Win: `C:\Program Files\Common Files\VST3\` | Preferences → Plug-Ins → Rescan. Insert on a MIDI track. |
| FL Studio | Same system VST3 folders | Options → Manage plugins → Find / Scan. |
| Reaper | Same, or Options → Preferences → VST → path | Scan for new. |
| Cubase | Same system VST3 folders | Studio → VST Plug-in Manager → Rescan. |
| Studio One | Same system VST3 folders | Studio One → Options → Locations → Rescan. |
| Logic Pro | No native VST3 | Use a VST3 wrapper, or wait for AU. Do not expect AU in this zip. |

Then: MIDI / instrument track → insert **Harmonia** → tap **Generate** → drag
**Drag MIDI** (not the piano-roll) into the DAW. Five tracks:
Chords ch 1, Bass ch 2, Synth ch 3, Piano ch 4, Drums ch 10 (GM kit).
Load YOUR sounds. Preview piano is a sketch.

Need a key? Tap Generate — Settings opens. Paste `HRM-XXXXXXXX-CCCC` (or Paste).
Drag MIDI works before a key. Full path: `GETTING_STARTED.txt`.
