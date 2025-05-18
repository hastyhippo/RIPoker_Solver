#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

#include "deck.h"

using namespace std;

class Game {
    private:
        int num_players = 3;
        vector<int> chips;
        int pot;

        vector<Card> hands;
        vector<Card> board;
        void GameLoop(int stage, int num_players, int curr_player, int last_bet, int last_bet_size, vector<int> &bet_states, vector<bool> &in_hand);

    public:
        Game(int starting_stack, int num_players);
        void StartGame(int playerTurn);
        void PrintGame();
        void ResetGame();

};