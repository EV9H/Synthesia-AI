#!/usr/bin/env bash
# Unsigned VST3 zip for Linux / Windows. No signing secrets.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release --target SynthesiaAI_VST3
#   Tools/package_vst3.sh
#
# Writes dist/Harmonia-<version>-<os>-vst3.zip
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${SYNTHESIA_BUILD_DIR:-$ROOT/build}"
DIST="${HARMONIA_DIST_DIR:-$ROOT/dist}"
VERSION="${HARMONIA_VERSION:-0.1.0}"

uname_s="$(uname -s | tr '[:upper:]' '[:lower:]')"
case "$uname_s" in
    linux*)  os="linux" ;;
    darwin*) os="macos" ;;
    mingw*|msys*|cygwin*) os="windows" ;;
    *) os="$uname_s" ;;
esac

candidates=(
    "$BUILD/SynthesiaAI_artefacts/Release/VST3/Harmonia.vst3"
    "$BUILD/SynthesiaAI_artefacts/RelWithDebInfo/VST3/Harmonia.vst3"
    "$BUILD/SynthesiaAI_artefacts/Debug/VST3/Harmonia.vst3"
    "$BUILD/SynthesiaAI_artefacts/Release/VST3/Synthesia AI.vst3"
    "$BUILD/SynthesiaAI_artefacts/RelWithDebInfo/VST3/Synthesia AI.vst3"
    "$BUILD/SynthesiaAI_artefacts/Debug/VST3/Synthesia AI.vst3"
)
VST3=""
for c in "${candidates[@]}"; do
    if [[ -e "$c" ]]; then
        VST3="$c"
        break
    fi
done

if [[ -z "$VST3" ]]; then
    echo "No VST3 found. Build first:"
    echo "  cmake --build $BUILD --config Release --target SynthesiaAI_VST3"
    exit 1
fi

mkdir -p "$DIST"
stamp="$(date -u +%Y%m%d)"
zipname="Harmonia-${VERSION}-${os}-vst3-unsigned-${stamp}.zip"
out="$DIST/$zipname"

rm -f "$out"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
# Buyer-ready folder: docs next to the .vst3, not a loose dump.
bundle="$stage/Harmonia"
mkdir -p "$bundle"
cp -R "$VST3" "$bundle/"
cp "$ROOT/Tools/README.txt" "$bundle/README.txt"
cp "$ROOT/Tools/GETTING_STARTED.txt" "$bundle/GETTING_STARTED.txt"
cp "$ROOT/Tools/UNSIGNED_SMOKE.txt" "$bundle/UNSIGNED_SMOKE.txt"
cp "$ROOT/Tools/LICENSE_NOTE.txt" "$bundle/LICENSE.txt"
cp "$ROOT/Tools/INSTALL.md" "$bundle/INSTALL.md"
# GHA windows-latest Git bash has no `zip`. Python is on every runner.
pack_zip() {
    local py
    py="$(command -v python3 || command -v python || true)"
    if command -v zip >/dev/null 2>&1; then
        ( cd "$stage" && zip -r -q "$out" Harmonia )
        return 0
    fi
    if [[ -z "$py" ]]; then
        echo "Need zip or python3 to pack the trial zip." >&2
        return 1
    fi
    "$py" - "$stage" "$out" <<'PY'
import sys, zipfile
from pathlib import Path
stage, out = Path(sys.argv[1]), sys.argv[2]
root = stage / "Harmonia"
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
    for path in root.rglob("*"):
        if path.is_file():
            zf.write(path, path.relative_to(stage).as_posix())
PY
}
pack_zip

cat <<EOF
Wrote $out
Unsigned zip only — do not charge for this build.

macOS signing / notarizing (needs YOUR Apple credentials, not in this repo):
  1. Apple Developer Program membership
  2. Developer ID Application certificate in the keychain (or .p12 + password)
  3. App-specific password / notarytool keychain profile
  4. Then, on a Mac:
       codesign --force --deep --options runtime \\
         --sign "Developer ID Application: YOUR NAME (TEAMID)" \\
         "Harmonia.vst3"
       ditto -c -k --keepParent "Harmonia.vst3" Harmonia-vst3.zip
       xcrun notarytool submit Harmonia-vst3.zip --keychain-profile "harmonia" --wait
       xcrun stapler staple "Harmonia.vst3"
  AU: repeat for "Harmonia.component"
  Standalone: repeat for "Harmonia.app"

Windows signing (needs YOUR Authenticode cert):
  signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 ^
    /f your.pfx /p YOUR_PASSWORD "Harmonia.vst3"
EOF
