#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <stdint.h>
#include <vector>
#include <bitset>

#include "node.h"
#include "game.h"
#include "defines.h"
using namespace std;

// Infoset key: "<packedHash>|<pot>|<bet>|<history>". packedHash bits: 0-3 hand,
// 4-11 board (stage-gated), 12-13 flush, 14-15 stage. History: see Get*Action.

Node::Node(const vector<bool> &actions) {
    strategy_sum = vector<double>(NUM_ACTIONS, 0);
    regret_sum = vector<double>(NUM_ACTIONS, 0);
    strategy = vector<double>(NUM_ACTIONS, 0);

    int n = 0; for (auto a : actions) if (a) n++;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (actions[i]) strategy[i] = 1.0/(double)n;
    }
    possible_actions = actions;
}

string Node::GetBetAction(int pot, int bet_size, bool useBuckets) {
    if (bet_size == 0) {
        //Check call
        return "0";
    }
    if (!useBuckets) {
        return "[" + to_string(bet_size) + "]";
    }
    double ratio = (double)bet_size / pot;
    if (ratio < BET_1_MAX) {
        return "1";
    } else if (ratio < BET_2_MAX) {
        return "2";
    } else if (ratio < BET_3_MAX) {
        return "3";
    }else if (ratio < BET_4_MAX) {
        return "4";
    } else if (ratio < BET_5_MAX) {
        return "5";
    } else if (ratio < BET_6_MAX) {
        return "6";
    } else if (ratio < BET_7_MAX) {
        return "7";
    } else if (ratio < BET_8_MAX) {
        return "8";
    } else {
        return "9";
    }
}

// RAISES ONLY
string Node::GetRaiseAction(int bet_size, int last_bet_size, bool useBuckets) {
    if (!useBuckets) {
        return "{" + to_string(bet_size) + "}";
    }
    double ratio = (double)bet_size / last_bet_size;
    if (ratio < RAISE_A_MAX) {
        return "A";
    } else if (ratio < RAISE_B_MAX) {
        return "B";
    } else if (ratio < RAISE_C_MAX) {
        return "C";
    } else if (ratio < RAISE_D_MAX) {
        return "D";
    } else if (ratio < RAISE_E_MAX) {
        return "E";
    } else {
        return "F";
    }
}

uint32_t Node::BuildHash(int handVal, int board0Val, int board1Val, int flushInfo, int stage) {
    uint32_t hash = 0;
    // Board/flush are stage-gated so no undealt card ever leaks into the hash.
    hash |= (uint32_t)handVal;                                  // 0-3
    if (stage >= FLOP) hash |= ((uint32_t)board0Val << 4);      // 4-7
    if (stage >= TURN) hash |= ((uint32_t)board1Val << 8);      // 8-11
    if (stage >= FLOP) hash |= ((uint32_t)flushInfo << 12);     // 12-13
    hash |= (uint32_t)stage << 14;                              // 14-15
    return hash;
}

string Node::BuildKey(int handVal, int board0Val, int board1Val, int flushInfo, int stage, int pot, int bet, const string &history) {
    uint32_t hash = BuildHash(handVal, board0Val, board1Val, flushInfo, stage);
    return to_string(hash) + "|" + to_string(pot) + "|" + to_string(bet) + "|" + history;
}

string Node::GetHash(Game &g) {
    int player = g.player;
    int flushInfo = 0;

    // Flush info depends on live suits, which BuildHash (ranks only) can't see.
    if (g.stage == FLOP) {
        if (g.hands[player].getSuit() == g.board[0].getSuit()) flushInfo = 1;
    } else if (g.stage == TURN) {
        if ((g.hands[player].getSuit() == g.board[0].getSuit()) && (g.board[0].getSuit() != g.board[1].getSuit())) flushInfo = 1; // bricked draw
        if ((g.hands[player].getSuit() == g.board[0].getSuit()) && (g.board[0].getSuit() == g.board[1].getSuit())) flushInfo = 2; // completes
        if ((g.hands[player].getSuit() != g.board[0].getSuit()) && (g.board[0].getSuit() == g.board[1].getSuit())) flushInfo = 3; // on board
    }

    return BuildKey(
        g.hands[player].getValue(),
        g.board[0].getValue(),
        g.board[1].getValue(),
        flushInfo,
        g.stage,
        g.KeyPot(),
        g.KeyBet(),
        g.abstractHistory
    );
}

