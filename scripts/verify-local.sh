#!/usr/bin/env bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
rm -rf build-agent-verify
cmake -S . -B build-agent-verify -DCMAKE_BUILD_TYPE=Release
cmake --build build-agent-verify --parallel 2
ctest --test-dir build-agent-verify --output-on-failure
python3 tests/quickstart.py build-agent-verify/bin/persistscope
python3 scripts/source_hygiene.py
python3 scripts/check_format.py
if [ -x safe-update/scripts/verify-local.sh ]; then safe-update/scripts/verify-local.sh; fi
