#!/usr/bin/env python3
"""Check a Harmonia-exported .mid is buyer-ready.

  Tools/verify_midi_export.py path/to/clip.mid

Requires:
  - 960 PPQ
  - named tracks Chords / Bass / Synth / Piano / Drums
  - drums on GM channel 10 (1-based)
  - at least one note-on on channels 1–4
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


REQUIRED_TRACKS = ("Chords", "Bass", "Synth", "Piano", "Drums")
EXPECTED_PPQ = 960
DRUM_CHANNEL = 10  # 1-based GM kit


def _read_chunk(data: bytes, offset: int) -> tuple[bytes, bytes, int]:
    if offset + 8 > len(data):
        raise ValueError("truncated MIDI chunk header")
    ident = data[offset : offset + 4]
    (length,) = struct.unpack(">I", data[offset + 4 : offset + 8])
    start = offset + 8
    end = start + length
    if end > len(data):
        raise ValueError(f"truncated MIDI chunk {ident!r}")
    return ident, data[start:end], end


def _read_varlen(buf: bytes, i: int) -> tuple[int, int]:
    value = 0
    for _ in range(4):
        if i >= len(buf):
            raise ValueError("truncated variable-length quantity")
        byte = buf[i]
        i += 1
        value = (value << 7) | (byte & 0x7F)
        if byte < 0x80:
            return value, i
    raise ValueError("invalid variable-length quantity")


def parse_smf(path: Path) -> dict:
    data = path.read_bytes()
    ident, header, offset = _read_chunk(data, 0)
    if ident != b"MThd" or len(header) < 6:
        raise ValueError("not a Standard MIDI File (missing MThd)")
    fmt, ntrks, division = struct.unpack(">HHH", header[:6])
    if division & 0x8000:
        raise ValueError("SMPTE time division is not supported")

    tracks: list[dict] = []
    for _ in range(ntrks):
        ident, body, offset = _read_chunk(data, offset)
        if ident != b"MTrk":
            raise ValueError(f"expected MTrk, got {ident!r}")
        tracks.append(_parse_track(body))

    return {"format": fmt, "ppq": division, "tracks": tracks}


def _parse_track(body: bytes) -> dict:
    name = ""
    note_channels: set[int] = set()
    i = 0
    running = None
    while i < len(body):
        _, i = _read_varlen(body, i)
        if i >= len(body):
            break
        status = body[i]
        if status < 0x80:
            if running is None:
                raise ValueError("running status with no previous status")
            status = running
        else:
            i += 1
            if status < 0xF0:
                running = status

        if status == 0xFF:
            if i >= len(body):
                break
            meta = body[i]
            i += 1
            length, i = _read_varlen(body, i)
            payload = body[i : i + length]
            i += length
            if meta == 0x03:
                name = payload.decode("latin-1", errors="replace").strip()
            continue

        if status == 0xF0 or status == 0xF7:
            length, i = _read_varlen(body, i)
            i += length
            running = None
            continue

        kind = status & 0xF0
        channel = (status & 0x0F) + 1  # 1–16
        if kind in (0xC0, 0xD0):
            i += 1
        elif kind == 0x90:
            if i + 1 >= len(body):
                break
            pitch, vel = body[i], body[i + 1]
            i += 2
            if vel > 0:
                note_channels.add(channel)
        else:
            i += 2

    return {"name": name, "note_channels": sorted(note_channels)}


def check(path: Path) -> list[str]:
    smf = parse_smf(path)
    errors: list[str] = []
    if smf["ppq"] != EXPECTED_PPQ:
        errors.append(f"PPQ is {smf['ppq']}, expected {EXPECTED_PPQ}")

    names = {t["name"] for t in smf["tracks"] if t["name"]}
    missing = [n for n in REQUIRED_TRACKS if n not in names]
    if missing:
        errors.append("missing track names: " + ", ".join(missing) + f" (found {sorted(names)})")

    drums = next((t for t in smf["tracks"] if t["name"] == "Drums"), None)
    if drums is not None:
        if DRUM_CHANNEL not in drums["note_channels"]:
            errors.append(
                f"Drums track has note channels {drums['note_channels']}, expected {DRUM_CHANNEL}"
            )
        extra = [c for c in drums["note_channels"] if c != DRUM_CHANNEL]
        if extra:
            errors.append(f"Drums track also has non-GM channels {extra}")

    melodic = {1, 2, 3, 4}
    found = set()
    for t in smf["tracks"]:
        if t["name"] == "Drums":
            continue
        found.update(c for c in t["note_channels"] if c in melodic)
    if not found:
        errors.append("no note-ons on channels 1–4 (Chords/Bass/Synth/Piano)")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("midi", type=Path, help="path to a Harmonia-exported .mid")
    args = parser.parse_args()
    if not args.midi.is_file():
        print(f"FAIL  {args.midi} does not exist", file=sys.stderr)
        return 2
    try:
        errors = check(args.midi)
    except ValueError as exc:
        print(f"FAIL  {args.midi}: {exc}", file=sys.stderr)
        return 2
    if errors:
        print(f"FAIL  {args.midi}")
        for err in errors:
            print(f"  - {err}")
        return 1
    print(f"PASS  {args.midi}  (960 PPQ, named parts, drums ch {DRUM_CHANNEL})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
