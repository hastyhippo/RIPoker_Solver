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

        // Abstraction granularity config - read by MakeMove for bet/raise
        // history encoding (exact chip amounts vs percentage-of-pot buckets).
        // See node.cpp's Node::GetBetAction/GetRaiseAction.
        bool useBetSizeBuckets;

        //Built
        Game(int stack0, int stack1, bool useBetSizeBuckets);
        void InitialiseGame(int OOP);
        string PrintGame(bool print);
        vector<bool> GetActions(bool print);
        void MakeMove(int move_type);
        void UnmakeMove();
        // Gets utility for the player it is to act
        int GetUtility();

        // Money state used (alongside the cards/history) to identify an
        // infoset - see node.cpp's key format. KeyPot is the total chips in
        // play right now (committed pot + both players' current-street bets);
        // KeyBet is what the acting player must add to call (0 when not facing
        // a bet). Both are read from the live state, so training and any
        // later replay of the same history produce identical values.
        int KeyPot();
        int KeyBet();

        // Replays an exact-mode abstractHistory string (see node.cpp's
        // encoding comment) on a fresh Game, reconstructing the real
        // pot/bet_states right at the point after that history. Lets a
        // caller with no live Game object of its own (export.cpp/server.cpp
        // only have the abstracted Node/positionMap data) recover the actual
        // chip amounts behind each currently-available bet/raise, by reusing
        // MakeMove's real arithmetic instead of duplicating it. Only
        // meaningful for history recorded in exact mode (bracket/brace
        // tokens, not single-letter buckets) - sets ok=false if it hits
        // anything it can't unambiguously decode (bucketed token, or a token
        // whose amount doesn't match any current bet_sizings/raise_sizings
        // choice at the reconstructed pot, e.g. stacks changed since this
        // history was recorded).
        static Game ReplayExactHistory(int stack0, int stack1, const string &history, bool &ok);
};