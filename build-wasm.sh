#!/usr/bin/env bash
# Builds the solver to WebAssembly for the browser (Option B: the whole engine
# runs client-side, so the GitHub Pages deploy matches the local server).
# Requires the emscripten SDK on PATH (emsdk_env.sh sourced).
#
# Output: web-live/solver.js + web-live/solver.wasm, loaded by web-live/index.html.
set -euo pipefail

OUT_DIR="web-live"

# Engine + shared JSON builder + WASM glue. Deliberately excludes server.cpp
# (httplib), interactive.cpp (stdin), main.cpp (CLI), and export.cpp (files).
SRCS=(
  src/card.cpp
  src/deck.cpp
  src/game.cpp
  src/node.cpp
  src/cfr_solver.cpp
  src/position.cpp
  src/display.cpp
  src/wasm_api.cpp
)

# Growable heap so deeper effective stacks (which explode the infoset tree)
# aren't capped by a fixed size. A growable heap is a *resizable* ArrayBuffer,
# and Chrome rejects TextDecoder/crypto views over one - we work around that by
# returning results by pointer+length (wasm_api.cpp) instead of std::string,
# and seeding RNGs off the clock (RandomSeed). HEAPU8 is exported so the
# frontend can read the result buffer.
em++ -O3 -std=c++17 -I./src "${SRCS[@]}" \
  --bind \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createSolverModule \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=268435456 \
  -s MAXIMUM_MEMORY=2147483648 \
  -s ENVIRONMENT=web \
  -s EXPORTED_RUNTIME_METHODS=HEAPU8 \
  -o "${OUT_DIR}/solver.js"

echo "Built ${OUT_DIR}/solver.js + ${OUT_DIR}/solver.wasm"
