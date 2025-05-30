#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <map>
#include <unordered_map>
#include <cassert>

#include "node.h"
#include "game.h"

#define ANTE 1
#define NUM_ACTIONS 18
#define NUM_PLAYERS 2
using namespace std;

#define FOLD 11
#define CALL 12
#define CHECK 0
#define ALLIN_BET 10
#define ALLIN_RAISE 18

vector<double> bet_sizings = {0.2, 0.33, 0.5, 0.66, 0.8, 1, 1.5, 2, 3};
vector<double> raise_sizings = {2.2, 2.6, 3, 3.6, 4.5};
vector<string> move_names = {"Check", "B20", "B33", "B50", "B66", "B80", "B100", "B150", "B200", "B300", "BALL",
"FOLD", "CALL", "R2.2", "R2.6", "R3", "R3.6", "R4.5", "RALL"};

bool isNumber(const string& s) {
    return !s.empty() && all_of(s.begin(), s.end(), ::isdigit);
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
string printArray(vector<int>v) {
    string str = "";
    str += "| ";
    for (int a : v) {
        str += to_string(a) + " | ";
    }
    str += "\n";
    return str;
}

string Game::PrintGame(bool print) {
    string str = "";
    str += "\n";
    str += "Stage: " + to_string(stage) + '\n';
    str += "Number of players: " +  to_string(NUM_PLAYERS) +  " | Action on: " + to_string(player) + " | OOP: " + to_string(first_to_act) + " \n";
    str +=  "Board: " + board[0].getName() + " | " + board[1].getName() + '\n'; 
    str += "Pot: " + to_string(pot) + '\n';
    for (int i = 0; i < NUM_PLAYERS; i++) {
        str += "Player: " + to_string(i) + " | Stack: " + to_string(effective_stack[i]) + " | Card: " + hands[i].getName() + "\n";
    }

    str += "Bet states\n";
    for (int i = 0; i < 3; i++) {
        str += "Stage:  " + to_string(i) + ": ";
        str += printArray(bet_states[i]);
    }
    str += "Terminal: " + to_string(terminal) + "\n\n";
    if(print) cout << str;
    return str;
}

Game::Game(int startingStack) {
    this->deck = Deck();
    // Create the cards that the players receive
    vector<Card> hands(NUM_PLAYERS);
    vector<Card> board(2);
    this->hands = hands;
    this->board = board;

    vector<int> effective_stack(NUM_PLAYERS);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        effective_stack[i] = startingStack;
    }
    this->effective_stack = effective_stack;
    this->pot = 0;
}

