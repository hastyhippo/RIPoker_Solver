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
        // Node stored by value; unordered_map guarantees reference stability,
        // so Node* handed out by GetNode stays valid across inserts.
        unordered_map<string, Node> positionMap;
        long long iteration = 0;
        int stack0 = STARTING_STACK, stack1 = STARTING_STACK;

        Node *GetNode(const string &hash, const vector<bool> &possible_actions);
        void TrainCFR();
        double CFR(Game &g, double p1, double p2);

        // Wipes all trained data and reconfigures stacks.
        void Reset(int newStack0, int newStack1);

        // Time-averaged strategy, uniform over legal actions if never visited.
        vector<double> GetAverageStrategy(const string &hash, const vector<bool> &possible_actions);

        // Exact best response over the public tree: br_player maximizes vs the
        // opponent's average strategy and range. Returns br_player's own EV.
        double ExactBestResponseValue(int br_player);

        // Exact exploitability (avg of both BR gains), chips/hand; 0 at equilibrium.
        double ComputeExploitability();

        // BR recursion helpers (see .cpp): V[brCard] = range-weighted EV sums.
        vector<double> BRWalk(Game &g, int br_player, const vector<double> &oppReach, int b0, int b1);
        void BREvalTerminal(Game &g, int br_player, const vector<double> &oppReach, int b0, int b1, vector<double> &V);
};