ParsedHash Node::ParseHash(const string& full_hash) {
    ParsedHash parsed{};

    // history never contains '|', so the three leading fields split cleanly.
    size_t s1 = full_hash.find('|');
    size_t s2 = (s1 == string::npos) ? string::npos : full_hash.find('|', s1 + 1);
    size_t s3 = (s2 == string::npos) ? string::npos : full_hash.find('|', s2 + 1);
    if (s3 == string::npos) {
        cerr << "Invalid hash format.\n";
        return parsed;
    }

    uint32_t hash = (uint32_t)stoul(full_hash.substr(0, s1));
    parsed.handVal = hash & 0xF;                 // bits 0-3
    parsed.board0Val = (hash >> 4) & 0xF;        // bits 4-7
    parsed.board1Val = (hash >> 8) & 0xF;        // bits 8-11
    parsed.flushInfo = (hash >> 12) & 0x3;       // bits 12-13
    parsed.stage = (hash >> 14) & 0x3;           // bits 14-15
    parsed.pot = stoi(full_hash.substr(s1 + 1, s2 - s1 - 1));
    parsed.bet = stoi(full_hash.substr(s2 + 1, s3 - s2 - 1));
    parsed.abstractHistory = full_hash.substr(s3 + 1);

    return parsed;
}

void Node::UpdateRegret(const vector<double> &new_regret, const vector<bool> &possible_actions, double opp_reach, double own_reach, long long iteration) {
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (!possible_actions[i]) continue;
        regret_sum[i] += opp_reach * new_regret[i];
        regret_sum[i] = max(0.0, regret_sum[i]); // CFR+ flooring
        // Linear-CFR averaging: weight by iteration number and own reach.
        strategy_sum[i] += (double)iteration * own_reach * strategy[i];
    }
}

void Node::UpdateStrategy(const vector<bool> &possible_actions) {
    double normalising_sum = 0;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (!possible_actions[i]) continue;
        normalising_sum += max(0.0, regret_sum[i]);
    }

    int n = 0;
    for (int i = 0; i < NUM_ACTIONS; i++) if (possible_actions[i]) n++;

    // Fully recomputed each call (zeroed for illegal actions), so it never
    // carries mass from a different legal-action set.
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (!possible_actions[i]) { strategy[i] = 0.0; continue; }
        strategy[i] = (normalising_sum == 0) ? 1.0 / (double)n : max(0.0, regret_sum[i] / normalising_sum);
    }
}

vector<double> Node::GetFinalStrategy(const vector<bool> &possible_actions) {
    vector<double> final_strategy(NUM_ACTIONS, 0.0);
    double normalising_sum = 0.0;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (possible_actions[i]) normalising_sum += max(0.0, strategy_sum[i]);
    }

    int n = 0;
    for (int i = 0; i < NUM_ACTIONS; i++) if (possible_actions[i]) n++;
    if (n == 0) return final_strategy; // no legal actions at all - shouldn't happen, but stay safe

    if (normalising_sum <= 0.0) {
        // No weight accumulated yet - fall back to uniform over legal actions.
        for (int i = 0; i < NUM_ACTIONS; i++) {
            final_strategy[i] = possible_actions[i] ? 1.0 / (double)n : 0.0;
        }
        return final_strategy;
    }

    for (int i = 0; i < NUM_ACTIONS; i++) {
        final_strategy[i] = possible_actions[i] ? max(0.0, strategy_sum[i]) / normalising_sum : 0.0;
    }
    return final_strategy;
}