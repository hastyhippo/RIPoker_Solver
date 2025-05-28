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

    public:
        Game(int starting_stack);
        int first_to_act;
        int player; // 0 or 1
        int stage;
        vector<int> effective_stack;
        vector<int> chips;
        vector<vector<int>> bet_states;
        int pot;
        Deck deck;
        vector<Card> hands;
        vector<Card> board;

        stack<string> moveHistory;

        string history;
        bool terminal;

        //Built
        void InitialiseGame(int OOP);
        void PrintGame();
        
        // TODO
        vector<bool> GetActions();
        void MakeMove(int move_type);
        void UnmakeMove();
};