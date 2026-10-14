"""Download small public MIDI reference subsets into a gitignored cache.

Drums: Magenta Groove MIDI Dataset (MIDI-only zip).
Harmony / bass / melody: Magenta MAESTRO MIDI sample + in-repo fixtures, plus
an optional Lakh-like multi-track grab when a compact archive is reachable.
"""
from __future__ import annotations

import csv
import io
import json
import random
import shutil
import tarfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

from paths import GENRES, refs_dir, repo_root

GROOVE_URL = "https://storage.googleapis.com/magentadata/datasets/groove/groove-v1.0.0-midionly.zip"
MAESTRO_URL = "https://storage.googleapis.com/magentadata/datasets/maestro/v3.0.0/maestro-v3.0.0-midi.zip"

# Compact multi-track MIDI packs used as a Lakh-like harmonic/bass/melody set
# when the full Lakh archive (1.7 GB) is not wanted. Tried in order.
LAKH_LIKE_URLS = (
    # Small public multi-instrument MIDI collection (if the host is up).
    "https://github.com/craffel/pretty-midi/archive/refs/heads/main.tar.gz",
)

UA = "SynthesiaAI-eval/1.0 (+https://github.com/EV9H/Synthesia-AI)"

# Groove `style` prefixes -> our genre buckets.
GROOVE_STYLE_MAP = {
    "hiphop": "trap",
    "hip-hop": "trap",
    "rap": "trap",
    "trap": "trap",
    "jazz": "lofi",
    "jazz-funk": "lofi",
    "soul": "rnb",
    "rnb": "rnb",
    "rn": "rnb",
    "funk": "rnb",
    "gospel": "rnb",
    "dance": "house",
    "disco": "house",
    "house": "house",
    "afrobeat": "house",
    "electronic": "house",
    "rock": "pop",
    "pop": "pop",
    "punk": "pop",
    "country": "pop",
    "songwriter": "pop",
    "neworleans": "pop",
    "latin": "cinematic",
    "blues": "cinematic",
    "reggae": "pop",
}


def _fetch(url: str, dest: Path, timeout: int = 120) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.stat().st_size > 0:
        return
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    print(f"  downloading {url}")
    with urllib.request.urlopen(req, timeout=timeout) as resp, dest.open("wb") as out:
        shutil.copyfileobj(resp, out)


def _extract_midis(archive: Path, dest: Path, limit: int | None = None) -> list[Path]:
    dest.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    suffix = archive.suffix.lower()
    name = archive.name.lower()

    def take(info_name: str, data: bytes) -> None:
        if limit is not None and len(written) >= limit:
            return
        low = info_name.lower()
        if not low.endswith(".mid") and not low.endswith(".midi"):
            return
        target = dest / Path(info_name).name
        if not target.exists():
            target.write_bytes(data)
        written.append(target)

    if name.endswith(".tar.gz") or suffix in {".tgz", ".gz"} and ".tar" in name:
        with tarfile.open(archive, "r:gz") as tar:
            for member in tar.getmembers():
                if limit is not None and len(written) >= limit:
                    break
                if not member.isfile():
                    continue
                extracted = tar.extractfile(member)
                if extracted is None:
                    continue
                take(member.name, extracted.read())
        return written

    with zipfile.ZipFile(archive) as zf:
        for info in zf.infolist():
            if limit is not None and len(written) >= limit:
                break
            take(info.filename, zf.read(info))
    return written


def _groove_genre(style: str) -> str | None:
    token = (style or "").split("/")[0].split("-")[0].strip().lower()
    if not token:
        return None
    if token in GROOVE_STYLE_MAP:
        return GROOVE_STYLE_MAP[token]
    for key, genre in GROOVE_STYLE_MAP.items():
        if token.startswith(key) or key.startswith(token):
            return genre
    return None


def download_groove(root: Path, per_genre: int = 16) -> dict[str, list[Path]]:
    zpath = root / "downloads" / "groove-midionly.zip"
    try:
        _fetch(GROOVE_URL, zpath)
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        print(f"  groove download failed: {exc}")
        return {g: [] for g in GENRES}

    buckets: dict[str, list[Path]] = {g: [] for g in GENRES}
    rng = random.Random(7)

    with zipfile.ZipFile(zpath) as zf:
        info_name = next((n for n in zf.namelist() if n.endswith("info.csv")), None)
        rows = []
        if info_name:
            with zf.open(info_name) as fh:
                text = io.TextIOWrapper(fh, encoding="utf-8")
                rows = list(csv.DictReader(text))
        if rows:
            by_genre: dict[str, list[dict]] = {g: [] for g in GENRES}
            for row in rows:
                genre = _groove_genre(row.get("style", ""))
                midi_name = row.get("midi_filename") or row.get("midi_file") or ""
                if genre and midi_name:
                    by_genre[genre].append(row)
            for genre, candidates in by_genre.items():
                rng.shuffle(candidates)
                for row in candidates:
                    if len(buckets[genre]) >= per_genre:
                        break
                    midi_name = row.get("midi_filename") or ""
                    candidates = (
                        midi_name,
                        "groove/" + midi_name,
                        "groove-v1.0.0-midionly/" + midi_name,
                    )
                    zip_member = next((n for n in candidates if n in zf.namelist()), None)
                    if zip_member is None:
                        zip_member = next((n for n in zf.namelist() if n.endswith("/" + midi_name) or n.endswith(midi_name)), None)
                    if zip_member is None:
                        continue
                    dest = root / "groove" / genre / Path(zip_member).name
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    if not dest.exists():
                        dest.write_bytes(zf.read(zip_member))
                    buckets[genre].append(dest)
        else:
            names = [n for n in zf.namelist() if n.lower().endswith(".mid")]
            rng.shuffle(names)
            genre_cycle = list(GENRES)
            for i, name in enumerate(names[: per_genre * len(GENRES)]):
                genre = genre_cycle[i % len(genre_cycle)]
                dest = root / "groove" / genre / Path(name).name
                dest.parent.mkdir(parents=True, exist_ok=True)
                if not dest.exists():
                    dest.write_bytes(zf.read(name))
                buckets[genre].append(dest)
    return buckets


