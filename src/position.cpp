#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_map>

#include "position.h"
#include "cfr_solver.h"
#include "node.h"
#include "card.h"
#include "game.h"
#include "defines.h"

using namespace std;

namespace {

void WriteJSONString(ostream &out, const string &s) {
    out << '"';
    for (char c : s) {
        if (c == '"' || c == '\\') out << '\\';
        out << c;
    }
    out << '"';
}

// One rendered strategy row: a (hand value, flush category) pair with data.
struct Row {
    int handVal;
    int flushInfo;
    int visits;
    Node *node;
};

} // namespace

namespace Position {

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

string FlushLabel(int stage, int flushInfo) {
    if (stage < FLOP) return "";                    // no board yet
    if (stage == FLOP) return flushInfo == 1 ? "flush draw" : "no flush draw";
    switch (flushInfo) {                            // turn: 4 categories
        case 0: return "no flush";
        case 1: return "flush draw";
        case 2: return "flush";
        case 3: return "flush on board";
    }
    return "?";
}

string BuildJSON(CFRSolver &cfr, int stage, int board0Val, int board1Val, const string &history) {
    const int numHandValues = NUM_CARDS / 4;

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

    // One row per (hand value, present flush category) - preflop has just the
    // single flush=0 slot, flop+ splits flush-draw from no-flush-draw, etc.
    vector<Row> rows;
    int totalVisits = 0;
    if (replayOk) {
        for (int handVal = 0; handVal < numHandValues; handVal++) {
            int flushCount = (stage >= FLOP) ? 4 : 1;
            for (int flushInfo = 0; flushInfo < flushCount; flushInfo++) {
                string key = Node::BuildKey(handVal, board0Val, board1Val, flushInfo, stage, pot, bet, history);
                auto it = cfr.positionMap.find(key);
                if (it == cfr.positionMap.end()) continue;
                rows.push_back({handVal, flushInfo, it->second.visits, &it->second});
                totalVisits += it->second.visits;
            }
        }
    }

    // Step-by-step trail of the line so the frontend can draw the action tree.
    bool trailOk = false;
    vector<TrailStep> trail;
    if (replayOk) trail = Game::BuildTrail(cfr.stack0, cfr.stack1, history, trailOk);

    ostringstream out;
    out << fixed << setprecision(3);
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
        out << "[";
        for (size_t t = 0; t < trail.size(); t++) {
            const TrailStep &s = trail[t];
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
            if (t + 1 < trail.size()) out << ", ";
        }
        out << "]";
    }

    out << ",\n  \"visits\": " << totalVisits << ",\n  \"rows\": [\n";
    for (size_t r = 0; r < rows.size(); r++) {
        const Row &row = rows[r];
        vector<double> strat = row.node->GetFinalStrategy(row.node->possible_actions);
        out << "    {\"rank\": ";
        WriteJSONString(out, RankName(row.handVal));
        out << ", \"flush\": " << row.flushInfo;
        if (stage >= FLOP) {
            out << ", \"flushLabel\": ";
            WriteJSONString(out, FlushLabel(stage, row.flushInfo));
        }
        out << ", \"visits\": " << row.visits << ", \"strategy\": [";
        bool first = true;
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (!row.node->possible_actions[i]) continue;
            if (strat[i] < 5e-4) continue; // drop actions that round to 0 (never shown)
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
        out << (r + 1 < rows.size() ? ",\n" : "\n");
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace Position
