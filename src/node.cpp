#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <stdint.h>
#include <vector>
#include <bitset>

#include "node.h"
#include "defines.h"
using namespace std;

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
        '0' - Check
        '1' - min-25% 
        '2' - 25%->40%
        '3' - 40%->60%
        '4' - 60%->85%
        '5' - 85%->110%
        '6' - 110%->150%
        '7' - 150%->200%
        '8' - 200%->300%
        '9' - 300%+
        'a' - allin
        'b' - call

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

Node::Node(vector<bool> actions) {
    strategy_sum = vector<double>(NUM_ACTIONS, 0);
    regret_sum = vector<double>(NUM_ACTIONS, 0);
    strategy = vector<double>(NUM_ACTIONS, 0);

    int n = 0; for (auto a : actions) if (a) n++;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (actions[i]) strategy[i] = 1.0/(double)n;
    }
    possible_actions = actions;
}

Node::Node() {
    strategy_sum = vector<double>(NUM_ACTIONS, 0);
    regret_sum = vector<double>(NUM_ACTIONS, 0);
    strategy = vector<double>(NUM_ACTIONS, 1.0/(double)NUM_ACTIONS);
}

char Node::GetBetAction(int pot, int bet_size) {
    if (bet_size == 0) {
        //Check call
        return '0';
    } 
    double ratio = (double)bet_size / pot;
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
char Node::GetRaiseAction(int bet_size, int last_bet_size) {
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

string Node::GetHash(Game &g) {
    uint32_t hash = 0;
    int player = g.player;

    hash |= g.hands[player].getValue() | (g.board[0].getValue() << 4) | (g.board[1].getValue() << 8);
    if (g.stage == FLOP) hash |= FLOP_MASK | TURN_MASK;
    if (g.stage == TURN) hash |= TURN_MASK;

    double spr = g.effective_stack[player];

    hash |= getSPR(spr) << 14; 
    // cout << "SPR: " <<  spr << '\n';
    if(g.stage == FLOP) {
        if (g.hands[player].getSuit() == g.board[0].getSuit()) hash |= (1 << 12);
    }
    if(g.stage == TURN) {
        //bricked flush draw
        if ((g.hands[player].getSuit() == g.board[0].getSuit()) && (g.board[0].getSuit() != g.board[1].getSuit())) hash |= (1 << 12);
        //flush completes
        if ((g.hands[player].getSuit() == g.board[0].getSuit()) && (g.board[0].getSuit() == g.board[1].getSuit())) hash |= (2 << 12);
        // flush on board
        if ((g.hands[player].getSuit() != g.board[0].getSuit()) && (g.board[0].getSuit() == g.board[1].getSuit())) hash |= (3 << 12);
    }


    bitset<18> hash_bits(hash);
    // cout << hash_bits;
    string hash_string = to_string(hash) + "|" + g.abstractHistory;
    // cout << "\n Value: " <<  hash_string;

    return hash_string;
}

void Node::ReverseHash(const string& full_hash) {
    size_t sep = full_hash.find("|");
    if (sep == string::npos) {
        cerr << "Invalid hash format.\n";
        return;
    }

    uint32_t hash = stoi(full_hash.substr(0, sep));
    string abstractHistory = full_hash.substr(sep + 1);

    int handVal = hash & 0xF;            // bits 0-3
    int board0Val = (hash >> 4) & 0xF;   // bits 4-7
    int board1Val = (hash >> 8) & 0xF;   // bits 8-11
    int flushInfo = (hash >> 12) & 0x3;  // bits 12-13
    int sprCat = (hash >> 14) & 0x7;     // bits 14-16

    bool isTurn = hash & TURN_MASK;
    bool isFlop = hash & FLOP_MASK;

    cout << "Parsed Hash Details:\n";
    cout << "Hand Value: " << handVal << '\n';
    cout << "Board[0] Value: " << board0Val << '\n';
    cout << "Board[1] Value: " << board1Val << '\n';

    if (isFlop && !isTurn)
        cout << "Stage: FLOP\n";
    else if (isTurn)
        cout << "Stage: TURN\n";
    else
        cout << "Stage: UNKNOWN\n";

    cout << "Flush Info: ";
    switch (flushInfo) {
        case 0: cout << "No flush\n"; break;
        case 1: cout << "Bricked flush draw\n"; break;
        case 2: cout << "Flush completes\n"; break;
        case 3: cout << "Flush on board\n"; break;
    }

    cout << "SPR Category (encoded): " << sprCat << '\n';
    cout << "Abstract History: " << abstractHistory << '\n';
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
    } else {
        for (int i = 0; i < NUM_ACTIONS; i++) {
            strategy[i] = max(0.0, regret_sum[i]/normalising_sum);
        }
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