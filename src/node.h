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

        Node(const vector<bool> &actions);

        // possible_actions is always taken as a parameter here (the CURRENT
        // concrete game state's real legality), never read from the stored
        // `possible_actions` member, which can be widened past what's legal
        // in any single state when the SPR/history abstraction maps distinct
        // concrete states onto the same hash. Using the member here would let
        // strategy[] spread mass over actions illegal in the current state,
        // making normalising_sum computed elsewhere (over only the true
        // legal set) too small and blowing up chance = strategy[i]/norm.
        void UpdateStrategy(const vector<bool> &possible_actions);
        void UpdateRegret(const vector<double> &new_regret, const vector<bool> &possible_actions, double opp_reach, double own_reach, long long iteration);
        vector<double> GetFinalStrategy(const vector<bool> &possible_actions);

    // useBuckets=true: today's threshold-bucketed single-character behavior.
    // useBuckets=false: an unambiguous exact-chip-amount token instead - "[N]"
    // for bets, "{N}" for raises (distinct delimiters so the decoder can tell
    // them apart; neither collides with the bucketed alphabet of digits/
    // A-F/a/c/f). Check (bet_size==0) always returns "0" either way.
    static string GetBetAction(int pot, int bet_size, bool useBuckets);
    static string GetRaiseAction(int bet_size, int last_bet_size, bool useBuckets);
    static string GetHash(Game &g);
    static ParsedHash ParseHash(const string& full_hash);

    // Packs the card/stage components (hand, board, flush, stage) into a
    // single integer. The money state (pot, facing bet) is NOT packed in -
    // those can exceed the few spare bits, so BuildKey carries them as plain
    // decimal fields in the key string instead.
    static uint32_t BuildHash(int handVal, int board0Val, int board1Val, int flushInfo, int stage);

    // The full infoset key: "<packedHash>|<pot>|<bet>|<abstractHistory>".
    // Shared by GetHash(Game&) (which derives every component from a live
    // game state) and by any caller building a lookup key from raw components
    // with no live Game object (e.g. a server answering "what's the strategy
    // at position X"), so both always produce byte-identical keys.
    static string BuildKey(int handVal, int board0Val, int board1Val, int flushInfo, int stage, int pot, int bet, const string &history);
};
