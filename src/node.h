#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

using namespace std;

class Game;
#include "game.h"

struct ParsedHash {
    int handVal;
    int board0Val;
    int board1Val;
    int flushInfo;
    int sprCat;
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

    static char GetBetAction(int pot, int bet_size);
    static char GetRaiseAction(int bet_size, int last_bet_size);
    static string GetHash(Game &g);
    static ParsedHash ParseHash(const string& full_hash);
};
