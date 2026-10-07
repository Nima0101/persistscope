#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$PWD/build/install"
cmake -S examples/consumer -B build/consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/consumer --parallel 2
build/consumer/consumer

ROOT="$(cd .. && pwd)"
PERSISTSCOPE_CLI="${PERSISTSCOPE_CLI:-$ROOT/build-agent-verify/bin/persistscope}"
if [ ! -x "$PERSISTSCOPE_CLI" ]; then
  cmake -S "$ROOT" -B "$ROOT/build-agent-verify" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/build-agent-verify" --parallel 2
fi
python3 tests/model.py "$PERSISTSCOPE_CLI"
