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
        
        int first_to_act;
        int player; // 0 or 1
        int stage;
        bool terminal;

        vector<int> effective_stack;
        vector<int> chips;
        vector<vector<int>> bet_states;

        int pot;
        Deck deck;
        vector<Card> hands;
        vector<Card> board;
        vector<string> moveHistory;

        string history;
        string abstractHistory;

        //Built
        Game(int starting_stack);
        void InitialiseGame(int OOP);
        string PrintGame(bool print);
        vector<bool> GetActions(bool print);
        void MakeMove(int move_type);
        void UnmakeMove();
        // Gets utility for the player it is to act
        int GetUtility();
};