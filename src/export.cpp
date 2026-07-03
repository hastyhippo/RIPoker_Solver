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
    int sprBucket;
    string history;
    int visits = 0;
    vector<HandRow> hands;
};

string GroupKey(const ParsedHash &p) {
    ostringstream ss;
    ss << p.stage << "|" << (p.stage >= FLOP ? p.board0Val : -1) << "|"
       << (p.stage >= TURN ? p.board1Val : -1) << "|" << p.sprCat << "|" << p.abstractHistory;
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
            ng.sprBucket = p.sprCat;
            ng.history = p.abstractHistory;
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
    // drift in that computation.
    const int REF = 100000;
    vector<pair<string, char>> actionLetters;
    actionLetters.push_back({"Check", Node::GetBetAction(REF, 0)});
    actionLetters.push_back({"Call", 'c'});
    actionLetters.push_back({"Fold", 'f'});
    actionLetters.push_back({"Allin", 'a'});
    for (int dx = 0; dx < NUM_BETS; dx++) {
        int betSize = (int)(REF * bet_sizings[dx]);
        actionLetters.push_back({move_names[MISC_ACTIONS + dx], Node::GetBetAction(REF, betSize)});
    }
    for (int dx = 0; dx < NUM_RAISES; dx++) {
        int raiseSize = (int)(REF * raise_sizings[dx]);
        actionLetters.push_back({move_names[MISC_ACTIONS + NUM_BETS + dx], Node::GetRaiseAction(raiseSize, REF)});
    }

    ofstream out(path);
    out << fixed << setprecision(4);

    out << "{\n  \"actionLetters\": {\n";
    for (size_t i = 0; i < actionLetters.size(); i++) {
        out << "    ";
        WriteJSONString(out, actionLetters[i].first);
        out << ": ";
        WriteJSONString(out, string(1, actionLetters[i].second));
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
        out << "      \"sprBucket\": " << ng.sprBucket << ",\n";
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
                out << ", \"prob\": " << hr.strategy[j].second << "}";
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
