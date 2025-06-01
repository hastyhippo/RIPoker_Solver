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

void CFRSolver::TrainCFR() {
    Game g(STARTING_STACK);
    g.InitialiseGame(0);

    double ev = CFR(g, 1,1);
    g.PrintGame(true);
    cout << "EV: " << ev<< "\n";
}

Node *CFRSolver::GetNode(string hash) {
    if(positionMap.count(hash) == 0) {
        Node *new_node = new Node();
        positionMap.emplace(hash, new_node);
        vector<double> strategy = new_node->strategy;
    } 
    return positionMap[hash];
}


// Only working for 2p 
double CFRSolver::CFR(Game &g, double p1, double p2) {
    // g.PrintGame(true);

    if (g.terminal) {
        return g.GetUtility();
    }
    // next card if applicable

    // has the position properly
    string hash = Node::GetHash(g);
    Node *currentNode = GetNode(hash);
    positionCount[hash]++;
    
    (*currentNode).UpdateStrategy();
    vector<double> strategy = currentNode->strategy;

    // cout << "RegretSum: ";
    // for (int i = 0; i < NUM_ACTIONS; i++) {
    //     cout << currentNode->regret_sum[i] << " | ";
    // }
    // cout << "\nStrategy: ";
    // for (auto a : currentNode->strategy) {
    //     cout << a << " | ";
    // }
    // cout << '\n';

    vector<double> action_val(NUM_ACTIONS);
    double node_utility = 0;

    vector<bool> possible_actions = g.GetActions(false);

    double normalising_sum = 0.0;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (possible_actions[i]) normalising_sum += strategy[i];
    }

    for (int i = 0; i < NUM_ACTIONS; i++) {
        // check if the move is vcalid
        if(!possible_actions[i]) continue;

        double chance = strategy[i]/normalising_sum;
        
        // perform action
        g.MakeMove(i);
        if (g.player == 0) {
            action_val[i] = -CFR(g, p1 * chance, p2);
        } else {
            action_val[i] = -CFR(g, p1, p2 * chance);
        }
        g.UnmakeMove();
        node_utility += action_val[i] * chance;
    }

    vector<double> regrets(NUM_ACTIONS);
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if(possible_actions[i]) regrets[i] = action_val[i] - node_utility;
    }
    (*currentNode).UpdateRegret(regrets);
    // cout << "\nEV: " << node_utility;
    return node_utility;
}
