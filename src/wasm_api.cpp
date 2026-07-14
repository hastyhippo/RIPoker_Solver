// WebAssembly entry points for the browser build. Mirrors src/server.cpp's
// HTTP endpoints, but as embind functions the frontend calls directly: the
// whole solver runs client-side, so the deployed page has full parity with
// the local server (live training included). No threads (single-threaded WASM).

#include <emscripten/bind.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
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
// One exploitability curve per (variant, weighting) config, so A/B runs plot
// side by side. Re-configuring a config rebuilds its curve; a stack change
// wipes all of them (curves at different stacks aren't comparable).
map<int, vector<pair<long long, double>>> g_exploit; // config key -> curve
int seriesKey(int variant, int weighting) { return variant * NUM_AVG_WEIGHTINGS + weighting; }
int g_seriesStack0 = STARTING_STACK, g_seriesStack1 = STARTING_STACK;
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

// --- Result marshalling ------------------------------------------------
// A growable WASM heap is a resizable ArrayBuffer, and Chrome refuses to
// TextDecoder-decode a view over one - which is how embind returns a
// std::string. So each export writes its JSON into g_out and returns the
// byte length; the frontend reads g_out via wasmResultPtr()+length and
// decodes a COPY (slice), which is a normal non-resizable buffer.
static string g_out;
unsigned emitResult(const string &s) { g_out = s; return (unsigned)g_out.size(); }
unsigned wasmResultPtr() { return (unsigned)(uintptr_t)g_out.data(); }

// The impl_* builders are defined below; forward-declare so the exports can
// wrap them.
string impl_actions();
string impl_configure(int stack0, int stack1, int variant, int weighting, bool exploitFull);
string impl_train(int iters, bool exploitFull);
string impl_position(string history, string board0, string board1);
string impl_randomPosition();
string impl_exploitability();

// --- Exports (names match the frontend's WasmBackend); each returns the
// byte length of the JSON now sitting at wasmResultPtr(). ---
unsigned wasmActions() { return emitResult(impl_actions()); }
unsigned wasmConfigure(int s0, int s1, int variant, int weighting, bool exploitFull) { return emitResult(impl_configure(s0, s1, variant, weighting, exploitFull)); }
unsigned wasmTrain(int iters, bool exploitFull) { return emitResult(impl_train(iters, exploitFull)); }
unsigned wasmPosition(string h, string b0, string b1) { return emitResult(impl_position(h, b0, b1)); }
unsigned wasmRandomPosition() { return emitResult(impl_randomPosition()); }
unsigned wasmExploitability() { return emitResult(impl_exploitability()); }

// Action order + human labels, same payload as GET /api/actions.
string impl_actions() {
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

// Wipe + reconfigure stacks/algorithm, then seed the untrained exploitability
// baseline. variant/weighting: CFRVariant + AvgWeighting ids (see
// cfr_solver.h). exploitFull: branch every board card exactly (slow);
// false = Monte Carlo (EXPLOIT_MC_CHANCE).
string impl_configure(int stack0, int stack1, int variant, int weighting, bool exploitFull) {
    ensureReady();
    if (variant < 0 || variant >= NUM_CFR_VARIANTS) variant = VARIANT_CFR_PLUS;
    if (weighting < 0 || weighting >= NUM_AVG_WEIGHTINGS) weighting = WEIGHT_LINEAR;
    g_cfr.Reset(stack0, stack1, variant, weighting);
    if (stack0 != g_seriesStack0 || stack1 != g_seriesStack1) {
        g_exploit.clear(); // other configs' curves were for different stacks
        g_seriesStack0 = stack0;
        g_seriesStack1 = stack1;
    }
    auto &curve = g_exploit[seriesKey(variant, weighting)];
    curve.clear(); // re-running a config rebuilds its line
    curve.push_back({0, g_cfr.ComputeExploitability(exploitFull ? 0 : EXPLOIT_MC_CHANCE)});
    return "{\"ok\": true}";
}

// Train `iters` hands, sampling exploitability every EXPLOIT_EVERY, and return
// the same status shape as POST /api/train. Called in small chunks by the UI.
string impl_train(int iters, bool exploitFull) {
    ensureReady();
    const int chance = exploitFull ? 0 : EXPLOIT_MC_CHANCE;
    auto &curve = g_exploit[seriesKey(g_cfr.variant, g_cfr.weighting)];
    if (curve.empty()) curve.push_back({0, g_cfr.ComputeExploitability(chance)});
    int trained = 0;
    for (int i = 0; i < iters; i++) {
        g_cfr.TrainCFR();
        trained++;
        if (g_cfr.iteration % EXPLOIT_EVERY == 0) {
            curve.push_back({g_cfr.iteration, g_cfr.ComputeExploitability(chance)});
        }
    }
    ostringstream out;
    out << "{\"iteration\": " << g_cfr.iteration << ", \"infosets\": " << g_cfr.positionMap.size()
        << ", \"trained\": " << trained << ", \"cancelled\": false}";
    return out.str();
}

// Position payload for a (history, board) - identical logic to GET /api/position.
string impl_position(string history, string board0, string board1) {
    ensureReady();
    int stage = (int)count(history.begin(), history.end(), ',');
    if (stage > TURN) stage = TURN;
    int board0Val = 0, board1Val = 0;
    if (stage >= FLOP) board0Val = Position::RankValue(board0);
    if (stage >= TURN) board1Val = Position::RankValue(board1);
    return Position::BuildJSON(g_cfr, stage, board0Val, board1Val, history);
}

// A uniformly random trained position (reservoir sampling), like /api/random-position.
string impl_randomPosition() {
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

// The per-variant exploitability curves, same shape as GET /api/exploitability.
string impl_exploitability() {
    ostringstream out;
    out << fixed << setprecision(6) << "{\"series\": [";
    bool firstSeries = true;
    for (auto &kv : g_exploit) {
        if (!firstSeries) out << ", ";
        firstSeries = false;
        int variant = kv.first / NUM_AVG_WEIGHTINGS, weighting = kv.first % NUM_AVG_WEIGHTINGS;
        out << "{\"variant\": " << variant << ", \"weighting\": " << weighting << ", \"name\": ";
        writeJSONString(out, string(CFRSolver::VariantName(variant)) + " · " + CFRSolver::WeightingName(weighting));
        out << ", \"points\": [";
        for (size_t i = 0; i < kv.second.size(); i++) {
            out << "{\"iter\": " << kv.second[i].first << ", \"value\": " << kv.second[i].second << "}";
            if (i + 1 < kv.second.size()) out << ", ";
        }
        out << "]}";
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
    emscripten::function("wasmResultPtr", &wasmResultPtr);
}
