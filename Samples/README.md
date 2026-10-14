# Preview drum and instrument samples

Preview-only samples for the built-in synth. Users are expected to play the
generated MIDI with **their own sounds in the DAW**. Processed to **44.1 kHz,
16-bit, stereo**. Total pack is about **32 MB**.

Keep **piano, EP, bass guitar, and one drum kit per genre**. Orchestra, organ,
and guitar layers were dropped to stay near 30 MB. `InstrumentLibrary` picks
the nearest root and a soft/hard velocity layer; piano also fires a release
sample on note-off.

Every source below is **CC0 1.0** or **CC-BY-3.0** (attribution in this file).
CC-BY-NC packs such as jRhodes3d are **not** shipped.

## Kits

| Genre | Character | Sources |
| --- | --- | --- |
| House | Punchy TR-8 **909** | MckAudio TR8 |
| Trap | Analog **808** (Fischer) | tidalcycles/sounds-tr808-fischer |
| Lo-fi | Dusty acoustic (soft/hard layers) | Sonic Pi / Freesound CC0 |
| Pop | Tight acoustic + short hats | Sonic Pi / Freesound CC0 |
| R&B | Jazz kick, soul snare, warm hats | Sonic Pi / Freesound CC0 |
| Cinematic | Boom kick, splash, toms, bell | Sonic Pi / Freesound CC0 |

## Sources and licenses

### MckAudio TR8 (House)

Hardware recordings of a Roland TR-8, released as CC0.

- Pack: https://github.com/MckAudio/MckSamplePacks
- License: https://github.com/MckAudio/MckSamplePacks/blob/main/LICENSE (CC0 1.0)

| File | Original |
| --- | --- |
| `house_kick.wav` | `TR8/BD/004_909_Bass_Drum_1.wav` |
| `house_snare.wav` | `TR8/SD/005_909_Snare_Drum_1.wav` |
| `house_clap.wav` | `TR8/PERC/019_909_Hand_Clap.wav` |
| `house_ch.wav` | `TR8/HATS/008_909_Closed_HiHat.wav` |
| `house_ch_rr1.wav` | `TR8/HATS/010_909_Closed_HiHat_Short.wav` |
| `house_ch_rr2.wav` | `TR8/HATS/012_707_Closed_HiHat.wav` |
| `house_oh.wav` | `TR8/HATS/007_909_Open_HiHat.wav` |
| `house_oh_rr1.wav` | `TR8/HATS/009_909_Open_HiHat_Short.wav` |
| `house_pedal.wav` | `TR8/HATS/010_909_Closed_HiHat_Short.wav` (short fade) |
| `house_tom.wav` | `TR8/TOMS/010_909_Low_Tom.wav` |
| `house_crash.wav` | `TR8/HATS/020_909_Crash_Cymbal.wav` |
| `house_ride.wav` | `TR8/HATS/026_909_Ride_Cymbal.wav` |
| `house_perc.wav` | `TR8/PERC/036_707_Tambourine.wav` |

### Michael Fischer TR-808 (Trap)

1994 analog TR-808 one-shots by Michael Fischer (Technopolis), “absolutely
free.” Redistributed by TidalCycles under CC0 1.0.

- Pack: https://github.com/tidalcycles/sounds-tr808-fischer
- License: https://github.com/tidalcycles/sounds-tr808-fischer/blob/main/LICENSE (CC0 1.0)
- Original notes: https://github.com/tidalcycles/sounds-tr808-fischer/blob/main/README.md

| File | Original |
| --- | --- |
| `trap_kick.wav` | `bd8/BD5050.WAV` |
| `trap_snare.wav` | `sd8/SD5050.WAV` |
| `trap_clap.wav` | `cp8/CP.WAV` |
| `trap_ch.wav` | `ch8/CH.WAV` |
| `trap_ch_rr1.wav` | MckAudio `TR8/HATS/002_808_Closed_HiHat.wav` |
| `trap_ch_rr2.wav` | MckAudio `TR8/HATS/004_808_Closed_HiHat_Short.wav` |
| `trap_oh.wav` | `oh8/OH50.WAV` |
| `trap_oh_rr1.wav` | `oh8/OH25.WAV` |
| `trap_oh_rr2.wav` | `oh8/OH10.WAV` |
| `trap_pedal.wav` | MckAudio `TR8/HATS/004_808_Closed_HiHat_Short.wav` |
| `trap_tom.wav` | `lt8/LT50.WAV` |
| `trap_crash.wav` | `cy8/CY5050.WAV` |
| `trap_perc.wav` | `rs8/RS.WAV` |

### Sonic Pi / Freesound (Lo-fi, Pop, R&B, Cinematic)

All other samples come from Sonic Pi’s factory library, themselves CC0
Freesound recordings (or CC0 donations). Sonic Pi documents the mapping at:

- https://github.com/sonic-pi-net/sonic-pi/blob/main/etc/samples/README.md
- License: https://creativecommons.org/publicdomain/zero/1.0/

