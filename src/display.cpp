#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <vector>

#include "display.h"
#include "game.h"
#include "card.h"
#include "node.h"
#include "cfr_solver.h"
#include "defines.h"

using namespace std;

namespace {
    const string RESET   = "\033[0m";
    const string BOLD     = "\033[1m";
    const string DIM       = "\033[2m";
    const string CYAN     = "\033[36m";
    const string GREEN    = "\033[32m";
    const string YELLOW   = "\033[33m";
    const string MAGENTA  = "\033[35m";
    const string RED      = "\033[31m";
    const string BLUE     = "\033[34m";

    // move_names' raw entries ("B50", "R2.2", ...) are used as stable keys
    // elsewhere (the web viewers key colors off these exact strings), so
    // rather than rename them, translate to a clearer label only where text
    // is actually shown to a user - spelling out the ratio instead of the
    // abbreviated code.
    string DisplayLabel(const string &moveName) {
        if (moveName == "B50") return "Bet 50%";
        if (moveName == "B100") return "Bet 100%";
        if (moveName == "B200") return "Bet 200%";
        if (moveName == "R2.2") return "Raise 2.2x";
        if (moveName == "R2.6") return "Raise 2.6x";
        if (moveName == "R3") return "Raise 3x";
        if (moveName == "R4") return "Raise 4x";
        if (moveName == "Allin") return "All-in";
        return moveName; // Check, Call, Fold
    }

    string RankName(int value) {
        // Reuses Card::getName() (suit 0 = Spades) so labelling stays
        // consistent with the rest of the codebase, then strips the suit.
        Card c(value * 4);
        string name = c.getName();
        return name.substr(0, name.find(" of"));
    }

