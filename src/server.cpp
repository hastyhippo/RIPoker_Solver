#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <vector>
#include <string>
#include <cctype>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "server.h"
#include "httplib.h"
#include "cfr_solver.h"
#include "node.h"
#include "card.h"
#include "game.h"
#include "defines.h"
#include "display.h"

using namespace std;

namespace {

string RankName(int value) {
    Card c(value * 4);
    string name = c.getName();
    return name.substr(0, name.find(" of"));
}

int RankValue(const string &label) {
    for (int i = 0; i < NUM_CARDS / 4; i++) {
        if (RankName(i) == label) return i;
    }
    return 0;
}

void WriteJSONString(ostream &out, const string &s) {
    out << '"';
    for (char c : s) {
        if (c == '"' || c == '\\') out << '\\';
        out << c;
    }
    out << '"';
}

// Minimal int extractor for flat {"key": 123} request bodies.
int ExtractJSONInt(const string &body, const string &key, int def) {
    string needle = "\"" + key + "\"";
    size_t pos = body.find(needle);
    if (pos == string::npos) return def;
    pos = body.find(':', pos + needle.size());
    if (pos == string::npos) return def;
    pos++;
    while (pos < body.size() && isspace((unsigned char)body[pos])) pos++;
    size_t start = pos;
    if (pos < body.size() && body[pos] == '-') pos++;
    while (pos < body.size() && isdigit((unsigned char)body[pos])) pos++;
    if (pos == start || (pos == start + 1 && body[start] == '-')) return def;
    return stoi(body.substr(start, pos - start));
}

struct BestMatch {
    bool found = false;
    int visits = 0;
    Node *node = nullptr;
};

// Strategy at a position. pot/bet come from replaying the exact history (else
// null hands); flush variants are scanned per hand, most-visited wins.
string BuildPositionJSON(CFRSolver &cfr, int stage, int board0Val, int board1Val, const string &history) {
    const int numHandValues = NUM_CARDS / 4;
    vector<BestMatch> bestPerHand(numHandValues);

    bool replayOk = false;
    Game replayed = Game::ReplayExactHistory(cfr.stack0, cfr.stack1, history, replayOk);
    int pot = replayOk ? replayed.KeyPot() : 0;
    int bet = replayOk ? replayed.KeyBet() : 0;

    // Chip amount per offered bet/raise/all-in (same bases MakeMove uses).
    unordered_map<string, int> actionChipSize;
    if (replayOk) {
        for (int dx = 0; dx < NUM_BETS; dx++) {
            actionChipSize[move_names[MISC_ACTIONS + dx]] = (int)(bet_sizings[dx] * replayed.pot);
        }
        int facing = replayed.bet_states[replayed.stage][1 - replayed.player];
        for (int dx = 0; dx < NUM_RAISES; dx++) {
            actionChipSize[move_names[MISC_ACTIONS + NUM_BETS + dx]] = (int)(raise_sizings[dx] * facing);
        }
        // All-in = whole remaining stack (largest bet -> darkest on the ramp).
        actionChipSize["Allin"] = replayed.effective_stack[replayed.player];
    }

    if (replayOk) {
        for (int handVal = 0; handVal < numHandValues; handVal++) {
            int flushCount = (stage >= FLOP) ? 4 : 1;
            for (int flushInfo = 0; flushInfo < flushCount; flushInfo++) {
                string key = Node::BuildKey(handVal, board0Val, board1Val, flushInfo, stage, pot, bet, history);
                auto it = cfr.positionMap.find(key);
                if (it == cfr.positionMap.end()) continue;
                if (it->second.visits > bestPerHand[handVal].visits) {
                    bestPerHand[handVal] = {true, it->second.visits, &it->second};
                }
            }
        }
    }

    int totalVisits = 0;
    for (auto &b : bestPerHand) if (b.found) totalVisits += b.visits;

    // Step-by-step trail of the line so the frontend can draw the action tree.
    bool trailOk = false;
    vector<TrailStep> trail;
    if (replayOk) trail = Game::BuildTrail(cfr.stack0, cfr.stack1, history, trailOk);

    ostringstream out;
    out << fixed << setprecision(4);
    out << "{\n  \"stage\": " << stage << ",\n  \"board\": [";
    if (stage >= FLOP) WriteJSONString(out, RankName(board0Val)); else out << "null";
    out << ", ";
    if (stage >= TURN) WriteJSONString(out, RankName(board1Val)); else out << "null";
    out << "],\n  \"pot\": " << pot << ",\n  \"bet\": " << bet << ",\n  \"history\": ";
    WriteJSONString(out, history);

    out << ",\n  \"trail\": ";
    if (!trailOk) {
        out << "null";
    } else {
        out << "[\n";
        for (size_t t = 0; t < trail.size(); t++) {
            const TrailStep &s = trail[t];
            out << "    ";
            if (s.isReveal) {
                out << "{\"type\": \"card\", \"card\": ";
                WriteJSONString(out, RankName(s.revealSlot == 0 ? board0Val : board1Val));
                out << "}";
            } else {
                out << "{\"type\": \"decision\", \"player\": " << s.player << ", \"chosen\": ";
                if (s.chosen < 0) out << "null";
                else WriteJSONString(out, move_names[s.chosen]);
                out << ", \"actions\": [";
                for (size_t j = 0; j < s.legal.size(); j++) {
                    out << "{\"action\": ";
                    WriteJSONString(out, move_names[s.legal[j]]);
                    if (s.chipSize[j] >= 0) out << ", \"size\": " << s.chipSize[j];
                    out << "}";
                    if (j + 1 < s.legal.size()) out << ", ";
                }
                out << "]}";
            }
            out << (t + 1 < trail.size() ? ",\n" : "\n");
        }
        out << "  ]";
    }

    out << ",\n  \"visits\": " << totalVisits
        << ",\n  \"hands\": [\n";

    for (int handVal = 0; handVal < numHandValues; handVal++) {
        BestMatch &b = bestPerHand[handVal];
        out << "    ";
        if (!b.found) {
            out << "null";
        } else {
            vector<double> strat = b.node->GetFinalStrategy(b.node->possible_actions);
            out << "{\"rank\": ";
            WriteJSONString(out, RankName(handVal));
            out << ", \"visits\": " << b.visits << ", \"strategy\": [";
            bool first = true;
            for (int i = 0; i < NUM_ACTIONS; i++) {
                if (!b.node->possible_actions[i]) continue;
                if (!first) out << ", ";
                first = false;
                out << "{\"action\": ";
                WriteJSONString(out, move_names[i]);
                out << ", \"prob\": " << strat[i];
                auto sizeIt = actionChipSize.find(move_names[i]);
                if (sizeIt != actionChipSize.end()) out << ", \"size\": " << sizeIt->second;
                out << "}";
            }
            out << "]}";
        }
        out << (handVal + 1 < numHandValues ? ",\n" : "\n");
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace

namespace Server {

void Start(CFRSolver &cfr, int port) {
    httplib::Server svr;
    mt19937 rng(random_device{}());

    // Local dev tool - no caching so a refresh always picks up the latest files.
    svr.set_default_headers({{"Cache-Control", "no-store"}});
    svr.set_mount_point("/", "./web-live");

    // Shared with /api/train/cancel on another thread; atomic avoids a mutex.
    atomic<bool> cancelRequested{false};

    // Serializes all solver access across httplib's thread pool. Cancel stays
    // lock-free (atomic only) so it can interrupt a train holding the lock.
    mutex solverMutex;

    // One exploitability sample per EXPLOIT_EVERY trained hands (plus an
    // iteration-0 baseline), for the frontend's graph. Guarded by solverMutex.
    // Monte Carlo over board reveals: EXPLOIT_MC_CHANCE cards per reveal.
    const long long EXPLOIT_EVERY = 1000;
    const int EXPLOIT_MC_CHANCE = 4; // for each exploitabtility sample, take 4 candidates for the next card only
    vector<pair<long long, double>> exploitHistory;

    // Static action config (order + labels) fetched once by the frontend.
    svr.Get("/api/actions", [&](const httplib::Request &, httplib::Response &res) {
        ostringstream out;
        out << "{\n  \"actionOrder\": [";
        for (size_t i = 0; i < move_names.size(); i++) {
            WriteJSONString(out, move_names[i]);
            if (i + 1 < move_names.size()) out << ", ";
        }
        out << "],\n  \"actionLabels\": {\n";
        for (size_t i = 0; i < move_names.size(); i++) {
            out << "    ";
            WriteJSONString(out, move_names[i]);
            out << ": ";
            WriteJSONString(out, Display::ActionDisplayLabel(move_names[i]));
            out << (i + 1 < move_names.size() ? ",\n" : "\n");
        }
        out << "  }\n}\n";
        res.set_content(out.str(), "application/json");
    });

    svr.Post("/api/configure", [&](const httplib::Request &req, httplib::Response &res) {
        int s0 = ExtractJSONInt(req.body, "stack0", STARTING_STACK);
        int s1 = ExtractJSONInt(req.body, "stack1", STARTING_STACK);
        lock_guard<mutex> lock(solverMutex);
        cfr.Reset(s0, s1);
        exploitHistory.clear();
        exploitHistory.push_back({0, cfr.ComputeExploitability(EXPLOIT_MC_CHANCE)}); // uniform baseline
        res.set_content("{\"ok\": true}", "application/json");
    });

    svr.Post("/api/train", [&](const httplib::Request &req, httplib::Response &res) {
        int iters = ExtractJSONInt(req.body, "iterations", 0);
        lock_guard<mutex> lock(solverMutex);
        cancelRequested = false;
        // Baseline for servers that go straight to training without a configure.
        if (exploitHistory.empty()) exploitHistory.push_back({0, cfr.ComputeExploitability(EXPLOIT_MC_CHANCE)});
        int trained = 0;
        for (int i = 0; i < iters; i++) {
            if (cancelRequested) break; // checked every single iteration for near-instant response
            cfr.TrainCFR();
            trained++;
            if (cfr.iteration % EXPLOIT_EVERY == 0) {
                exploitHistory.push_back({cfr.iteration, cfr.ComputeExploitability(EXPLOIT_MC_CHANCE)});
            }
        }
        ostringstream out;
        out << "{\"iteration\": " << cfr.iteration << ", \"infosets\": " << cfr.positionMap.size()
            << ", \"trained\": " << trained << ", \"cancelled\": " << (cancelRequested ? "true" : "false") << "}";
        res.set_content(out.str(), "application/json");
    });

    svr.Post("/api/train/cancel", [&](const httplib::Request &, httplib::Response &res) {
        cancelRequested = true;
        res.set_content("{\"ok\": true}", "application/json");
    });

    svr.Get("/api/exploitability", [&](const httplib::Request &, httplib::Response &res) {
        lock_guard<mutex> lock(solverMutex);
        ostringstream out;
        out << fixed << setprecision(6) << "{\"points\": [";
        for (size_t i = 0; i < exploitHistory.size(); i++) {
            out << "{\"iter\": " << exploitHistory[i].first << ", \"value\": " << exploitHistory[i].second << "}";
            if (i + 1 < exploitHistory.size()) out << ", ";
        }
        out << "]}";
        res.set_content(out.str(), "application/json");
    });

    svr.Get("/api/position", [&](const httplib::Request &req, httplib::Response &res) {
        string history = req.get_param_value("history");
        int stage = (int)count(history.begin(), history.end(), ',');
        if (stage > TURN) stage = TURN;
        int board0Val = 0, board1Val = 0;
        if (stage >= FLOP) board0Val = RankValue(req.get_param_value("board0"));
        if (stage >= TURN) board1Val = RankValue(req.get_param_value("board1"));
        lock_guard<mutex> lock(solverMutex);
        res.set_content(BuildPositionJSON(cfr, stage, board0Val, board1Val, history), "application/json");
    });

    svr.Get("/api/random-position", [&](const httplib::Request &, httplib::Response &res) {
        lock_guard<mutex> lock(solverMutex);
        if (cfr.positionMap.empty()) {
            res.set_content(BuildPositionJSON(cfr, PREFLOP, 0, 0, ""), "application/json");
            return;
        }
        // Reservoir sampling of size 1: single pass, uniform over all keys,
        // no need to materialize the (potentially very large) key set.
        string chosenKey;
        long seen = 0;
        for (auto &kv : cfr.positionMap) {
            seen++;
            uniform_int_distribution<long> dist(1, seen);
            if (dist(rng) == seen) chosenKey = kv.first;
        }
        ParsedHash p = Node::ParseHash(chosenKey);
        res.set_content(BuildPositionJSON(cfr, p.stage, p.board0Val, p.board1Val, p.abstractHistory), "application/json");
    });

    // Pre-listen, so the page shows the uniform baseline on first load.
    exploitHistory.push_back({0, cfr.ComputeExploitability(EXPLOIT_MC_CHANCE)});

    cout << "Serving on http://localhost:" << port << "/ (Ctrl-C to stop)\n";
    svr.listen("0.0.0.0", port);
}

} // namespace Server
