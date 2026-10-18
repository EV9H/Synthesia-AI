#!/usr/bin/env bash
# Operator mint — before Lemon checkout is live:
#   Tools/make_license_key.sh
# Prints one offline HRM-XXXXXXXX-CCCC. Email that line to a tester.
# No Lemon secret. Offline keys never expire and never leave the plugin.
set -euo pipefail
payload="$(python3 - <<'PY'
import hashlib, os, time
raw = hashlib.sha1(f"{os.urandom(8).hex()}{time.time()}".encode()).hexdigest()[:8].upper()
s = 0xA5
for ch in raw:
    n = int(ch, 16)
    s = (s * 7 + n) & 0xFFFF
print(f"{raw} {s:04X}")
PY
)"
hex="${payload%% *}"
sum="${payload##* }"
echo "HRM-${hex}-${sum}"
