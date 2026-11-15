#!/bin/sh
# Local checks before the SDK build; geometry refresh is explicit.
set -eu
cd "$(dirname "$0")"
if ! command -v python3 >/dev/null 2>&1; then
  echo 'ERROR: Python 3 required (brew install python)' >&2;exit 1
fi
if ! command -v cc >/dev/null 2>&1; then
  echo 'ERROR: Apple Command Line Tools required (xcode-select --install)' >&2;exit 1
fi
if ! command -v ufbt >/dev/null 2>&1; then
  echo 'ERROR: uFBT required (brew install ufbt)' >&2;exit 1
fi
printf '\n=== STEP 1 / 3: local automated test cases ===\n'
./tests/run_tests.sh
printf '\n=== STEP 2 / 3: bundled track geometries (optional refresh) ===\n'
if [ "${GP_UPDATE_TRACKS:-0}" = 1 ]; then
  python3 tools/generate_tracks.py
fi
printf '\n=== STEP 3 / 3: compile Flipper .fap ===\n'
ufbt
python3 tools/check_gui_stack.py
printf '\nVersion 0.7.1 built. Connect Flipper, close qFlipper, run: ufbt launch\n'
