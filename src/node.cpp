#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <stdint.h>
#include <vector>
#include <bitset>

#include "node.h"

using namespace std;

#define NUM_ACTIONS 2.0
#define BET_1_MAX 0.2
#define BET_2_MAX 0.4
#define BET_3_MAX 0.6
#define BET_4_MAX 0.85
#define BET_5_MAX 1.1
#define BET_6_MAX 1.5
#define BET_7_MAX 2
#define BET_8_MAX 3

#define RAISE_A_MAX 2.4
#define RAISE_B_MAX 2.8 
#define RAISE_C_MAX 3.4 
#define RAISE_D_MAX 4 
#define RAISE_E_MAX 5  

#define SPR_0_MAX 0.5
#define SPR_1_MAX 1
#define SPR_2_MAX 2
#define SPR_3_MAX 4
#define SPR_4_MAX 6
#define SPR_5_MAX 10
#define SPR_6_MAX 20

/*
    string:
    0x111100000000000000 - Card Value
        0->12 Value of card
    0x000011111111000000 - Community Cards
    0x000000000000110000 - Suits Connections
        0 -> All different
        1 -> 1 Board + Hole Card same (Flush Draw for flop, Brick for turn)
        2 -> 2 Board + Hole Card same (Flush complete)
        3 -> Board has same suits (Flush on board, no flush for you)
    0x000000000000001111 - SPR 
    1: Effective SPR (Stack: Pot) 
        0 :  0 -> 0.5
        1 :  0.5 -> 1
        2 :  1 -> 2
        3 :  2 -> 4
        4 :  4 -> 6
        5 :  6 -> 10
        6 :  10 -> 20
        7 :  20 +

    Betting sequence
    String:
        '0' - Check/Call
        '1' - min-20% 
        '2' - 20%->40%
        '3' - 40%->60%
        '4' - 60%->85%
        '5' - 85%->110%
        '6' - 110%->150%
        '7' - 150%->200%
        '8' - 200%->300%
        '9' - 300%+
        'a' - allin

        'A' - raise 2x-2.4x
        'B' - raise 2.4x - 2.8x
        'C' - raise 2.8x - 3.4x
        'D' - raise 3.4x - 4x
        'E' - raise 4x - 5x
        'F' - 5x+
*/



#define NO_FLOP 0x00001111
#define NO_TURN 0x000000001111

#define HOLE__MASK  0x111100000000000000
#define FLOP_MASK   0x000011110000000000
#define TURN_MASK   0x000000001111000000
#define FLUSH_MASK  0x000000000000110000
#define SPR_MASK    0x000000000000001111

Node::Node() {

}

char Node::GetAction(int pot, int bet_size, int flag) {
    if (flag == Game::CHECK) {
        //Check call
        return '0';
    } 
    if (flag == Game::FOLD) {
        return 'f';
    }
    double ratio = (double)bet_size / pot;
    cout << "ratio: " << ratio << "\n";
    if (ratio < BET_1_MAX) {
        return '1';
    } else if (ratio < BET_2_MAX) {
        return '2';
    } else if (ratio < BET_3_MAX) {
        return '3';
    }else if (ratio < BET_4_MAX) {
        return '4';
    } else if (ratio < BET_5_MAX) {
        return '5';
    } else if (ratio < BET_6_MAX) {
        return '6';
    } else if (ratio < BET_7_MAX) {
        return '7';
    } else if (ratio < BET_8_MAX) {
        return '8';
    } else {
        return '9';
    }
}

// RAISES ONLY
char Node::GetAction(int bet_size, int last_bet_size) {
    double ratio = (double)bet_size / last_bet_size;
    if (ratio < RAISE_A_MAX) {
        return 'A';
    } else if (ratio < RAISE_B_MAX) {
        return 'B';
    } else if (ratio < RAISE_C_MAX) {
        return 'C';
    } else if (ratio < RAISE_D_MAX) {
        return 'D';
    } else if (ratio < RAISE_E_MAX) {
        return 'E';
    } else {
        return 'F';
    }
}

int getSPR(double spr) {
    if(spr < SPR_0_MAX) {
        return 0;
    } else if (spr < SPR_1_MAX) {
        return 1;
    } else if (spr < SPR_2_MAX) {
        return 2;
    } else if (spr < SPR_3_MAX) {
        return 3;
    } else if (spr < SPR_4_MAX) {
        return 4;
    } else if (spr < SPR_5_MAX) {
        return 5;
    } else if (spr < SPR_6_MAX) {
        return 6;
    } 
    return 7;
}

string Node::GetHash(int stage, bool can_raise, double spr, string game_history, Card hole, Card board1, Card board2) {
    uint32_t hash = 0;
    hash |= hole.getValue() | (board1.getValue() << 4) | (board2.getValue() << 8);
    if (stage == Game::FLOP) hash |= FLOP_MASK | TURN_MASK;
    if (stage == Game::TURN) hash |= TURN_MASK;

    hash |= getSPR(spr) << 14; 
    cout << "SPR: " <<  spr << '\n';
    if(stage == Game::FLOP) {
        if (hole.getSuit() == board1.getSuit()) hash |= (1 << 12);
    }
    if(stage == Game::TURN) {
        //bricked flush draw
        if ((hole.getSuit() == board1.getSuit()) && (board1.getSuit() != board2.getSuit())) hash |= (1 << 12);
        //flush completes
        if ((hole.getSuit() == board1.getSuit()) && (board1.getSuit() == board2.getSuit())) hash |= (2 << 12);
        // flush on board
        if ((hole.getSuit() != board1.getSuit()) && (board1.getSuit() == board2.getSuit())) hash |= (3 << 12);
    }

    bitset<18> hash_bits(hash);
    cout << hash_bits;
    string hash_string = to_string(hash) + "|" + game_history;
    cout << "\n Value: " <<  hash_string;
    return hash_string;
}

void Node::UpdateRegret(vector<double> new_regret) {
    for (int i = 0; i < NUM_ACTIONS; i++) {
        regret_sum[i] += new_regret[i];
        strategy_sum[i] += strategy[i];
    }
}

void Node::UpdateStrategy() {
    double normalising_sum = 0;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        normalising_sum += max(0.0, regret_sum[i]);
    }

    if (normalising_sum == 0) {
        strategy = vector<double>(NUM_ACTIONS, 1.0/NUM_ACTIONS);
    }

    for (int i = 0; i < NUM_ACTIONS; i++) {
        strategy[i] = max(0.0, regret_sum[i]/normalising_sum);
    }
}

vector<double> Node::GetFinalStrategy() {
    vector<double> final_strategy(NUM_ACTIONS);
    double normalising_sum = 0.0;
    for (auto a : strategy_sum) normalising_sum += a;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        final_strategy[i] = strategy_sum[i] / normalising_sum;
    }
    return final_strategy;
}