#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <vector>
#include <string>
#include <cctype>
#include <atomic>
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

// Minimal flat-object int extractor - the request bodies this server accepts
// are always simple {"key": 123, ...} objects, so a full JSON parser is
// unnecessary.
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

// Same flat-object assumption as ExtractJSONInt - JSON booleans are the bare
// words true/false, no quoting to strip.
bool ExtractJSONBool(const string &body, const string &key, bool def) {
    string needle = "\"" + key + "\"";
    size_t pos = body.find(needle);
    if (pos == string::npos) return def;
    pos = body.find(':', pos + needle.size());
    if (pos == string::npos) return def;
    pos++;
    while (pos < body.size() && isspace((unsigned char)body[pos])) pos++;
    if (body.compare(pos, 4, "true") == 0) return true;
    if (body.compare(pos, 5, "false") == 0) return false;
    return def;
}

struct BestMatch {
    bool found = false;
    int visits = 0;
    Node *node = nullptr;
};

// Answers "what's the strategy at position X". The caller specifies
// stage/board/history; the money state (pot, facing bet) that the infoset key
// now depends on is recovered by replaying the exact history (see
// Game::ReplayExactHistory), which reproduces the same pot/bet the trainer
// hashed with. Flush category still isn't pinned down by a position query
// (it depends on the concrete suits), so those 1-4 variants are scanned per
// hand value and the most-visited match wins - the same tie-break used for
// reporting elsewhere. If the history can't be replayed (e.g. it was recorded
// in bucketed rather than exact bet-size mode, so no exact chip amounts
// exist), the money state is unknown and every hand comes back null.
string BuildPositionJSON(CFRSolver &cfr, int stage, int board0Val, int board1Val, const string &history) {
    const int numHandValues = NUM_CARDS / 4;
    vector<BestMatch> bestPerHand(numHandValues);

    bool replayOk = false;
    Game replayed = Game::ReplayExactHistory(cfr.stack0, cfr.stack1, history, replayOk);
    int pot = replayOk ? replayed.KeyPot() : 0;
    int bet = replayOk ? replayed.KeyBet() : 0;

    // Actual chip amount behind each currently-offered bet/raise at this
    // position, using the same bases MakeMove sizes against (committed pot for
    // bets, the facing bet for raises).
    unordered_map<string, int> actionChipSize;
    if (replayOk) {
        for (int dx = 0; dx < NUM_BETS; dx++) {
            actionChipSize[move_names[MISC_ACTIONS + dx]] = (int)(bet_sizings[dx] * replayed.pot);
        }
        int facing = replayed.bet_states[replayed.stage][1 - replayed.player];
        for (int dx = 0; dx < NUM_RAISES; dx++) {
            actionChipSize[move_names[MISC_ACTIONS + NUM_BETS + dx]] = (int)(raise_sizings[dx] * facing);
        }
    }

    if (replayOk) {
        for (int handVal = 0; handVal < numHandValues; handVal++) {
            int flushCount = (stage >= FLOP) ? 4 : 1;
            for (int flushInfo = 0; flushInfo < flushCount; flushInfo++) {
                string key = Node::BuildKey(handVal, board0Val, board1Val, flushInfo, stage, pot, bet, history);
                auto it = cfr.positionMap.find(key);
                if (it == cfr.positionMap.end()) continue;
                int visits = cfr.positionCount[key];
                if (visits > bestPerHand[handVal].visits) {
                    bestPerHand[handVal] = {true, visits, it->second};
                }
            }
        }
    }

    int totalVisits = 0;
    for (auto &b : bestPerHand) if (b.found) totalVisits += b.visits;

    ostringstream out;
    out << fixed << setprecision(4);
    out << "{\n  \"stage\": " << stage << ",\n  \"board\": [";
    if (stage >= FLOP) WriteJSONString(out, RankName(board0Val)); else out << "null";
    out << ", ";
    if (stage >= TURN) WriteJSONString(out, RankName(board1Val)); else out << "null";
    out << "],\n  \"pot\": " << pot << ",\n  \"bet\": " << bet << ",\n  \"history\": ";
    WriteJSONString(out, history);
    out << ",\n  \"visits\": " << totalVisits
        << ",\n  \"useBetSizeBuckets\": " << (cfr.useBetSizeBuckets ? "true" : "false")
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

    // This is a local dev tool whose static files change frequently during
    // development - disable caching so a browser refresh always picks up
    // the latest index.html/app.js/style.css instead of a stale copy.
    svr.set_default_headers({{"Cache-Control", "no-store"}});
    svr.set_mount_point("/", "./web-live");

    // Shared between /api/train (runs on whichever thread handles that
    // request) and /api/train/cancel (runs on a different thread while the
    // former is still in flight) - atomic so setting/checking it across
    // threads is well-defined without a full mutex for a single flag.
    atomic<bool> cancelRequested{false};

    // Static config, fetched once by the frontend at page load: which actions
    // exist, in what order, and their display labels. move_names is the
    // single source of truth (bet/raise sizings are configurable in
    // game.cpp), so the frontend reads this instead of hardcoding its own
    // action list.
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
        bool useBetSizeBuckets = ExtractJSONBool(req.body, "useBetSizeBuckets", false);
        cfr.Reset(s0, s1, useBetSizeBuckets);
        res.set_content("{\"ok\": true}", "application/json");
    });

    svr.Post("/api/train", [&](const httplib::Request &req, httplib::Response &res) {
        int iters = ExtractJSONInt(req.body, "iterations", 0);
        cancelRequested = false;
        int trained = 0;
        for (int i = 0; i < iters; i++) {
            if (cancelRequested) break; // checked every single iteration for near-instant response
            cfr.TrainCFR();
            trained++;
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

    svr.Get("/api/position", [&](const httplib::Request &req, httplib::Response &res) {
        string history = req.get_param_value("history");
        int stage = (int)count(history.begin(), history.end(), ',');
        if (stage > TURN) stage = TURN;
        int board0Val = 0, board1Val = 0;
        if (stage >= FLOP) board0Val = RankValue(req.get_param_value("board0"));
        if (stage >= TURN) board1Val = RankValue(req.get_param_value("board1"));
        res.set_content(BuildPositionJSON(cfr, stage, board0Val, board1Val, history), "application/json");
    });

    svr.Get("/api/random-position", [&](const httplib::Request &, httplib::Response &res) {
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

    cout << "Serving on http://localhost:" << port << "/ (Ctrl-C to stop)\n";
    svr.listen("0.0.0.0", port);
}

} // namespace Server