    string StageName(int stage) {
        if (stage == PREFLOP) return "Preflop";
        if (stage == FLOP) return "Flop";
        if (stage == TURN) return "Turn";
        return "?";
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

    // Builds a solid unicode bar of the given width using block characters.
    string BlockBar(double frac, int width = 20) {
        int filled = (int)round(max(0.0, min(1.0, frac)) * width);
        filled = max(0, min(width, filled));
        string bar;
        for (int i = 0; i < filled; i++) bar += "\xE2\x96\x88"; // "█"
        for (int i = filled; i < width; i++) bar += "\xC2\xB7";  // "·"
        return bar;
    }

    string Line(int width = 66) {
        string s;
        for (int i = 0; i < width; i++) s += "\xE2\x94\x80"; // "─"
        return s;
    }

    // Percent-formats a pot-fraction threshold with no trailing decimals
    // when it's a whole number (e.g. 0.5 -> "50", 0.55 -> "55").
    string Pct(double frac) {
        ostringstream ss;
        ss << (int)round(frac * 100);
        return ss.str();
    }

    string Mult(double m) {
        ostringstream ss;
        ss << fixed << setprecision(1) << m;
        string s = ss.str();
        if (s.size() >= 2 && s[s.size() - 1] == '0' && s[s.size() - 2] == '.') s.resize(s.size() - 2);
        return s;
    }

    // Translates one abstractHistory character (see Node::GetHash's comment
    // block for the encoding) into a human-readable action description.
    string ActionLetterName(char c) {
        switch (c) {
            case '0': return "Check";
            case 'c': return "Call";
            case 'f': return "Fold";
            case 'a': return "All-in";
            case '1': return "Bet <" + Pct(BET_1_MAX) + "%pot";
            case '2': return "Bet " + Pct(BET_1_MAX) + "-" + Pct(BET_2_MAX) + "%pot";
            case '3': return "Bet " + Pct(BET_2_MAX) + "-" + Pct(BET_3_MAX) + "%pot";
            case '4': return "Bet " + Pct(BET_3_MAX) + "-" + Pct(BET_4_MAX) + "%pot";
            case '5': return "Bet " + Pct(BET_4_MAX) + "-" + Pct(BET_5_MAX) + "%pot";
            case '6': return "Bet " + Pct(BET_5_MAX) + "-" + Pct(BET_6_MAX) + "%pot";
            case '7': return "Bet " + Pct(BET_6_MAX) + "-" + Pct(BET_7_MAX) + "%pot";
            case '8': return "Bet " + Pct(BET_7_MAX) + "-" + Pct(BET_8_MAX) + "%pot";
            case '9': return "Bet " + Pct(BET_8_MAX) + "%+pot";
            case 'A': return "Raise <" + Mult(RAISE_A_MAX) + "x";
            case 'B': return "Raise " + Mult(RAISE_A_MAX) + "-" + Mult(RAISE_B_MAX) + "x";
            case 'C': return "Raise " + Mult(RAISE_B_MAX) + "-" + Mult(RAISE_C_MAX) + "x";
            case 'D': return "Raise " + Mult(RAISE_C_MAX) + "-" + Mult(RAISE_D_MAX) + "x";
            case 'E': return "Raise " + Mult(RAISE_D_MAX) + "-" + Mult(RAISE_E_MAX) + "x";
            case 'F': return "Raise " + Mult(RAISE_E_MAX) + "x+";
        }
        return string(1, c);
    }

    // Expands the compact abstractHistory code (e.g. "07AA,0") into a full,
    // street-by-street, human-readable action sequence. Handles both the
    // bucketed single-character alphabet and the exact-bet-sizing tokens
    // ("[N]" for a bet of exactly N chips, "{N}" for a raise to exactly N).
    string DecodeAbstractHistory(const string &hist) {
        if (hist.empty()) return "(no actions yet this street)";

        static const vector<string> streetNames = {"Preflop", "Flop", "Turn"};
        vector<string> streets;
        string current;
        for (size_t i = 0; i < hist.size(); i++) {
            char c = hist[i];
            if (c == ',') {
                streets.push_back(current);
                current.clear();
                continue;
            }
            if (!current.empty()) current += " -> ";
            if (c == '[' || c == '{') {
                char close = (c == '[') ? ']' : '}';
                size_t end = hist.find(close, i + 1);
                string amount = (end == string::npos) ? hist.substr(i + 1) : hist.substr(i + 1, end - i - 1);
                current += (c == '[' ? "Bet " : "Raise to ") + amount + " chips (exact)";
                i = (end == string::npos) ? hist.size() - 1 : end;
            } else {
                current += ActionLetterName(c);
            }
        }
        streets.push_back(current);

        string result;
        for (size_t i = 0; i < streets.size(); i++) {
            if (streets[i].empty()) continue;
            if (!result.empty()) result += "  |  ";
            result += (i < streetNames.size() ? streetNames[i] : "?") + ": " + streets[i];
        }
        return result;
    }
}

namespace Display {

void PrintTrainingProgress(long long iteration, long long totalIterations, size_t numNodes) {
    cout << CYAN << "[train] " << RESET
         << "hand " << BOLD << iteration << RESET << "/" << totalIterations
         << DIM << "  |  infosets: " << numNodes << RESET << "\n";
}

void PrintExploitability(long long iteration, double exploitability) {
    cout << MAGENTA << "  \xE2\x86\xB3 " << RESET
         << "exploitability @ hand " << iteration << ": "
         << BOLD << fixed << setprecision(4) << exploitability << RESET
         << " chips/hand (avg of both players' best response gain)\n";
}

void PrintFinalStrategyReport(CFRSolver &cfr, int topN) {
    cout << "\n" << BOLD << YELLOW << Line() << RESET << "\n";
    cout << BOLD << YELLOW << "  FINAL STRATEGY REPORT" << RESET
         << DIM << "  (top " << topN << " most-visited nodes, of " << cfr.positionMap.size() << " total)" << RESET << "\n";
    cout << BOLD << YELLOW << Line() << RESET << "\n";

    vector<pair<string, Node*>> rows(cfr.positionMap.begin(), cfr.positionMap.end());
    sort(rows.begin(), rows.end(), [&](const pair<string, Node*> &a, const pair<string, Node*> &b) {
        return cfr.positionCount[a.first] > cfr.positionCount[b.first];
    });
    if ((int)rows.size() > topN) rows.resize(topN);

    for (auto &entry : rows) {
        const string &hash = entry.first;
        Node *node = entry.second;
        ParsedHash p = Node::ParseHash(hash);

        cout << "\n" << BOLD << CYAN << "Hand: " << RESET << RankName(p.handVal);
        if (p.stage >= FLOP) cout << BOLD << "  Board: " << RESET << RankName(p.board0Val);
        if (p.stage >= TURN) cout << " " << RankName(p.board1Val);
        cout << DIM << "  [Stage: " << StageName(p.stage) << ", Flush: " << FlushInfoName(p.flushInfo)
             << ", Pot: " << p.pot << ", To call: " << p.bet << "]" << RESET << "\n";
        cout << DIM << "Raw history code: " << (p.abstractHistory.empty() ? "(none)" : p.abstractHistory)
             << "  |  visits: " << cfr.positionCount[hash] << RESET << "\n";
        cout << DIM << "Action sequence: " << RESET << DecodeAbstractHistory(p.abstractHistory) << "\n";

        vector<double> strat = node->GetFinalStrategy(node->possible_actions);
        vector<pair<int, double>> actions;
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (node->possible_actions[i]) actions.push_back({i, strat[i]});
        }
        sort(actions.begin(), actions.end(), [](const pair<int, double> &a, const pair<int, double> &b) {
            return a.second > b.second;
        });

        for (auto &a : actions) {
            cout << "  " << setw(12) << left << DisplayLabel(move_names[a.first]) << RESET
                 << GREEN << BlockBar(a.second) << RESET
                 << "  " << fixed << setprecision(1) << (a.second * 100.0) << "%\n";
        }
    }
    cout << "\n" << BOLD << YELLOW << Line() << RESET << "\n\n";
}

void PrintPreflopStrategy(CFRSolver &cfr) {
    cout << "\n" << BOLD << YELLOW << Line() << RESET << "\n";
    cout << BOLD << YELLOW << "  PREFLOP STRATEGY" << RESET
         << DIM << "  (first action of the hand, by hole card)" << RESET << "\n";
    cout << BOLD << YELLOW << Line() << RESET << "\n";

    // One slot per hole card value; keep whichever node (there should only
    // ever be one at a fixed starting stack) has the most visits, defensively
    // in case the money-state key ever splits the opening decision.
    int numHandValues = NUM_CARDS / 4;
    vector<pair<string, Node*>> best(numHandValues, {"", nullptr});

    for (auto &entry : cfr.positionMap) {
        ParsedHash p = Node::ParseHash(entry.first);
        if (p.stage != PREFLOP || !p.abstractHistory.empty()) continue;
        if (p.handVal < 0 || p.handVal >= numHandValues) continue;
        if (!best[p.handVal].second || cfr.positionCount[entry.first] > cfr.positionCount[best[p.handVal].first]) {
            best[p.handVal] = entry;
        }
    }

    for (int handVal = 0; handVal < numHandValues; handVal++) {
        if (!best[handVal].second) continue;
        const string &hash = best[handVal].first;
        Node *node = best[handVal].second;
        ParsedHash p = Node::ParseHash(hash);

        cout << "\n" << BOLD << CYAN << "Hand: " << RESET << RankName(handVal)
             << DIM << "  [Stage: " << StageName(p.stage) << ", Pot: " << p.pot << ", To call: " << p.bet << "]"
             << "  |  visits: " << cfr.positionCount[hash] << RESET << "\n";
        cout << DIM << "Action sequence: " << RESET << DecodeAbstractHistory(p.abstractHistory) << "\n";

        vector<double> strat = node->GetFinalStrategy(node->possible_actions);
        vector<pair<int, double>> actions;
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (node->possible_actions[i]) actions.push_back({i, strat[i]});
        }
        sort(actions.begin(), actions.end(), [](const pair<int, double> &a, const pair<int, double> &b) {
            return a.second > b.second;
        });

        for (auto &a : actions) {
            cout << "  " << setw(12) << left << DisplayLabel(move_names[a.first]) << RESET
                 << GREEN << BlockBar(a.second) << RESET
                 << "  " << fixed << setprecision(1) << (a.second * 100.0) << "%\n";
        }
    }
    cout << "\n" << BOLD << YELLOW << Line() << RESET << "\n\n";
}