void Game::InitialiseGame(int OOP) {
    deck.Shuffle();
    deck.PrintDeck();
    for (int i = 0; i < NUM_PLAYERS; i++) {
        hands[i] = deck.Draw();
    }
    board[0] = deck.Draw(); 
    board[1] = deck.Draw();

    bet_states = {{0, 0}, {0,0}, {0,0}};
    
    cout << "Players ante " << ANTE << " chip/s each" << '\n';
    for (int i = 0; i < NUM_PLAYERS; i++) {
        effective_stack[i] -= ANTE;
        pot += ANTE;
    }

    effective_stack[0] = min(effective_stack[0], effective_stack[1]);
    effective_stack[1] = min(effective_stack[0], effective_stack[1]);

    // TODO: Make this alternate
    first_to_act = OOP;
    player = first_to_act;
    stage = 0;
    terminal = false;
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

vector<bool> Game::GetActions(bool print) {
    vector<bool> actions(NUM_ACTIONS + 1,false);
    if(terminal) return actions;

    if(bet_states[stage][0] == 0 && bet_states[stage][1] == 0) {
        // X is available
        actions[CHECK] = true;
        unordered_map<int, int> values;

        for (int i = 1; i <= 9; i++) {
            int bet_size = pot * bet_sizings[i-1];
            if (values.count(bet_size) == 0 && bet_size < effective_stack[player] && bet_size > 0) {
                values[bet_size]++;
                actions[i] = true;
            }
        }
        // allin always available
        if(effective_stack[1 - player] > 0 && effective_stack[player] > 0) {
            actions[ALLIN_BET] = true;
        }    
    } else {
        // Facing a bet/raise - Fold/Call always available
        actions[FOLD] = true;
        actions[CALL] = true;

        unordered_map<int, int> values;

        for (int i = 13; i <= 17; i++) {
            int raise_size = bet_states[stage][1 - player] * raise_sizings[i-13];
            if (values.count(raise_size) == 0 && raise_size < effective_stack[player] && raise_size > 0) {
                values[raise_size]++;
                actions[i] = true;
            }
        }
        if(effective_stack[1 - player] > 0 && effective_stack[player] > 0) {
            actions[ALLIN_RAISE] = true;
        }
    }

    if(print) {
        for (int i = 0; i <= NUM_ACTIONS; i++) {
            if (actions[i]) {
                cout << i << " | " << move_names[i];
                if (i >= 1 && i <= 9) {
                    cout << " : " << (int)(pot * bet_sizings[i-1]) << '\n';
                } else if (i >= 13 && i <= 17) {
                    cout << " : " << (int)(bet_states[stage][1 - player] * raise_sizings[i-13])  << '\n';
                } else {
                    cout << '\n';
                }
            } 
        }
        cout << '\n';
    }

    return actions;
}

void Game::MakeMove(int move_type) {
    // int effective_stack = min(effective_stack[0], effective_stack[1]);
    bool update_stage = false;
    assert(terminal == false);

    if (move_type == CHECK) {
        // if last move is also a check, skip to next stage
        if (!moveHistory.empty() && 
        moveHistory.top().length() == 3 && moveHistory.top().substr(1,2) == "B0") {
            stage++;
            update_stage = true;
        }
        moveHistory.push(to_string(player) + "B0");

        // if it checks through
    } else if (move_type == FOLD) {
        moveHistory.push(to_string(player) + "F");
        terminal = true;
    }
    else if (move_type == CALL) {
        int call_size = bet_states[stage][1 - player] - bet_states[stage][player];
        assert(call_size > 0);

        moveHistory.push(to_string(player) + "B" + to_string(call_size));
        effective_stack[player] -= (bet_states[stage][1 - player] - bet_states[stage][player]);
        bet_states[stage][player] = bet_states[stage][1 - player];

        // End state - increase pot size
        pot += bet_states[stage][player] * 2;

        // Calling will always end action for 2p
        stage++;
        update_stage = true;

        // If either play is allin, then the node is terminal
        if (effective_stack[0] == 0 && effective_stack[1] == 0) {
            terminal = true;
        }
    } else if (move_type == ALLIN_BET || move_type == ALLIN_RAISE) {
        // equivalent of betting whole stack
        moveHistory.push(to_string(player) + "B" + to_string(effective_stack[player]));
        assert(effective_stack[player] > 0);
        bet_states[stage][player] += effective_stack[player];
        effective_stack[player] = 0;

    } else if (move_type >= 1 && move_type <= 9 ) {
        //Bet
        int bet_size = bet_sizings[move_type - 1] * pot;
        assert(bet_size > 0);

        effective_stack[player] -= bet_size;
        bet_states[stage][player] = bet_size;

        moveHistory.push(to_string(player) + "B" + to_string(bet_size));
    } else if (move_type >= 13 && move_type <= 17) {
        //Raise
        int raise_size = bet_states[stage][1 - player] * raise_sizings[move_type - 13];
        assert(raise_size > 0);

        int extra_chips = raise_size - bet_states[stage][player];
        effective_stack[player] -= extra_chips;
        bet_states[stage][player] = raise_size;

        moveHistory.push(to_string(player) + "B" + to_string(extra_chips));
    }

    if (update_stage) {
        // Indicate new chance node and next stage
        moveHistory.push("|");
        if (stage == 3) {
            terminal = true;
            return;
        }
        player = first_to_act;
    } else {
        // Swap players
        player = 1 - player;
    }
}

void Game::UnmakeMove() {
    assert(!moveHistory.empty());
    terminal = false;
    string last_move = moveHistory.top();

    if (last_move[1] == 'F') {
        moveHistory.pop();
        player = last_move[0] - '0';
        return;
    } else if (last_move == "|") {
        stage--;
        assert(bet_states[stage][0] == bet_states[stage][1]);

        // Update pot amount and get the next move before that
        pot -= bet_states[stage][0] + bet_states[stage][1];
        moveHistory.pop();
        last_move = moveHistory.top();
    }
    assert(!moveHistory.empty());

    moveHistory.pop();
    player = last_move[0] - '0';
    int bet_size = stoi(last_move.substr(2,last_move.length()-2));
    bet_states[stage][player] -= bet_size;
    effective_stack[player] += bet_size;
}




