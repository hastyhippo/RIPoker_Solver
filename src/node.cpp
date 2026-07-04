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

/*
    Infoset key format: "<packedHash>|<pot>|<bet>|<abstractHistory>"

    packedHash bit layout (bits, low to high):
      0-3   : hole card value
      4-7   : board[0] value (only set once stage >= FLOP)
      8-11  : board[1] value (only set once stage >= TURN)
      12-13 : suit connection info
        0 -> All different
        1 -> 1 Board + Hole Card same (Flush Draw for flop, Brick for turn)
        2 -> 2 Board + Hole Card same (Flush complete)
        3 -> Board has same suits (Flush on board, no flush for you)
      14-15 : stage (PREFLOP/FLOP/TURN)

    The money state is keyed on the ACTUAL chip amounts rather than a
    stack-to-pot ratio bucket: <pot> is the total chips in play at this
    decision (committed pot + both players' current-street bets) and <bet> is
    what the acting player must add to call (0 when not facing a bet). These
    are plain decimal fields in the key string (not bit-packed) because they
    can exceed the spare bits above; abstractHistory never contains '|', so
    the three leading '|'-delimited fields parse unambiguously.

    Board bits are only ever OR'd in once that street's card has actually been
    dealt (stage-gated), so no information about undealt cards is ever encoded.

    Betting sequence string - two encodings depending on Game::useBetSizeBuckets:
      Bucketed (default), one character per action:
        '0' - Check
        '1' - min-25%
        '2' - 25%->40%
        '3' - 40%->60%
        '4' - 60%->85%
        '5' - 85%->110%
        '6' - 110%->150%
        '7' - 150%->200%
        '8' - 200%->300%
        '9' - 300%+
        'a' - allin
        'c' - call
        'f' - fold

        'A' - raise 2x-2.4x
        'B' - raise 2.4x - 2.8x
        'C' - raise 2.8x - 3.4x
        'D' - raise 3.4x - 4x
        'E' - raise 4x - 5x
        'F' - 5x+
      Exact, bets/raises become a delimited exact chip amount instead of a
      single letter (check/call/fold/allin are unaffected either way):
        "[N]" - bet of exactly N chips
        "{N}" - raise to exactly N chips
*/

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

    // Bits 0-3: hole card value. Always known to the acting player.
    hash |= (uint32_t)handVal;

    // Bits 4-7 / 8-11: board cards. Only included once that street's card has
    // actually been dealt, so no future/undealt card ever leaks into the hash.
    if (stage >= FLOP) hash |= ((uint32_t)board0Val << 4);
    if (stage >= TURN) hash |= ((uint32_t)board1Val << 8);

    // Bits 12-13: suit connection info (only meaningful once relevant cards are dealt).
    if (stage >= FLOP) hash |= ((uint32_t)flushInfo << 12);

    // Bits 14-15: stage, tagged explicitly so it can always be recovered reliably.
    hash |= (uint32_t)stage << 14;

    return hash;
}

string Node::BuildKey(int handVal, int board0Val, int board1Val, int flushInfo, int stage, int pot, int bet, const string &history) {
    uint32_t hash = BuildHash(handVal, board0Val, board1Val, flushInfo, stage);
    return to_string(hash) + "|" + to_string(pot) + "|" + to_string(bet) + "|" + history;
}

string Node::GetHash(Game &g) {
    int player = g.player;
    int flushInfo = 0;

    // Suit connection info depends on the live game's actual suits, which
    // BuildHash (given only ranks) can't recompute - derive it here.
    if (g.stage == FLOP) {
        if (g.hands[player].getSuit() == g.board[0].getSuit()) flushInfo = 1;
    } else if (g.stage == TURN) {
        //bricked flush draw
        if ((g.hands[player].getSuit() == g.board[0].getSuit()) && (g.board[0].getSuit() != g.board[1].getSuit())) flushInfo = 1;
        //flush completes
        if ((g.hands[player].getSuit() == g.board[0].getSuit()) && (g.board[0].getSuit() == g.board[1].getSuit())) flushInfo = 2;
        // flush on board
        if ((g.hands[player].getSuit() != g.board[0].getSuit()) && (g.board[0].getSuit() == g.board[1].getSuit())) flushInfo = 3;
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

    // Key format: "<packedHash>|<pot>|<bet>|<abstractHistory>". abstractHistory
    // never contains '|', so the three leading fields split cleanly and the
    // history is everything past the third separator.
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
        // Counterfactual regret: weighted by the probability the OPPONENT
        // (not the acting player) contributed to reaching this node.
        regret_sum[i] += opp_reach * new_regret[i];
        // CFR+-style flooring: clamp accumulated regret to zero immediately,
        // not just when strategy is derived from it.
        regret_sum[i] = max(0.0, regret_sum[i]);
        // Linear-CFR averaging: weight this iteration's contribution to the
        // running average strategy by both the iteration number and the
        // acting player's own reach probability.
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

    // strategy[] is fully recomputed (zeroed for non-legal indices) every
    // call, so it always corresponds exactly to THIS call's possible_actions
    // - it never carries over mass from a different legal-action set.
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
        // No accumulated weight yet (e.g. a freshly-created or rarely-visited
        // node) - fall back to uniform over legal actions rather than NaN.
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