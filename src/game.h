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

extern vector<double> bet_sizings;
extern vector<double> raise_sizings;
extern vector<string> move_names;

// One moveHistory entry, exactly what UnmakeMove needs. Chips covers
// check/call/bet/raise/all-in; StreetClose marks the pot bump + stage++.
struct Move {
    enum Kind : uint8_t { Chips, Fold, StreetClose };
    Kind kind;
    int8_t player; // acting player (meaningless for StreetClose)
    int amount;    // chips added to bet_states this move (0 for check/fold)
};

class Game {
    private:

    public:

        int first_to_act;
        int player; // 0 or 1
        int stage;
        bool terminal;

        vector<int> effective_stack;
        vector<vector<int>> bet_states;

        int pot;
        Deck deck;
        vector<Card> hands;
        vector<Card> board;
        vector<Move> moveHistory;

        string abstractHistory;

        //Built
        Game(int stack0, int stack1);
        void InitialiseGame(int OOP);
        string PrintGame(bool print);
        vector<bool> GetActions(bool print);
        void MakeMove(int move_type);
        void UnmakeMove();
        // Gets utility for the player it is to act
        int GetUtility();

        // Terminal payoff from a fixed player-0 perspective (see .cpp).
        int GetUtilityForPlayer0();

        // Money state in the infoset key: KeyPot = total chips in play,
        // KeyBet = chips the acting player owes to call (0 if not facing a bet).
        int KeyPot();
        int KeyBet();

        // Replays an exact-mode history on a fresh Game to recover the real
        // pot/bet_states; sets ok=false if a token can't be decoded exactly.
        static Game ReplayExactHistory(int stack0, int stack1, const string &history, bool &ok);
};