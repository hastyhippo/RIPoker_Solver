#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include "cfr_solver.h"
#include "node.h"
#include "game.h"
#include "defines.h"

CFRSolver::CFRSolver() {

}

CFRSolver::~CFRSolver() {
    for (auto &entry : positionMap) delete entry.second;
}

void CFRSolver::Reset(int newStack0, int newStack1, bool newUseBetSizeBuckets) {
    for (auto &entry : positionMap) delete entry.second;
    positionMap.clear();
    positionCount.clear();
    iteration = 0;
    stack0 = newStack0;
    stack1 = newStack1;
    useBetSizeBuckets = newUseBetSizeBuckets;
}

void CFRSolver::TrainCFR() {
    iteration++;
    Game g(stack0, stack1, useBetSizeBuckets);
    g.InitialiseGame(0);
    CFR(g, 1, 1);
}

Node *CFRSolver::GetNode(const string &hash, const vector<bool> &possible_actions) {
    auto it = positionMap.find(hash);
    if (it == positionMap.end()) {
        Node *new_node = new Node(possible_actions);
        positionMap.emplace(hash, new_node);
        return new_node;
    }
    // The stored legality mask may not match a later visit's real
    // possible_actions; that's fine - live math always takes the current mask.
    return it->second;
}

// 2p only. Fixed player-0-perspective utilities (p1/p2 are the players' reach
// probs); avoids the alternate-and-negate scheme that broke at street resets.
double CFRSolver::CFR(Game &g, double p1, double p2) {
    if (g.terminal) {
        return g.GetUtilityForPlayer0();
    }

    // Fixed before any MakeMove below can mutate g.player.
    int actingPlayer = g.player;

    vector<bool> possible_actions = g.GetActions(false);
    string hash = Node::GetHash(g);
    Node *currentNode = GetNode(hash, possible_actions);
    positionCount[hash]++;

    currentNode->UpdateStrategy(possible_actions);
    vector<double> strategy = currentNode->strategy;

    vector<double> action_val(NUM_ACTIONS, 0.0);
    double node_utility = 0;

    double normalising_sum = 0.0;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (possible_actions[i]) normalising_sum += strategy[i];
    }

    // Player-0 utility, no per-child negation; only the acting player's reach
    // picks up this action's chance.
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (!possible_actions[i]) continue;

        double chance = strategy[i] / normalising_sum;

        g.MakeMove(i);
        if (actingPlayer == 0) {
            action_val[i] = CFR(g, p1 * chance, p2);
        } else {
            action_val[i] = CFR(g, p1, p2 * chance);
        }
        g.UnmakeMove();
        node_utility += action_val[i] * chance;
    }

    // Regret is in the acting player's perspective (negated for player 1).
    double perspective = (actingPlayer == 0) ? 1.0 : -1.0;
    vector<double> regrets(NUM_ACTIONS, 0.0);
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (possible_actions[i]) regrets[i] = perspective * (action_val[i] - node_utility);
    }

    // Regret weighted by opponent reach; average strategy by own reach.
    if (actingPlayer == 0) {
        currentNode->UpdateRegret(regrets, possible_actions, /*opp_reach=*/p2, /*own_reach=*/p1, iteration);
    } else {
        currentNode->UpdateRegret(regrets, possible_actions, /*opp_reach=*/p1, /*own_reach=*/p2, iteration);
    }

    return node_utility;
}

vector<double> CFRSolver::GetAverageStrategy(const string &hash, const vector<bool> &possible_actions) {
    auto it = positionMap.find(hash);
    if (it == positionMap.end()) {
        vector<double> uniform(NUM_ACTIONS, 0.0);
        int n = 0;
        for (int i = 0; i < NUM_ACTIONS; i++) if (possible_actions[i]) n++;
        for (int i = 0; i < NUM_ACTIONS; i++) if (possible_actions[i]) uniform[i] = 1.0 / (double)n;
        return uniform;
    }
    return it->second->GetFinalStrategy(possible_actions);
}

// Player-0-perspective value (same convention as CFR) with br_player playing
// a best response and the opponent playing its trained average strategy.
double CFRSolver::BestResponseValue(Game &g, int br_player) {
    if (g.terminal) return g.GetUtilityForPlayer0();

    vector<bool> possible_actions = g.GetActions(false);

    if (g.player == br_player) {
        // Maximise br_player's own utility, but return the line's player-0 value.
        double perspective = (br_player == 0) ? 1.0 : -1.0;
        double bestOwn = -1e18, bestP0 = 0.0;
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (!possible_actions[i]) continue;
            g.MakeMove(i);
            double childP0 = BestResponseValue(g, br_player);
            g.UnmakeMove();
            double own = perspective * childP0;
            if (own > bestOwn) { bestOwn = own; bestP0 = childP0; }
        }
        return bestP0;
    } else {
        string hash = Node::GetHash(g);
        vector<double> strat = GetAverageStrategy(hash, possible_actions);
        double node_value = 0;
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (!possible_actions[i]) continue;
            g.MakeMove(i);
            node_value += strat[i] * BestResponseValue(g, br_player);
            g.UnmakeMove();
        }
        return node_value;
    }
}

double CFRSolver::EstimateExploitability(int num_samples) {
    double br0_total = 0, br1_total = 0;
    for (int s = 0; s < num_samples; s++) {
        Game g0(stack0, stack1, useBetSizeBuckets);
        g0.InitialiseGame(0);
        br0_total += BestResponseValue(g0, 0);

        Game g1(stack0, stack1, useBetSizeBuckets);
        g1.InitialiseGame(0);
        // BestResponseValue returns player-0 utility; player 1's own
        // best-response value is its negation.
        br1_total += -BestResponseValue(g1, 1);
    }
    return (br0_total + br1_total) / (2.0 * num_samples);
}