def download_maestro(root: Path, n: int = 24) -> list[Path]:
    zpath = root / "downloads" / "maestro-v3-midi.zip"
    try:
        _fetch(MAESTRO_URL, zpath, timeout=180)
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        print(f"  maestro download failed: {exc}")
        return []
    dest = root / "maestro"
    dest.mkdir(exist_ok=True)
    written: list[Path] = []
    with zipfile.ZipFile(zpath) as zf:
        names = [n for n in zf.namelist() if n.lower().endswith(".midi") or n.lower().endswith(".mid")]
        names.sort()
        for name in names[:n]:
            target = dest / Path(name).name
            if not target.exists():
                target.write_bytes(zf.read(name))
            written.append(target)
    return written


def download_lakh_like(root: Path, n: int = 20) -> list[Path]:
    dest = root / "lakh"
    dest.mkdir(exist_ok=True)
    existing = list(dest.glob("*.mid")) + list(dest.glob("*.midi"))
    if len(existing) >= min(8, n):
        return existing[:n]
    for url in LAKH_LIKE_URLS:
        try:
            archive = root / "downloads" / Path(url).name
            _fetch(url, archive, timeout=90)
            files = _extract_midis(archive, dest, limit=n)
            if files:
                return files
        except (urllib.error.URLError, TimeoutError, OSError, tarfile.TarError, zipfile.BadZipFile) as exc:
            print(f"  lakh-like skip {url}: {exc}")
    return list(dest.glob("*.mid"))[:n]


def copy_local_fixtures(root: Path) -> dict[str, list[Path]]:
    src = repo_root() / "Tools" / "midi"
    buckets: dict[str, list[Path]] = {g: [] for g in GENRES}
    if not src.exists():
        return buckets
    dest = root / "local"
    dest.mkdir(exist_ok=True)
    for midi in src.rglob("*.mid"):
        target = dest / midi.name
        if not target.exists():
            shutil.copy2(midi, target)
        name = midi.name.lower()
        if "neo-soul" in name or "rnb" in name:
            buckets["rnb"].append(target)
        elif "lofi" in name or "jazz" in name:
            buckets["lofi"].append(target)
        else:
            buckets["pop"].append(target)
            buckets["rnb"].append(target)
    return buckets


def ensure_refs(force: bool = False) -> dict:
    root = refs_dir()
    index_path = root / "index.json"
    if index_path.exists() and not force:
        data = json.loads(index_path.read_text())
        if data.get("ok"):
            return data

    print("==> Fetching reference MIDI subsets")
    (root / "downloads").mkdir(exist_ok=True)
    groove = download_groove(root)
    maestro = download_maestro(root)
    lakh = download_lakh_like(root)
    local = copy_local_fixtures(root)

    files: dict[str, dict[str, list[str]]] = {g: {p: [] for p in ("drums", "harmony")} for g in GENRES}
    for genre, paths in groove.items():
        files[genre]["drums"] = [str(p) for p in paths]
    for genre, paths in local.items():
        files[genre]["harmony"].extend(str(p) for p in paths)

    # MAESTRO piano performances: voice-leading / register refs for every genre,
    # with extra weight on cinematic and pop.
    for i, path in enumerate(maestro):
        files[GENRES[i % len(GENRES)]]["harmony"].append(str(path))
        if i % 2 == 0:
            files["cinematic"]["harmony"].append(str(path))
        if i % 3 == 0:
            files["pop"]["harmony"].append(str(path))

    for i, path in enumerate(lakh):
        genre = GENRES[i % len(GENRES)]
        files[genre]["harmony"].append(str(path))

    summary = {
        "ok": True,
        "sources": {
            "groove": GROOVE_URL,
            "maestro": MAESTRO_URL,
            "lakh_like": list(LAKH_LIKE_URLS),
            "local": str(repo_root() / "Tools" / "midi"),
        },
        "counts": {
            g: {"drums": len(files[g]["drums"]), "harmony": len(files[g]["harmony"])}
            for g in GENRES
        },
        "files": files,
    }
    index_path.write_text(json.dumps(summary, indent=2))
    print("  ref counts:", summary["counts"])
    return summary


if __name__ == "__main__":
    ensure_refs(force="--force" in __import__("sys").argv)
