#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

#include "export.h"
#include "cfr_solver.h"
#include "node.h"
#include "game.h"
#include "card.h"
#include "defines.h"
#include "display.h"

using namespace std;

namespace {

string RankName(int value) {
    // Same suit-stripped Card::getName() trick used in display.cpp; kept as
    // a tiny local duplicate rather than sharing a header for this one line.
    Card c(value * 4);
    string name = c.getName();
    return name.substr(0, name.find(" of"));
}

string FlushInfoName(int flushInfo) {
    switch (flushInfo) {
        case 0: return "no flush";
        case 1: return "flush draw/brick";
        case 2: return "flush complete";
        case 3: return "flush on board";
    }
    return "?";
}

struct HandRow {
    string rank;
    bool hasFlush;
    int flushCategory;
    string flushLabel;
    int visits;
    vector<pair<string, double>> strategy;
};

struct NodeGroup {
    int stage;
    bool hasBoard0, hasBoard1;
    int board0, board1;
    int pot, bet;
    string history;
    int visits = 0;
    vector<HandRow> hands;
    // Real chip amount behind each currently-available bet/raise action,
    // reconstructed once per group via Game::ReplayExactHistory - empty if
    // that replay wasn't possible (e.g. history was recorded in bucketed
    // rather than exact mode). Keyed by the same move_names entries as each
    // hand's strategy.
    unordered_map<string, int> actionChipSize;
};

// Reconstructs the real pot/bet_states behind `history` (see
// Game::ReplayExactHistory) and, if that succeeds, works out the actual chip
// amount each currently-offered bet/raise size resolves to at this position.
unordered_map<string, int> ComputeActionChipSizes(CFRSolver &cfr, const string &history) {
    unordered_map<string, int> sizes;
    bool ok = false;
    Game replayed = Game::ReplayExactHistory(cfr.stack0, cfr.stack1, history, ok);
    if (!ok) return sizes;

    for (int dx = 0; dx < NUM_BETS; dx++) {
        sizes[move_names[MISC_ACTIONS + dx]] = (int)(bet_sizings[dx] * replayed.pot);
    }
    int facing = replayed.bet_states[replayed.stage][1 - replayed.player];
    for (int dx = 0; dx < NUM_RAISES; dx++) {
        sizes[move_names[MISC_ACTIONS + NUM_BETS + dx]] = (int)(raise_sizings[dx] * facing);
    }
    return sizes;
}

string GroupKey(const ParsedHash &p) {
    ostringstream ss;
    ss << p.stage << "|" << (p.stage >= FLOP ? p.board0Val : -1) << "|"
       << (p.stage >= TURN ? p.board1Val : -1) << "|" << p.pot << "|" << p.bet << "|" << p.abstractHistory;
    return ss.str();
}

// Escapes the handful of characters that could ever appear in a printed
// double/string field. History codes, rank labels and action names in this
// codebase never contain '"' or '\', so this only needs to be defensive.
void WriteJSONString(ostream &out, const string &s) {
    out << '"';
    for (char c : s) {
        if (c == '"' || c == '\\') out << '\\';
        out << c;
    }
    out << '"';
}

} // namespace

namespace Export {

void WriteSolverJSON(CFRSolver &cfr, const string &path, int minVisits) {
    unordered_map<string, NodeGroup> groups;

    for (auto &entry : cfr.positionMap) {
        int visits = cfr.positionCount[entry.first];
        if (visits < minVisits) continue;

        ParsedHash p = Node::ParseHash(entry.first);
        Node *node = entry.second;

        string key = GroupKey(p);
        auto it = groups.find(key);
        if (it == groups.end()) {
            NodeGroup ng;
            ng.stage = p.stage;
            ng.hasBoard0 = p.stage >= FLOP;
            ng.board0 = p.board0Val;
            ng.hasBoard1 = p.stage >= TURN;
            ng.board1 = p.board1Val;
            ng.pot = p.pot;
            ng.bet = p.bet;
            ng.history = p.abstractHistory;
            ng.actionChipSize = ComputeActionChipSizes(cfr, ng.history);
            it = groups.emplace(key, ng).first;
        }
        it->second.visits += visits;

        HandRow hr;
        hr.rank = RankName(p.handVal);
        hr.hasFlush = p.stage >= FLOP;
        hr.flushCategory = p.flushInfo;
        hr.flushLabel = FlushInfoName(p.flushInfo);
        hr.visits = visits;

        vector<double> strat = node->GetFinalStrategy(node->possible_actions);
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (node->possible_actions[i]) hr.strategy.push_back({move_names[i], strat[i]});
        }
        it->second.hands.push_back(hr);
    }

