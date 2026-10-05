#!/usr/bin/env bash
# Downloads and builds external baselines into third_party/ (not versioned), then builds the
# drivers in src/baselines/. Prints the commit of every dependency so results can be traced.
# Usage: bash scripts/baselines.sh
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
THIRD=$ROOT/third_party
mkdir -p "$THIRD"

# ColPack (coloring): https://github.com/CSCsw/ColPack
if [[ ! -d "$THIRD/ColPack" ]]; then
    git clone --depth 1 https://github.com/CSCsw/ColPack.git "$THIRD/ColPack"
fi
echo "== ColPack commit: $(git -C "$THIRD/ColPack" rev-parse HEAD)"
cmake -S "$THIRD/ColPack/build/cmake" -B "$THIRD/ColPack/build/out" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$THIRD/install" -DENABLE_EXAMPLES=OFF > /dev/null
cmake --build "$THIRD/ColPack/build/out" -j 4 > /dev/null
cmake --install "$THIRD/ColPack/build/out" > /dev/null

INCLUDE=$(dirname "$(find "$THIRD/install" -name ColPackHeaders.h | head -1)")
LIBDIR=$(dirname "$(find "$THIRD/install" -name 'libColPack*' | head -1)")
g++ -std=c++17 -O3 -fopenmp -I"$INCLUDE" "$ROOT/src/baselines/colpack/driver.cpp" \
    -L"$LIBDIR" -lColPack -Wl,-rpath,"$LIBDIR" -o "$ROOT/src/baselines/colpack/driver"
echo "== built src/baselines/colpack/driver"
