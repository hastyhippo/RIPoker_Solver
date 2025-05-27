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

uint64_t positionToState() {
    return 0;
}

CFRSolver::CFRSolver(int num_players) {
    
}

vector<double> normaliseStrategy() {
    return {};
}

Node CFRSolver::getNode(string history) {
    return Node();
}


bool actionClosed(string history) {
    // If on the Turn, XX or BC
    if(history.length() > 2) {
        if (history[-1] == 'X' && history[-2] == 'X') return true;
    } 
    if (history[-1] == 'C') return true;
    return false;
}

bool isTerminal(string history) {
    // One player fold, all in and call
    if (history[-1] == 'F') return true;
    if (history.length() > 2 && history[-2] == 'A' && history[-1] == 'C') return true;
    if (history[0] == '2') {
        return actionClosed(history);
    }

    return false;
    // if a player folded
}

double GetUtility(bool player, string history, int winner, int pot) {
    int len = history.length();
    for (int i = 0; i < len; i++) {
        // history[i];
    }

    // fold -> pnl is -pot/2
    if (history[-1] == 'f') return (pot * player)/2;

    return (player ? 1 : -1) * (winner * pot) / 2;
}

vector<bool> possibleActions(string history) {
    vector<bool>actions(NUM_ACTIONS, false);
    
    // If last action is all in, you can either fold or call
    if (history[-1] == 'A') {
        actions[11] = true;
        actions[12] = true;
    }

    // If last action is passive (X or C), you can X or B
    if (history[-1] == 'X' || history[-1] == 'C' || history[-1] == '|') {
        for (int i = 0; i <= 10; i++) actions[i] = true;
    } else { // If last action doesn't reopen action, you can F, C, R
        for (int i = 11; i <= 18; i++) actions[i] = true;
    }

    return actions;
}
 
void CFRSolver::TrainCFR(CFRSolver cfr) {
    // int strength0 = Card::getStrength(board[0], board[1], hands[0]);
    // int strength1 = Card::getStrength(board[0], board[1], hands[1]);
    
    // int winner = (strength0 == strength1) ? (strength0 < strength1 ? 1 : -1) : 0; 
    
    // double ev = CFR(cfr, "0", 1, 1, 0, winner, 2);

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

// Only working for 2p 
double CFR(CFRSolver cfr, string history, double p1, double p2, bool player, int winner, int pot) {

    if (isTerminal(history)) {
        return GetUtility(player, history, winner, pot);
    }
    // next card if applicable

    // has the position properly
    Node currentNode = cfr.getNode("ASD");
    vector<double> strategy = currentNode.strategy;

    vector<double> action_val(12);
    double node_utility = 0;

    vector<bool> possible_actions = possibleActions(history);
    for (int i = 0; i < NUM_ACTIONS; i++) {
        // check if the move is valid
        if(!possible_actions[i]) continue;
        
        // perform action

        // updat ehistory properly
        if (player) {
            action_val[i] = -CFR(cfr, history, p1 * strategy[i], p2, !player, winner, pot);
        } else {
            action_val[i] = -CFR(cfr, history, p1, p2 * strategy[i], !player, winner, pot);
        }
        node_utility += action_val[i] * strategy[i];
    }

    for (int i = 0; i < NUM_ACTIONS; i++) {
        currentNode.UpdateStrategy();
    }
    return node_utility;
}
