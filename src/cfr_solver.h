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
        // false = record exact chip amounts in the history; true = ratio buckets.
        bool useBetSizeBuckets = false;

        CFRSolver();
        ~CFRSolver();
        Node *GetNode(const string &hash, const vector<bool> &possible_actions);
        void TrainCFR();
        double CFR(Game &g, double p1, double p2);

        // Wipes all trained data and reconfigures stacks/abstraction.
        void Reset(int newStack0, int newStack1, bool newUseBetSizeBuckets);

        // Time-averaged strategy, uniform over legal actions if never visited.
        vector<double> GetAverageStrategy(const string &hash, const vector<bool> &possible_actions);

        // Monte-Carlo best response: br_player best-responds, opponent plays
        // its average strategy. Player-0-perspective value (see .cpp).
        double BestResponseValue(Game &g, int br_player);

        // Exploitability (avg of both players' best-response gains), chips/hand.
        double EstimateExploitability(int num_samples);
};
