#define PREFLOP 0
#define FLOP 1
#define TURN 2

#define NUM_CARDS 24
#define NUM_CARD_VALUES 12

// Not a flat ante - the first player to act (OOP) posts ANTE_FIRST_TO_ACT,
// the other player (IP) posts ANTE_SECOND_TO_ACT, before any action.
#define ANTE_FIRST_TO_ACT 1
#define ANTE_SECOND_TO_ACT 2
#define NUM_ACTIONS 11
#define MISC_ACTIONS 4
#define NUM_BETS 3
#define NUM_RAISES 4

#define NUM_PLAYERS 2

#define CHECK 0
#define CALL 1
#define FOLD 2
#define ALLIN 3

#define STARTING_STACK 10

// Action indices: 0-3 Check/Call/Fold/Allin, then NUM_BETS bets (B33/B75/B125),
// then NUM_RAISES raises (R2.2/R2.6/R3/R4). See move_names in game.cpp.