    // Precompute, using the SAME functions the solver uses to build
    // abstractHistory, which letter each action name maps to. Bet/raise
    // sizes are always fixed multiples of the current pot/last bet, so the
    // resulting letter is deterministic regardless of the actual chip
    // amounts involved - a large reference pot avoids integer-truncation
    // drift in that computation. Always computed bucketed (useBuckets=true)
    // regardless of the live solver's actual mode: this static tree browser's
    // click-navigation assumes one fixed letter per action name, which only
    // means anything for bucketed sizing - exporting/browsing an exact-mode
    // solve with this tool is a known, out-of-scope limitation.
    const int REF = 100000;
    vector<pair<string, string>> actionLetters;
    actionLetters.push_back({"Check", Node::GetBetAction(REF, 0, true)});
    actionLetters.push_back({"Call", "c"});
    actionLetters.push_back({"Fold", "f"});
    actionLetters.push_back({"Allin", "a"});
    for (int dx = 0; dx < NUM_BETS; dx++) {
        int betSize = (int)(REF * bet_sizings[dx]);
        actionLetters.push_back({move_names[MISC_ACTIONS + dx], Node::GetBetAction(REF, betSize, true)});
    }
    for (int dx = 0; dx < NUM_RAISES; dx++) {
        int raiseSize = (int)(REF * raise_sizings[dx]);
        actionLetters.push_back({move_names[MISC_ACTIONS + NUM_BETS + dx], Node::GetRaiseAction(raiseSize, REF, true)});
    }

    ofstream out(path);
    out << fixed << setprecision(4);

    // move_names is the single source of truth for which actions exist and
    // what order they come in (bet/raise sizings are configurable in
    // game.cpp) - the frontend reads these instead of hardcoding its own
    // action list, so it stays correct if the sizing grid ever changes.
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
    out << "  },\n  \"actionLetters\": {\n";
    for (size_t i = 0; i < actionLetters.size(); i++) {
        out << "    ";
        WriteJSONString(out, actionLetters[i].first);
        out << ": ";
        WriteJSONString(out, actionLetters[i].second);
        out << (i + 1 < actionLetters.size() ? ",\n" : "\n");
    }
    out << "  },\n  \"nodes\": [\n";

    size_t groupIdx = 0;
    for (auto &kv : groups) {
        const NodeGroup &ng = kv.second;
        out << "    {\n";
        out << "      \"stage\": " << ng.stage << ",\n";
        out << "      \"board\": [";
        if (ng.hasBoard0) WriteJSONString(out, RankName(ng.board0)); else out << "null";
        out << ", ";
        if (ng.hasBoard1) WriteJSONString(out, RankName(ng.board1)); else out << "null";
        out << "],\n";
        out << "      \"pot\": " << ng.pot << ",\n";
        out << "      \"bet\": " << ng.bet << ",\n";
        out << "      \"history\": ";
        WriteJSONString(out, ng.history);
        out << ",\n";
        out << "      \"visits\": " << ng.visits << ",\n";
        out << "      \"hands\": [\n";

        for (size_t i = 0; i < ng.hands.size(); i++) {
            const HandRow &hr = ng.hands[i];
            out << "        {\n";
            out << "          \"rank\": ";
            WriteJSONString(out, hr.rank);
            out << ",\n";
            if (hr.hasFlush) {
                out << "          \"flushCategory\": " << hr.flushCategory << ",\n";
                out << "          \"flushLabel\": ";
                WriteJSONString(out, hr.flushLabel);
                out << ",\n";
            }
            out << "          \"visits\": " << hr.visits << ",\n";
            out << "          \"strategy\": [";
            for (size_t j = 0; j < hr.strategy.size(); j++) {
                out << "{\"action\": ";
                WriteJSONString(out, hr.strategy[j].first);
                out << ", \"prob\": " << hr.strategy[j].second;
                auto sizeIt = ng.actionChipSize.find(hr.strategy[j].first);
                if (sizeIt != ng.actionChipSize.end()) out << ", \"size\": " << sizeIt->second;
                out << "}";
                if (j + 1 < hr.strategy.size()) out << ", ";
            }
            out << "]\n";
            out << "        }" << (i + 1 < ng.hands.size() ? ",\n" : "\n");
        }

        out << "      ]\n";
        out << "    }" << (++groupIdx < groups.size() ? ",\n" : "\n");
    }

    out << "  ]\n}\n";
}

} // namespace Export
