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

void CFRSolver::TrainCFR() {
    iteration++;
    Game g(STARTING_STACK);
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
    // Note: the SPR/bet-size abstraction can in principle map two distinct
    // concrete states onto the same hash, so the stored node's legality mask
    // (set at creation time) may not exactly match every later visit's real
    // possible_actions. That's fine - UpdateStrategy/UpdateRegret/GetFinalStrategy
    // always take the CURRENT possible_actions as a parameter and never read
    // the stored mask for live math, so a mismatch here can't corrupt the
    // regret/strategy computation. Widening the stored mask on every visit
    // used to seem like a safety improvement, but it actually broke CFR: it
    // let strategy[] (built from the stored mask) spread mass onto actions
    // illegal in the current state, making normalising_sum (summed over only
    // the true legal set) collapse toward zero and chance = strategy[i]/norm
    // blow up past 1, corrupting reach-probability weighting and producing
    // NaN/astronomical values downstream.
    return it->second;
}

// Only working for 2p
double CFRSolver::CFR(Game &g, double p1, double p2) {
    if (g.terminal) {
        return g.GetUtility();
    }

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

    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (!possible_actions[i]) continue;

        double chance = strategy[i] / normalising_sum;

        g.MakeMove(i);
        if (g.player == 0) {
            action_val[i] = -CFR(g, p1 * chance, p2);
        } else {
            action_val[i] = -CFR(g, p1, p2 * chance);
        }
        g.UnmakeMove();
        node_utility += action_val[i] * chance;
    }

    vector<double> regrets(NUM_ACTIONS, 0.0);
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (possible_actions[i]) regrets[i] = action_val[i] - node_utility;
    }

    // Counterfactual regret is weighted by the OPPONENT's reach probability;
    // the running average strategy is weighted by the ACTING player's own
    // reach probability (standard vanilla-CFR formulation).
    if (g.player == 0) {
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

double CFRSolver::BestResponseValue(Game &g, int br_player) {
    if (g.terminal) return g.GetUtility();

    vector<bool> possible_actions = g.GetActions(false);

    if (g.player == br_player) {
        double best = -1e18;
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (!possible_actions[i]) continue;
            g.MakeMove(i);
            best = max(best, -BestResponseValue(g, br_player));
            g.UnmakeMove();
        }
        return best;
    } else {
        string hash = Node::GetHash(g);
        vector<double> strat = GetAverageStrategy(hash, possible_actions);
        double node_value = 0;
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (!possible_actions[i]) continue;
            g.MakeMove(i);
            node_value += strat[i] * -BestResponseValue(g, br_player);
            g.UnmakeMove();
        }
        return node_value;
    }
}

double CFRSolver::EstimateExploitability(int num_samples) {
    double br0_total = 0, br1_total = 0;
    for (int s = 0; s < num_samples; s++) {
        Game g0(STARTING_STACK);
        g0.InitialiseGame(0);
        br0_total += BestResponseValue(g0, 0);

        Game g1(STARTING_STACK);
        g1.InitialiseGame(0);
        // BestResponseValue returns utility for whoever is to act at the
        // root (player 0, since first_to_act=0); negate to get player 1's
        // own best-response value.
        br1_total += -BestResponseValue(g1, 1);
    }
    return (br0_total + br1_total) / (2.0 * num_samples);
}
