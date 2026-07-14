#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <map>
#include <cstdint>
#include <unordered_map>
#include "node.h"
#include "defines.h"

class Game;
class Node;

using namespace std;

// Regret handling only - how regret sums accumulate and turn into the current
// strategy. Averaging weight is the separate AvgWeighting axis below.
enum CFRVariant : int {
    VARIANT_VANILLA = 0,   // signed regrets accumulate
    VARIANT_CFR_PLUS = 1,  // RM+: regrets floored at 0
    VARIANT_PCFR_PLUS = 2, // predictive RM+: floored, strategy from regrets + last regret
    NUM_CFR_VARIANTS = 3,
};

// How each iteration's strategy is weighted into the time-average (the
// strategy the UI shows and exploitability measures).
enum AvgWeighting : int {
    WEIGHT_UNIFORM = 0,   // equal weighting
    WEIGHT_LINEAR = 1,    // weight t
    WEIGHT_QUADRATIC = 2, // weight t^2 used mostly with PCFR+
    NUM_AVG_WEIGHTINGS = 3,
};

class CFRSolver {
    private:
    public:
        // Node stored by value; unordered_map guarantees reference stability,
        // so Node* handed out by GetNode stays valid across inserts.
        unordered_map<string, Node> positionMap;
        long long iteration = 0;
        int stack0 = STARTING_STACK, stack1 = STARTING_STACK;

        // The (CFRVariant, AvgWeighting) pair this solver trains with. Only
        // changed via Reset() - regret/average sums from different configs
        // are not comparable, so no mid-run switching.
        int variant = VARIANT_CFR_PLUS;
        int weighting = WEIGHT_LINEAR;

        // Human-readable labels (shared by the server and WASM JSON).
        static const char *VariantName(int variant);
        static const char *WeightingName(int weighting);

        Node *GetNode(const string &hash, const vector<bool> &possible_actions);
        void TrainCFR();
        double CFR(Game &g, double p1, double p2);

        // Wipes all trained data and reconfigures stacks + algorithm config.
        void Reset(int newStack0, int newStack1, int newVariant = VARIANT_CFR_PLUS, int newWeighting = WEIGHT_LINEAR);

        // Time-averaged strategy, uniform over legal actions if never visited.
        vector<double> GetAverageStrategy(const string &hash, const vector<bool> &possible_actions);

        // Exact best response over the public tree: br_player maximizes vs the
        // opponent's average strategy and range. Returns br_player's own EV.
        double ExactBestResponseValue(int br_player);

        // Exploitability (avg of both BR gains), chips/hand; ~0 at equilibrium.
        // chanceSamples > 0 Monte-Carlo samples that many board cards per
        // reveal (fast, small upward bias); 0 branches every card exactly.
        double ComputeExploitability(int chanceSamples = 0);

        // Board cards sampled per reveal during a BR walk (0 = branch all).
        int brChanceSamples = 0;

        // BR recursion helpers (see .cpp): V[brCard] = range-weighted EV sums.
        vector<double> BRWalk(Game &g, int br_player, const vector<double> &oppReach, int b0, int b1);
        void BREvalTerminal(Game &g, int br_player, const vector<double> &oppReach, int b0, int b1, vector<double> &V);
};