void PrintBoard(Game &g, int humanPlayer) {
    cout << "\n" << BOLD << BLUE << Line() << RESET << "\n";
    cout << BOLD << "  " << StageName(g.stage) << RESET
         << DIM << "  |  Pot: " << RESET << BOLD << g.pot << RESET << "\n";

    cout << "  Board: ";
    if (g.stage >= FLOP) cout << g.board[0].getName(); else cout << DIM << "?" << RESET;
    cout << "  ";
    if (g.stage >= TURN) cout << g.board[1].getName(); else cout << DIM << "?" << RESET;
    cout << "\n";

    cout << "  Your hand: " << BOLD << g.hands[humanPlayer].getName() << RESET << "\n";
    cout << "  Stacks -> You: " << g.effective_stack[humanPlayer]
         << "  |  Opponent: " << g.effective_stack[1 - humanPlayer] << "\n";
    cout << BOLD << BLUE << Line() << RESET << "\n";
}

void PrintActionMenu(const vector<bool> &actions, Game &g) {
    cout << YELLOW << "Available actions:" << RESET << "\n";
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (!actions[i]) continue;
        cout << "  " << BOLD << i << RESET << ") ";
        // Leads with the exact chip amount here (unlike the aggregate
        // strategy reports, which have no single pot to compute one from) -
        // more directly useful than the abstracted "B200"-style label when
        // there's a real, current pot to size against.
        if (i >= MISC_ACTIONS && i < MISC_ACTIONS + NUM_BETS) {
            cout << "Bet " << (int)(g.pot * bet_sizings[i - MISC_ACTIONS]) << " chips"
                 << DIM << "  (" << DisplayLabel(move_names[i]) << ")" << RESET;
        } else if (i >= MISC_ACTIONS + NUM_BETS && i < MISC_ACTIONS + NUM_BETS + NUM_RAISES) {
            int target = (int)(g.bet_states[g.stage][1 - g.player] * raise_sizings[i - (MISC_ACTIONS + NUM_BETS)]);
            cout << "Raise to " << target << " chips"
                 << DIM << "  (" << DisplayLabel(move_names[i]) << ")" << RESET;
        } else {
            cout << DisplayLabel(move_names[i]);
        }
        cout << "\n";
    }
}

