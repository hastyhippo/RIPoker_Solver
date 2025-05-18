#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

#include "game.h"

using namespace std;

#define ANTE 1
#define NUM_ACTIONS 2
Game::Game(int startingStack, int num_players) {
    this->num_players = num_players;
    Deck cards = Deck();

    // Create the cards that the players receive
    vector<Card> hands(0);
    vector<Card> board(2);

    for (int i = 0; i < num_players; i++) {
        hands.push_back(cards.Draw());
    }
    board[0] = cards.Draw(); 
    board[1] = cards.Draw();

    this->hands = hands;
    this->board = board;

    vector<int> chips(num_players);
    for (int i = 0; i < num_players; i++) {
        chips[i] = startingStack;
    }
    this->chips = chips;
    this->pot = 0;
}

bool isNumber(const string& s) {
    return !s.empty() && all_of(s.begin(), s.end(), ::isdigit);
}

void printArray(vector<int>v) {
    cout << "| ";
    for (auto a : v) {
        cout << a << " | ";
    }
    cout << "\n";
}
// stage: 0 - preflop   1 - flop  2 - turn
void Game::GameLoop(int stage, int num_players, int curr_player, int last_bet, int last_bet_size, vector<int> &bet_states, vector<bool> &in_hand) {
    while(!in_hand[curr_player]) {
        curr_player = (curr_player + 1) % num_players;
    }
    bool no_bet = bet_states[last_bet] == 0;
    if (curr_player == last_bet && !no_bet) {
        return;
    }

    cout << "\n\nPlayer: " << curr_player + 1 << " | Hole Card: " << hands[curr_player].getName() << '\n';
    if (last_bet_size != 0)     cout << "Player " << last_bet + 1 << " raises to " << last_bet_size << '\n';
    cout << "Stacks: ";
    printArray(chips);
    cout << "Bet states: ";
    printArray(bet_states);
    cout << "Pot: " << pot << "\n";

    cout << "Your options: " << '\n';
    if (no_bet) {
        cout << "Check (X)" << '\n';
        cout << "Bet (B + amount)" << '\n';
    } else {
        cout << "Fold (F)" << '\n';
        cout << "Call " << last_bet_size - bet_states[curr_player] << " (C) " << '\n';
        if (chips[curr_player] > last_bet_size || last_bet_size < bet_states[curr_player * 2]) {
            cout << "Raise (R) " << '\n';
        }
    }
    cout << "Your action: ";

    string ch;

    bool valid_action = false;
    while(!valid_action) {
        cin >> ch;
        if(no_bet) {
            if (ch == "X") {
                valid_action = true;
            } else if (ch[0] == 'B') {
                //verify bet is valid
                string bet_size_str = ch.substr(1);
                if(!isNumber(bet_size_str)) {
                    cout << "Your bet sizing: " << bet_size_str << " is not an integer amount\n";
                    continue;
                }
                int bet_size = stoi(bet_size_str);
                cout << "Bet to " << bet_size << '\n';
                last_bet_size = bet_size;

                valid_action = true;
                last_bet = curr_player;
                bet_states[curr_player] = bet_size;
            } else {
                cout << "Your input: `" << ch << "` is invalid\n\n";
            }
        } else {
            if (ch == "F") {
                valid_action = true;
                in_hand[curr_player] = false;
            } else if (ch[0] == 'R') {
                string raise_size_str = ch.substr(1);
                if(!isNumber(raise_size_str)) {
                    cout << "Your bet sizing: " << raise_size_str << " is not an integer amount\n";
                    continue;
                }
                int raise_size = stoi(raise_size_str);

                if (raise_size == chips[curr_player]) {
                    cout << "player all in\n\n"; 
                    bet_states[curr_player] = raise_size;
                    last_bet_size = raise_size;
                    last_bet = curr_player;
                    valid_action = true;
                    continue;
                } else if (raise_size > chips[curr_player]) {
                    cout << "Your bet sizing: " << raise_size << " is greater than your chip count: " << chips[curr_player] << '\n';
                    continue;
                } else {
                    if (raise_size < bet_states[last_bet] * 2) {
                        cout << "Your raise sizing: " << raise_size << " is not enough" << '\n';
                        continue;
                    }
                }
                cout << "Raise to " << raise_size << '\n';
                last_bet_size = raise_size;
                valid_action = true;
                last_bet = curr_player;
                bet_states[curr_player] = raise_size;

            } else if (ch == "C") {
                valid_action = true;
                bet_states[curr_player] = last_bet_size;
            } else {
                cout << "Your input: `" << ch << "` is invalid\n\n";
            }
        }
    }
    no_bet = bet_states[last_bet] == 0;


    if (no_bet && curr_player == last_bet ) {
        return;
    }
    GameLoop(stage, num_players, (curr_player+1) % num_players, last_bet, last_bet_size, bet_states, in_hand);

}
void Game::StartGame(int player_to_act) {
    cout << "Players ante " << ANTE << " chip/s each" << '\n';
    for (int i = 0; i < num_players; i++) {
        chips[i] -= ANTE;
        pot += ANTE;
    }

    int last_player = (player_to_act + num_players - 1) % num_players;
    vector<int> bet_states(num_players);
    vector<bool> in_hand(num_players, true);
    for (int i = 0; i < 3; i++) {
        GameLoop(i , num_players, player_to_act, last_player, 0, bet_states, in_hand);
        for (int i = 0; i < num_players; i++) {
            chips[i] -= bet_states[i];
            pot += bet_states[i];
            bet_states[i] = 0;
        }

        int player_count = 0;
        for (int i = 0; i < num_players; i++) {
            player_count += in_hand[i];
        }
        if (player_count == 1) {
            cout << "Player: " << *max_element(in_hand.begin(), in_hand.end()) << " wins a pot of " << pot << "\n";
            break;
        }

        if (i == 0) {
            cout << "\n\nFlop: " << board[0].getName() << "\n";
        } else if (i == 1) {
            cout << "\n\nTurn: " << board[1].getName() << "\n";
        } else {
            cout << "\n\nPot: " << pot << " | Showdown: \n";
            for (int i = 0; i < num_players; i++) {
                
            }
        }
    }
}

void Game::PrintGame() {
    cout << "\n";
    cout << "Number of players: " << num_players << " \n";
    cout << "Board: " << board[0].getName() << " | " << board[1].getName() << '\n'; 
    cout << "Pot: " << pot <<  '\n';
    for (int i = 0; i < num_players; i++) {
        cout << "Player: " << i << " | Stack: " << chips[i] <<  " | Card: " << hands[i].getName() << "\n";
    }
}

void Game::ResetGame() {

}