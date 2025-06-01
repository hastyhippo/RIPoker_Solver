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
#include "defines.h"

vector<double> bet_sizings = {0.33, 0.66, 1, 1.5, 2, 3};
// vector<double> bet_sizings = {0.2, 0.33, 0.5, 0.66, 0.8, 1, 1.5, 2, 3};
vector<double> raise_sizings = {2.2, 2.6, 3, 4};
// vector<string> move_names = {"Check", "B20", "B33", "B50", "B66", "B80", "B100", "B150", "B200", "B300", "BALL",
// "FOLD", "CALL", "R2.2", "R2.6", "R3", "R3.6", "R4.5", "RALL"};
vector<string> move_names = {"Check", "Call", "Fold", "Allin", "B33", "B66", "B100", "B150", "B200", "B300", "R2.2", "R2.6", "R3", "R4"};

bool isNumber(const string& s) {
    return !s.empty() && all_of(s.begin(), s.end(), ::isdigit);
}


Game::Game(int startingStack) {
    this->deck = Deck();
    // Create the cards that the players receive
    this->hands = vector<Card>(NUM_PLAYERS);
    this->board = vector<Card>(2);
    this->effective_stack = vector<int>(NUM_PLAYERS, startingStack);
    this->chips = vector<int>(NUM_PLAYERS, startingStack);
    this->pot = 0;
    this->abstractHistory = "";
    this->player = 0;
    this->stage = 0;
}

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
    str += "\nMoveHistory: ";
    for (auto a : moveHistory) {
        str += a + " ";
    }
    str += "\nAbstractedHistory:" + abstractHistory + "\n";

    for (int i = 0; i < NUM_PLAYERS; i++) {
        str += "Player: " + to_string(i) + " | Effective Stack: " + to_string(effective_stack[i]) + " | Stack: " +  to_string(effective_stack[i])  + " | Card: " + hands[i].getName() + "\n";
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

void Game::InitialiseGame(int OOP) {
    deck.Shuffle();
    for (int i = 0; i < NUM_PLAYERS; i++) {
        hands[i] = deck.Draw();
    }

    assert(board.size() == 2);

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

    first_to_act = OOP;
    player = first_to_act;
    stage = 0;
    terminal = false;
    moveHistory = {};
    abstractHistory = "";
}

int Game::GetUtility() {
    assert(terminal == true);
    if (stage != 3 && (bet_states[stage][0] != bet_states[stage][1])) {
        int winner = bet_states[stage][0] > bet_states[stage][1] ? 0 : 1;
        int profit = (pot/2) + bet_states[stage][1-winner];
        return (winner == player ? 1 : -1) * profit;
    } else {
        vector<int>  strengths = {
            Card::getStrength(hands[0], board[0], board[1]) , 
            Card::getStrength(hands[1], board[0], board[1])
        };

        if (strengths[0] == strengths[1]) {
            return 0;
        } else {
            return (strengths[player] < strengths[1-player] ? 1 : -1) * (pot/2);
        }
    }
}

vector<bool> Game::GetActions(bool print) {
    vector<bool> actions(NUM_ACTIONS,false);
    if(terminal) return actions;

    if(bet_states[stage][0] == 0 && bet_states[stage][1] == 0) {
        // X is available
        actions[CHECK] = true;
        unordered_map<int, int> values;

        for (int dx = 0; dx <  NUM_BETS; dx++) {
            int i = dx + MISC_ACTIONS;
            int bet_size = pot * bet_sizings[dx];
            if (values.count(bet_size) == 0 && bet_size < effective_stack[player] && bet_size > 0) {
                values[bet_size]++;
                actions[i] = true;
            }
        }
        // allin always available
        if(effective_stack[1 - player] > 0 && effective_stack[player] > 0) {
            actions[ALLIN] = true;
        }    
    } else {
        // Facing a bet/raise - Fold/Call always available
        actions[FOLD] = true;
        actions[CALL] = true;

        unordered_map<int, int> values;

        for (int dx = 0; dx < NUM_RAISES; dx++) {
            int i = MISC_ACTIONS + NUM_BETS + dx;
            int raise_size = bet_states[stage][1 - player] * raise_sizings[dx];
            if (values.count(raise_size) == 0 && raise_size < effective_stack[player] && raise_size > 0) {
                values[raise_size]++;
                actions[i] = true;
            }
        }
        if(effective_stack[1 - player] > 0 && effective_stack[player] > 0) {
            actions[ALLIN] = true;
        }
    }

    if(print) {
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (actions[i]) {
                cout << i << " | " << move_names[i];
                if (i >= MISC_ACTIONS && i < MISC_ACTIONS + NUM_BETS ) {
                    cout << " : " << (int)(pot * bet_sizings[i-MISC_ACTIONS]) << '\n';
                } else if (i >= MISC_ACTIONS + NUM_BETS && i < MISC_ACTIONS + NUM_BETS + NUM_RAISES) {
                    cout << " : " << (int)(bet_states[stage][1 - player] * raise_sizings[i-(MISC_ACTIONS + NUM_BETS)])  << '\n';
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
        moveHistory.back().length() == 3 && moveHistory.back().substr(1,2) == "B0") {
            stage++;
            update_stage = true;
        }
        moveHistory.push_back(to_string(player) + "B0");
        abstractHistory += Node::GetBetAction(pot, 0);

    } else if (move_type == FOLD) {
        moveHistory.push_back(to_string(player) + "F");
        terminal = true;
        abstractHistory += 'f';
    }
    else if (move_type == CALL) {
        int call_size = bet_states[stage][1 - player] - bet_states[stage][player];
        assert(call_size > 0);

        moveHistory.push_back(to_string(player) + "B" + to_string(call_size));
        abstractHistory += 'c';
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
    } else if (move_type == ALLIN) {
        // equivalent of betting whole stack
        moveHistory.push_back(to_string(player) + "B" + to_string(effective_stack[player]));
        abstractHistory += 'a';
        assert(effective_stack[player] > 0);
        bet_states[stage][player] += effective_stack[player];
        effective_stack[player] = 0;

    } else if (move_type >= MISC_ACTIONS && move_type < MISC_ACTIONS + NUM_BETS) {
        //Bet
        int bet_size = bet_sizings[move_type - MISC_ACTIONS] * pot;
        assert(bet_size > 0);

        effective_stack[player] -= bet_size;
        bet_states[stage][player] = bet_size;

        moveHistory.push_back(to_string(player) + "B" + to_string(bet_size));
        abstractHistory += Node::GetBetAction(pot, bet_size);
    } else if (move_type >= MISC_ACTIONS + NUM_BETS && move_type <= MISC_ACTIONS + NUM_BETS + NUM_RAISES) {
        //Raise
        int raise_size = bet_states[stage][1 - player] * raise_sizings[move_type - (MISC_ACTIONS + NUM_BETS)];
        assert(raise_size > 0);

        int extra_chips = raise_size - bet_states[stage][player];
        effective_stack[player] -= extra_chips;
        bet_states[stage][player] = raise_size;

        moveHistory.push_back(to_string(player) + "B" + to_string(extra_chips));
        abstractHistory += Node::GetRaiseAction(raise_size, bet_states[stage][1-player]);
    }


    if (update_stage) {
        // Indicate new chance node and next stage
        moveHistory.push_back("|");
        abstractHistory += ",";
        player = first_to_act;

        if (stage == 3 || terminal) {
            terminal = true;
            return;
        }

    } else {
        if(terminal) {
            return;
        }
        // Swap players
        player = 1 - player;
    }
}

void Game::UnmakeMove() {
    assert(!moveHistory.empty());
    terminal = false;
    string last_move = moveHistory.back();

    if (last_move[1] == 'F') {
        moveHistory.pop_back();
        abstractHistory.pop_back();
        player = last_move[0] - '0';
        return;
    } else if (last_move == "|") {
        stage--;
        assert(bet_states[stage][0] == bet_states[stage][1]);

        // Update pot amount and get the next move before that
        pot -= bet_states[stage][0] + bet_states[stage][1];
        moveHistory.pop_back();
        abstractHistory.pop_back();
        last_move = moveHistory.back();
    }
    assert(!moveHistory.empty());

    moveHistory.pop_back();
    abstractHistory.pop_back();
    player = last_move[0] - '0';
    int bet_size = stoi(last_move.substr(2,last_move.length()-2));
    bet_states[stage][player] -= bet_size;
    effective_stack[player] += bet_size;
}