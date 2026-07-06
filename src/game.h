#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <cstdint>
class CFRSolver;

#include "deck.h"
#include "cfr_solver.h"
using namespace std;

extern vector<double> bet_sizings;
extern vector<double> raise_sizings;
extern vector<string> move_names;

// One step of a replayed position: a decision point (legal moves, resolved
// chip sizes, which was taken) or a board-card reveal between streets. Each
// step stores the history that reaches it so the frontend can navigate the
// tree by clicking (see gotoHistory / history).
struct TrailStep {
    bool isReveal;        // true = board reveal, false = decision point
    int revealSlot;       // 0 = flop card, 1 = turn card (reveals only)
    int player;           // acting player at a decision
    int stack = 0;        // acting player's remaining stack at a decision
    int chosen;           // move_type taken; -1 at the live (last) decision
    int pot = 0;          // reveal: committed pot entering the new street
    string history;       // reveal: history just after the card is dealt
    vector<int> legal;    // legal move_type indices at this point
    vector<int> chipSize; // chips per legal entry; -1 when not applicable
    vector<string> gotoHistory; // history reached by playing each legal move
};

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

        // Replays a history into a step-by-step trail (decisions + reveals);
        // the last step is the live decision point (chosen = -1).
        static vector<TrailStep> BuildTrail(int stack0, int stack1, const string &history, bool &ok);
};