#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include "cfr_solver.h"
#include "node.h"
#include "game.h"

#define NUM_ACTIONS 18

CFRSolver::CFRSolver(int num_players) {

}

void CFRSolver::TrainCFR() {
    Game g(200);
    g.InitialiseGame(0);
    double ev = CFR(g, 1,1);
    cout << "EV: " << ev<< "\n";
}

/*
    Actions:
    - X Check (0)
    - 1 B20 (1)
    - 2 B33 (2)
    - 3 B50 (3)
    - 4 B66 (4)
    - 5 B80 (5)
    - 6 B100 (6)
    - 7 B150 (7)
    - 8 B200 (8)
    - 9 B300 (9)
    - A ALLIN (10)

    Against a Bet:
    - F Fold (11)
    - C Call (12)
    - 1 R 2.2x (13)
    - 2 R 2.6x (14)
    - 3 R 3x (15)
    - 4 R 3.6x (16)
    - 5 R 4.5x (17)
    - A R ALLIN (18)
*/ 

Node& CFRSolver::GetNode(string hash) {
    if(positionMap.count(hash) == 0) {
        Node newNode = Node();
        positionMap[hash] = newNode;
    } else {
        return positionMap[hash];
    }
}

// Only working for 2p 
double CFRSolver::CFR(Game &g, double p1, double p2) {
    g.PrintGame(true);

    if (g.terminal) {
        return g.GetUtility();
    }
    // next card if applicable

    // has the position properly
    Node& currentNode = GetNode(Node::GetHash(g));
    currentNode.UpdateStrategy();
    vector<double> strategy = currentNode.strategy;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        cout << strategy[i] << " | ";
    }
    cout << '\n';

    vector<double> action_val(NUM_ACTIONS);
    double node_utility = 0;

    vector<bool> possible_actions = g.GetActions(false);
    for (int i = 0; i < NUM_ACTIONS; i++) {
        // check if the move is valid
        if(!possible_actions[i]) continue;
        
        // perform action
        g.MakeMove(i);
        if (g.player == 0) {
            action_val[i] = -CFR(g, p1 * strategy[i], p2);
        } else {
            action_val[i] = -CFR(g, p1, p2 * strategy[i]);
        }
        g.UnmakeMove();
        node_utility += action_val[i] * strategy[i];
    }

    vector<double> regrets(NUM_ACTIONS);
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if(possible_actions[i]) regrets[i] = action_val[i] - node_utility;
    }
    currentNode.UpdateRegret(regrets);

    return node_utility;
}
