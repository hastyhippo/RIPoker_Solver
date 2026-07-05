// WebAssembly entry points for the browser build. Mirrors src/server.cpp's
// HTTP endpoints, but as embind functions the frontend calls directly: the
// whole solver runs client-side, so the deployed page has full parity with
// the local server (live training included). No threads (single-threaded WASM).

#include <emscripten/bind.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <utility>

#include "cfr_solver.h"
#include "position.h"
#include "node.h"
#include "card.h"
#include "game.h"
#include "display.h"
#include "defines.h"

using namespace std;

namespace {

CFRSolver g_cfr;
vector<pair<long long, double>> g_exploit; // exploitability curve for the graph
mt19937 g_rng{RandomSeed()};
bool g_ready = false;

const long long EXPLOIT_EVERY = 1000;
const int EXPLOIT_MC_CHANCE = 4; // Monte-Carlo board reveals, matches the server

// One-time card-strength table build (the CLI/server do this in Initialise()).
void ensureReady() {
    if (g_ready) return;
    Card::initialiseMap();
    g_ready = true;
}

void writeJSONString(ostream &out, const string &s) {
    out << '"';
    for (char c : s) {
        if (c == '"' || c == '\\') out << '\\';
        out << c;
    }
    out << '"';
}

} // namespace

// --- Exports (names match the frontend's WasmBackend) ---

// Action order + human labels, same payload as GET /api/actions.
string wasmActions() {
    ostringstream out;
    out << "{\n  \"actionOrder\": [";
    for (size_t i = 0; i < move_names.size(); i++) {
        writeJSONString(out, move_names[i]);
        if (i + 1 < move_names.size()) out << ", ";
    }
    out << "],\n  \"actionLabels\": {\n";
    for (size_t i = 0; i < move_names.size(); i++) {
        out << "    ";
        writeJSONString(out, move_names[i]);
        out << ": ";
        writeJSONString(out, Display::ActionDisplayLabel(move_names[i]));
        out << (i + 1 < move_names.size() ? ",\n" : "\n");
    }
    out << "  }\n}\n";
    return out.str();
}

// Wipe + reconfigure stacks, then seed the untrained exploitability baseline.
string wasmConfigure(int stack0, int stack1) {
    ensureReady();
    g_cfr.Reset(stack0, stack1);
    g_exploit.clear();
    g_exploit.push_back({0, g_cfr.ComputeExploitability(EXPLOIT_MC_CHANCE)});
    return "{\"ok\": true}";
}

// Train `iters` hands, sampling exploitability every EXPLOIT_EVERY, and return
// the same status shape as POST /api/train. Called in small chunks by the UI.
string wasmTrain(int iters) {
    ensureReady();
    if (g_exploit.empty()) g_exploit.push_back({0, g_cfr.ComputeExploitability(EXPLOIT_MC_CHANCE)});
    int trained = 0;
    for (int i = 0; i < iters; i++) {
        g_cfr.TrainCFR();
        trained++;
        if (g_cfr.iteration % EXPLOIT_EVERY == 0) {
            g_exploit.push_back({g_cfr.iteration, g_cfr.ComputeExploitability(EXPLOIT_MC_CHANCE)});
        }
    }
    ostringstream out;
    out << "{\"iteration\": " << g_cfr.iteration << ", \"infosets\": " << g_cfr.positionMap.size()
        << ", \"trained\": " << trained << ", \"cancelled\": false}";
    return out.str();
}

// Position payload for a (history, board) - identical logic to GET /api/position.
string wasmPosition(string history, string board0, string board1) {
    ensureReady();
    int stage = (int)count(history.begin(), history.end(), ',');
    if (stage > TURN) stage = TURN;
    int board0Val = 0, board1Val = 0;
    if (stage >= FLOP) board0Val = Position::RankValue(board0);
    if (stage >= TURN) board1Val = Position::RankValue(board1);
    return Position::BuildJSON(g_cfr, stage, board0Val, board1Val, history);
}

// A uniformly random trained position (reservoir sampling), like /api/random-position.
string wasmRandomPosition() {
    ensureReady();
    if (g_cfr.positionMap.empty()) {
        return Position::BuildJSON(g_cfr, PREFLOP, 0, 0, "");
    }
    string chosenKey;
    long seen = 0;
    for (auto &kv : g_cfr.positionMap) {
        seen++;
        uniform_int_distribution<long> dist(1, seen);
        if (dist(g_rng) == seen) chosenKey = kv.first;
    }
    ParsedHash p = Node::ParseHash(chosenKey);
    return Position::BuildJSON(g_cfr, p.stage, p.board0Val, p.board1Val, p.abstractHistory);
}

// The exploitability curve, same shape as GET /api/exploitability.
string wasmExploitability() {
    ostringstream out;
    out << fixed << setprecision(6) << "{\"points\": [";
    for (size_t i = 0; i < g_exploit.size(); i++) {
        out << "{\"iter\": " << g_exploit[i].first << ", \"value\": " << g_exploit[i].second << "}";
        if (i + 1 < g_exploit.size()) out << ", ";
    }
    out << "]}";
    return out.str();
}

EMSCRIPTEN_BINDINGS(solver) {
    emscripten::function("wasmActions", &wasmActions);
    emscripten::function("wasmConfigure", &wasmConfigure);
    emscripten::function("wasmTrain", &wasmTrain);
    emscripten::function("wasmPosition", &wasmPosition);
    emscripten::function("wasmRandomPosition", &wasmRandomPosition);
    emscripten::function("wasmExploitability", &wasmExploitability);
}