void PrintHandResult(Game &g, int humanPlayer, int utilityForActingPlayer) {
    int humanUtility = (g.player == humanPlayer) ? utilityForActingPlayer : -utilityForActingPlayer;
    cout << "\n" << BOLD << (humanUtility >= 0 ? GREEN : RED)
         << (humanUtility >= 0 ? "You win " : "You lose ") << abs(humanUtility) << " chips" << RESET << "\n\n";
}

void PrintOptimalStrategy(const vector<double> &strategy, const vector<bool> &possible_actions, bool isHumanTurn) {
    cout << MAGENTA << (isHumanTurn ? "Solver's optimal strategy here" : "Opponent's optimal strategy here")
         << RESET << ":\n";

    vector<pair<int, double>> actions;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (possible_actions[i]) actions.push_back({i, strategy[i]});
    }
    sort(actions.begin(), actions.end(), [](const pair<int, double> &a, const pair<int, double> &b) {
        return a.second > b.second;
    });

    for (auto &a : actions) {
        cout << "  " << setw(12) << left << DisplayLabel(move_names[a.first]) << RESET
             << GREEN << BlockBar(a.second, 12) << RESET
             << "  " << fixed << setprecision(1) << (a.second * 100.0) << "%\n";
    }
}

string ActionDisplayLabel(const string &moveName) {
    return DisplayLabel(moveName);
}

}