Lo-fi (`lofi_*`): `drum_bass_{soft,hard}`, `drum_snare_{soft,hard}`,
`perc_snap`, `hat_gnu` (closed), `hat_tap` / `hat_psych` (closed RR),
`drum_cymbal_open` / `hat_zan` (open), `drum_cymbal_pedal`, `drum_tom_lo_soft`,
`drum_splash_soft`, `ride_via` (https://freesound.org/people/trivialAccapella/sounds/425448/),
`perc_snap2`.

Pop (`pop_*`): `drum_heavy_kick` (https://freesound.org/people/Zajo/sounds/4832/),
`bd_haus` (https://freesound.org/people/Rodrigo%20The%20Mad/sounds/137722/),
`sn_dolf` (https://freesound.org/people/Dolfeus/sounds/57534/),
`drum_snare_hard`, `perc_snap2`, `hat_bdu` (https://freesound.org/people/bdu/sounds/802/),
`hat_zild` (https://freesound.org/people/pjcohen/sounds/45668/),
`drum_tom_mid_hard`, `drum_cymbal_hard`, `drum_cowbell`.

R&B (`rnb_*`): `bd_jazz` (https://freesound.org/people/tripjazz/sounds/511139/),
`drum_bass_hard`, `sn_zome` (https://freesound.org/people/Dolfeus/sounds/55232/),
`sn_dolf`, `hat_tap`, `hat_psych`, `hat_zan`, `drum_tom_lo_hard`,
`drum_cymbal_soft`, `drum_cymbal_pedal`.

Cinematic (`cine_*`): `drum_bass_hard`, `bd_boom` (https://freesound.org/people/Snapper4298/sounds/157245/),
`sn_generic`, `drum_snare_hard`, `perc_bell`, `hat_metal`, `drum_cymbal_open`,
`drum_tom_lo_hard`, `drum_splash_hard`, `ride_tri` (https://freesound.org/people/trivialAccapella/sounds/425449/),
`drum_tom_hi_hard`.

Acoustic kit pieces with `_soft` / `_hard` filenames are velocity layers
(soft below velocity ≈ 0.55, otherwise hard). Hats also have `_rr1` / `_rr2`
round-robins and a `_pedal` chick (GM 44) so 16th patterns do not machine-gun
the same take. Electronic kits still scale gain with velocity.

Hat round-robins (all CC0):

| File | Original |
| --- | --- |
| `lofi_ch.wav` | Sonic Pi `hat_gnu` |
| `lofi_ch_rr1.wav` | Sonic Pi `hat_tap` |
| `lofi_ch_rr2.wav` | Sonic Pi `hat_psych` |
| `lofi_oh.wav` | Sonic Pi `drum_cymbal_open` |
| `lofi_oh_rr1.wav` | Sonic Pi `hat_zan` |
| `lofi_pedal.wav` | Sonic Pi `drum_cymbal_pedal` |
| `pop_ch_rr1.wav` | Sonic Pi `hat_star` |
| `pop_oh_rr1.wav` | Sonic Pi `hat_gem` |
| `pop_pedal.wav` | Sonic Pi `drum_cymbal_pedal` |
| `rnb_ch_rr1.wav` | Sonic Pi `hat_zild` |
| `rnb_oh_rr1.wav` | Sonic Pi `hat_raw` |
| `rnb_pedal.wav` | Sonic Pi `drum_cymbal_pedal` |
| `cine_ch_rr1.wav` | Sonic Pi `hat_metal` |
| `cine_oh_rr1.wav` | Sonic Pi `hat_hier` |
| `cine_pedal.wav` | Sonic Pi `drum_cymbal_pedal` |

## Pitched instruments

Stereo preview multisamples (piano, EP, bass). One drum kit per genre is
documented above.

### Salamander Grand Piano (CC-BY-3.0)

Yamaha C5 recorded by Alexander Holm (AB pair, 44.1 kHz 16-bit). Two
velocity layers (v4/v12) on C and F# from C2–C6, plus hammer-release samples.

- Source: https://archive.org/details/SalamanderGrandPianoV3
- License: https://creativecommons.org/licenses/by/3.0/ (credit: Alexander Holm)

| Files | Notes |
| --- | --- |
| `inst_piano_{36,42,48,54,60,66,72,78,84}_{soft,hard}.wav` | 9 roots × 2 velocities, ~3.6 s tails |
| `inst_piano_{36,48,60,72}_rel.wav` | Pitched key-release (rel36/48/60/72) |
| `inst_piano_rel_{soft,hard}.wav` | Generic releases (rel5 / rel12) |

### Wurlitzer EP200 (CC-BY-3.0)

Real electro-mechanical EP recorded by Greg Sullivan. jRhodes3d is CC-BY-NC
and is not included; this Wurlitzer is the permissive stand-in.

- Pack: https://github.com/sfzinstruments/GregSullivan.E-Pianos
- License: CC-BY-3.0 (credit: Greg Sullivan)
- Original: http://www.sullivang.net/

| Files | Notes |
| --- | --- |
| `inst_ep_{33,40,45,51,56,61,67,73,79}_{soft,hard}.wav` | pp/mp vs f/ff, A1–G5 |

### FreePats electric bass (CC0-1.0)

Yamaha RBX finger (soft) and pick (hard), chromatic E1–E2.

- Pack: https://github.com/freepats/electric-bass-YR
- License: CC0-1.0 (https://creativecommons.org/publicdomain/zero/1.0/)

| Files | Notes |
| --- | --- |
| `inst_bass_{28–40}_{soft,hard}.wav` | Chromatic E1–E2 |

House stabs, supersaw, 808 bass, and cinematic sub stay synthesized. Preview
only — drag the MIDI into the DAW for the real mix.
