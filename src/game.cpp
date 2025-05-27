#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

#include "node.h"
#include "game.h"

using namespace std;

#define ANTE 1
#define NUM_ACTIONS 18


bool Game::GameEnd() {
    int cnt = 0;
    for (int i = 0; i < num_players; i++) {
        if (chips[i] > 0) cnt++;
    }
    return cnt == 1;
}

Game::Game(int startingStack, int num_players) {
    this->num_players = num_players;
    this->deck = Deck();
    // Create the cards that the players receive
    vector<Card> hands(num_players);
    vector<Card> board(2);
    this->hands = hands;
    this->board = board;

    vector<int> chips(num_players);
    for (int i = 0; i < num_players; i++) {
        chips[i] = startingStack;
    }
    this->chips = chips;
    this->pot = 0;
}

void Game::InitialiseGame() {
    deck.Shuffle();
    deck.PrintDeck();
    for (int i = 0; i < num_players; i++) {
        hands[i] = deck.Draw();
    }
    board[0] = deck.Draw(); 
    board[1] = deck.Draw();

    cout << "Players ante " << ANTE << " chip/s each" << '\n';
    for (int i = 0; i < num_players; i++) {
        chips[i] -= ANTE;
        pot += ANTE;
    }
}

void Game::MakeMove() {

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


/*
TO_DO: implement effective stack
*/
// stage: 0 - preflop   1 - flop  2 - turn
void Game::GameLoop(int stage, int num_players, int curr_player, int last_bet, int last_bet_size, vector<int> &bet_states, vector<bool> &in_hand, string &game_history) {
    // Skip all players who have folded
    while(!in_hand[curr_player]) {
        curr_player = (curr_player + 1) % num_players;
    }
    
    bool no_bet = bet_states[last_bet] == 0;
    // If there has been a bet, then continue if action has not reopened
    if (curr_player == last_bet && !no_bet) {
        return;
    }

    cout << "\n\nPlayer: " << curr_player + 1 << " | Hole Card: " << hands[curr_player].getName() << '\n';
    if (last_bet_size != 0)     cout << "Player " << last_bet + 1 << " raises to " << last_bet_size << '\n';
    cout << "Stacks: ";
    printArray(chips);
    cout << "Bet states: ";
    printArray(bet_states);
    cout << "Pot: " << pot << "\n\n";

    cout << "Your options: " << '\n';

    if (no_bet) {
            Node::GetHash(stage, false, (double)chips[curr_player]/pot, game_history, hands[curr_player], board[0], board[1]);
        // No pre-existing bet - option to check or bet
        cout << "Check (X)" << '\n';
        cout << "Bet (B + amount)" << '\n';
    } else {
        // Pre-existing bet - option to fold, call or raise (if you have enough)
            Node::GetHash(stage, true, (double)chips[curr_player]/pot, game_history, hands[curr_player], board[0], board[1]);

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
                game_history += '0';
            } else if (ch[0] == 'B') {
                //verify bet is valid
                string bet_size_str = ch.substr(1);
                if(!isNumber(bet_size_str)) {
                    cout << "Your bet sizing: " << bet_size_str << " is not an integer amount\n";
                    continue;
                }
                int bet_size = stoi(bet_size_str);

                if (bet_size > chips[curr_player]) {
                    cout << "Bet is more than players holdings\n";
                    continue;
                } 
                cout << "Bet to " << bet_size << '\n';
                last_bet_size = bet_size;

                //Update states
                valid_action = true;
                last_bet = curr_player;
                bet_states[curr_player] = bet_size;

                if (bet_size == chips[curr_player]) {
                    game_history += 'a';
                } else {
                    game_history += stage == PREFLOP ? Node::GetAction(bet_size, 1 * num_players) : Node::GetAction(pot, bet_size, BET);
                }
            } else {
                cout << "Your input: `" << ch << "` is invalid\n\n";
            }
        } else {
            if (ch == "F") {
                valid_action = true;
                in_hand[curr_player] = false;
                game_history += 'f';
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
                    game_history += 'a';
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

                game_history += Node::GetAction(raise_size, last_bet_size);
                last_bet_size = raise_size;
                valid_action = true;
                last_bet = curr_player;
                bet_states[curr_player] = raise_size;

            } else if (ch == "C") {
                valid_action = true;
                bet_states[curr_player] = last_bet_size;
                game_history += '0';

            } else {
                cout << "Your input: `" << ch << "` is invalid\n\n";
            }
        }
    }
    no_bet = bet_states[last_bet] == 0;


    if (no_bet && curr_player == last_bet ) {
        return;
    }
    GameLoop(stage, num_players, (curr_player+1) % num_players, last_bet, last_bet_size, bet_states, in_hand, game_history);

}

void Game::StartGame(int player_to_act) {


    int last_player = (player_to_act + num_players - 1) % num_players;
    vector<int> bet_states(num_players);
    vector<bool> in_hand(num_players, true);

    string history = "";
    for (int i = 0; i < 3; i++) {

        // make sure last person to act is actually in the hand
        while (!in_hand[last_player]) last_player = (last_player - 1 + num_players) % num_players;
        GameLoop(i , num_players, player_to_act, last_player, 0, bet_states, in_hand, history);
        for (int j = 0; j < num_players; j++) {
            chips[j] -= bet_states[j];
            pot += bet_states[j];
            bet_states[j] = 0;
        }

        int player_count = 0;
        for (int j = 0; j < num_players; j++) {
            player_count += in_hand[j];
        }

        // Everyone else Folds
        if (player_count == 1) {
            int winner = max_element(in_hand.begin(), in_hand.end()) - in_hand.begin();
            cout << "Player: " << winner << " wins a pot of " << pot << "\n";
            chips[winner] += pot;
            pot = 0;
            break;
        }

        if (i == PREFLOP) {
            cout << "\n\nFlop: " << board[0].getName() << "\n";
        } else if (i == FLOP) {
            cout << "\n\nTurn: " << board[1].getName() << "\n";
        } else {
            // If we reach the turn and the betting completes
            cout << "\n\nPot: " << pot << " | Showdown: \n";
            vector<int> handStrengths(num_players);
            vector<int> pot_winners;

            for (int j = 0; j < num_players; j++) {
                handStrengths[j] = in_hand[j] ? Card::getStrength(hands[j], board[0], board[1]) : INT_MAX;
            }

            int winning_strength = *min_element(handStrengths.begin(), handStrengths.end());

            for (int j = 0; j < num_players; j++) {
                if (handStrengths[j] == winning_strength) pot_winners.push_back(j);
            }
            cout << "Player/s: ";
            for (auto a : pot_winners) cout << a + 1 << ", ";
            cout << " won the pot of " <<  pot << "\n\n\n";
            
            int num_split = pot_winners.size(); int amount_to_split = pot / num_split;
            for (auto a : pot_winners) {
                chips[a] += amount_to_split;
                pot -= amount_to_split;
            }

            // Handle any chop pot situations where the chips can't be broken down (integer rounding)
            for (auto a : pot_winners) {
                if (pot > 0) {
                    chips[a] ++;
                    pot --;
                }
            }


        }
    }

    cout << "FINAL RESULT\n\n";
    printArray(chips);
    int sum = 0;
    for (auto a : chips) sum +=a ;
    cout << "sum: " << sum << "\n\n";
    cout << "History: " << history << "\n";
    
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