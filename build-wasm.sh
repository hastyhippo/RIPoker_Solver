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

# Fixed (non-growable) 768 MB heap. A growable heap is a *resizable*
# ArrayBuffer, and current Chrome rejects TextDecoder/crypto views over one -
# a fixed heap sidesteps that entirely. 768 MB fits ~150k+ trained hands at
# the default stack; heavier training would need a larger INITIAL_MEMORY.
em++ -O3 -std=c++17 -I./src "${SRCS[@]}" \
  --bind \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createSolverModule \
  -s INITIAL_MEMORY=805306368 \
  -s ENVIRONMENT=web \
  -s EXPORTED_RUNTIME_METHODS='[]' \
  -o "${OUT_DIR}/solver.js"

echo "Built ${OUT_DIR}/solver.js + ${OUT_DIR}/solver.wasm"
