#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include "cfr_solver.h"

/*
    64bits:
    0x11000000000000000 - Turn Number
        0 - Preflop
        1 - Flop
        2 - Turn
    0x00111100000000000 - Card Value
        0->12 Value of card
    0x00000011111111000 - Community Cards
    0x00000011110000000 - 1st
    0x00000000001111000 - 2nd
    0x00000000000000111 - Suits Connections
        0 -> All different
        1 -> 1 Board + Hole Card same (Flush Draw for turn 1, Brick for turn 2)
        2 -> 2 Board + Hole Card same (Flush complete)
        3 -> Board has same suits (Flush on board, no flush for you)

    0x00000000000000000 11112222000000000 - Cost to see next card (1) + SPR (2)
    Cost (relative to pot)
        0 : Check is available
        1 : B0 -> B10
        2 : B10 -> B25
        3 : B25 -> B33
        4 : B33 -> B50
        5 : B50 -> B66
        6 : B66 -> B75
        7 : B75 -> B100
        8 : B100 -> B150
        9 : B150 -> B200
        10 : B200 -> B400
        12 : ALLIN
    SPR (Stack: Pot) 
        0 :  0 -> 1
        2 :  1 -> 2
        3 :  2 -> 4
        4 :  4 -> 6
        5 :  6 -> 10
        6 :  10 -> 20
        7 :  20 +
*/

uint64_t positionToState() {
    return 0;
}

CFRSolver::CFRSolver(int num_players) {
    
}

vector<double> normaliseStrategy() {

}

double CFRSolver::CFR(Game G) {
    /*
        If terminal node, return utility
        Iterate through all possible actions
            for each action, record utility after each action
        Calculate ev for the node
        Update Regret Sum and strategy
        Return EV
    */ 
   double avg_utility = 0;
   return avg_utility;
}