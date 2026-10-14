from __future__ import annotations

import os
from pathlib import Path


def repo_root() -> Path:
    here = Path(__file__).resolve().parent
    return Path(os.environ.get("SYNTHESIA_ROOT", here.parent.parent))


def cache_dir() -> Path:
    root = repo_root()
    path = Path(os.environ.get("SYNTHESIA_EVAL_CACHE", root / ".cache" / "eval"))
    path.mkdir(parents=True, exist_ok=True)
    return path


def refs_dir() -> Path:
    path = cache_dir() / "refs"
    path.mkdir(parents=True, exist_ok=True)
    return path


def venv_dir() -> Path:
    return cache_dir() / "venv"


GENRES = ("pop", "lofi", "house", "trap", "rnb", "cinematic")
PARTS = ("chords", "bass", "synth", "piano", "drums")

GENRE_CLI = {
    "pop": "pop",
    "lofi": "lofi",
    "house": "house",
    "trap": "trap",
    "rnb": "rnb",
    "cinematic": "cinematic",
}

# SynthesiaRender --genre names already match.
DEFAULT_TEMPO = {
    "pop": 110.0,
    "lofi": 84.0,
    "house": 124.0,
    "trap": 140.0,
    "rnb": 88.0,
    "cinematic": 80.0,
}

REGISTER_LIMITS = {
    "bass": (24, 60),
    "synth": (48, 96),
    "piano": (21, 96),
    "chords": (36, 88),
    "drums": (27, 87),
}

GM_KICK = {35, 36}
GM_SNARE = {37, 38, 39, 40}
GM_HAT = {42, 44, 46}
GM_OPEN_HAT = {46}
