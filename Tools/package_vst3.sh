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
cp -R "$VST3" "$stage/"
cp "$ROOT/Tools/GETTING_STARTED.txt" "$stage/GETTING_STARTED.txt"
(
    cd "$stage"
    zip -r -q "$out" .
)

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
         "Synthesia AI.vst3"
       ditto -c -k --keepParent "Synthesia AI.vst3" Harmonia-vst3.zip
       xcrun notarytool submit Harmonia-vst3.zip --keychain-profile "harmonia" --wait
       xcrun stapler staple "Synthesia AI.vst3"
  AU: repeat for "Synthesia AI.component"
  Standalone: repeat for "Synthesia AI.app"

Windows signing (needs YOUR Authenticode cert):
  signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 ^
    /f your.pfx /p YOUR_PASSWORD "Synthesia AI.vst3"
EOF
