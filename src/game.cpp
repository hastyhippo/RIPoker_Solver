#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

#include "game.h"

using namespace std;

Game::Game(int num_players) {
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
}

void Game::StartGame() {
    // At each stage, call the player to act

}

void Game::PrintGame() {
    cout << "Number of players: " << num_players << " \n";
    cout << "Board: " << board[0].getName() << " | " << board[1].getName() << '\n';
    for (int i = 0; i < num_players; i++) {
        cout << "Player: " << i << " | Card: " << hands[i].getName() << "\n";
    }
    cout << '\n';
}