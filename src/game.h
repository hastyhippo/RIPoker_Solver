#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
class CFRSolver;

#include "deck.h"
#include "cfr_solver.h"
using namespace std;
class Game {
    private:
        int num_players = 2;
        void GameLoop(int stage, int num_players, int curr_player, int last_bet, int last_bet_size, vector<int> &bet_states, vector<bool> &in_hand, string &game_history);

    public:
        vector<int> chips;
        vector<int> bet_states;
        int pot;
        Deck deck;
        vector<Card> hands;
        vector<Card> board;

        string history;

        Game(int starting_stack, int num_players);
        void StartGame(int playerTurn);

        void InitialiseGame();
        void PrintGame();
        
        void ResetGame();
        bool GameEnd();
        void MakeMove();

        static constexpr int CHECK = 0;
        static constexpr int BET = 1;
        static constexpr int FOLD = 2;
        static constexpr int RAISE = 3;

        static constexpr int PREFLOP = 0;
        static constexpr int FLOP = 1;
        static constexpr int TURN = 2;

};