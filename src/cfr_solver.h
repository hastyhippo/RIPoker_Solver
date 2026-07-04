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

class CFRSolver {
    private:
    public:
        unordered_map<string, Node*> positionMap;
        unordered_map<string, int> positionCount;
        long long iteration = 0;
        int stack0 = STARTING_STACK, stack1 = STARTING_STACK;
        // Defaults to exact chip amounts, not percentage-of-pot buckets: the
        // percentage grid (bet_sizings/raise_sizings in game.cpp) still
        // determines which sizes are *offered*, but once one is chosen, the
        // exact resulting chip amount is what gets recorded in the history by
        // default - bucketing by ratio is opt-in via this toggle.
        bool useBetSizeBuckets = false;

        CFRSolver();
        ~CFRSolver();
        Node *GetNode(const string &hash, const vector<bool> &possible_actions);
        void TrainCFR();
        double CFR(Game &g, double p1, double p2);

        // Wipes all trained data (every Node, positionCount, the iteration
        // counter) and configures the stacks/abstraction granularity used by
        // subsequent TrainCFR() calls. Needed whenever the game itself
        // changes (stack sizes, or what an infoset even means), since
        // previously-trained strategies belong to a different game.
        void Reset(int newStack0, int newStack1, bool newUseBetSizeBuckets);

        // Looks up a node's time-averaged strategy, falling back to uniform
        // over legal actions if the node hasn't accumulated any weight yet.
        vector<double> GetAverageStrategy(const string &hash, const vector<bool> &possible_actions);

        // Monte-Carlo best response: br_player plays the best available
        // action at each of their own decision nodes, the opponent plays
        // their trained average strategy. Returns utility for whoever is to
        // act at the (sampled) game's current node - mirrors CFR()'s convention.
        double BestResponseValue(Game &g, int br_player);

        // Estimated exploitability (average of both players' best-response
        // gains against the current average strategy) over num_samples
        // randomly sampled hands, in chips/hand.
        double EstimateExploitability(int num_samples);
};
