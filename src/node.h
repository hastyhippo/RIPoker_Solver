#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <cstdint>

using namespace std;

class Game;
#include "game.h"

struct ParsedHash {
    int handVal;
    int board0Val;
    int board1Val;
    int flushInfo;
    int pot; // total chips in play at this decision (committed pot + both players' current-street bets)
    int bet; // chips the acting player must put in to call (0 when not facing a bet)
    int stage;
    string abstractHistory;
};

class Node {
    private:
    public:
        vector<double> strategy;
        vector<double> strategy_sum;
        vector<double> regret_sum;
        vector<bool> possible_actions;
        int visits = 0; // times the trainer hit this infoset

        Node(const vector<bool> &actions);

        // possible_actions is the CURRENT state's real legality (passed in),
        // never the stored member, which the abstraction may have widened.
        void UpdateStrategy(const vector<bool> &possible_actions);
        void UpdateRegret(const vector<double> &new_regret, const vector<bool> &possible_actions, double opp_reach, double own_reach, long long iteration);
        vector<double> GetFinalStrategy(const vector<bool> &possible_actions);

    // History token for a bet/raise: the exact chip amount "[N]"/"{N}".
    // Check (bet_size==0) always returns "0".
    static string GetBetAction(int bet_size);
    static string GetRaiseAction(int bet_size);
    static string GetHash(Game &g);
    static ParsedHash ParseHash(const string& full_hash);

    // Packs hand/board/flush/stage into one int (money state lives in the key).
    static uint32_t BuildHash(int handVal, int board0Val, int board1Val, int flushInfo, int stage);

    // Full infoset key "<packedHash>|<pot>|<bet>|<history>"; used by both the
    // trainer and lookups so they produce byte-identical keys.
    static string BuildKey(int handVal, int board0Val, int board1Val, int flushInfo, int stage, int pot, int bet, const string &history);
};
