#define PREFLOP 0
#define FLOP 1
#define TURN 2

#define NUM_CARDS 24
#define NUM_CARD_VALUES 12

#define BET_1_MAX 0.5
#define BET_2_MAX 0.55
#define BET_3_MAX 0.6
#define BET_4_MAX 0.85
#define BET_5_MAX 1.1
#define BET_6_MAX 1.5
#define BET_7_MAX 2
#define BET_8_MAX 3

#define RAISE_A_MAX 2.4
#define RAISE_B_MAX 2.8
#define RAISE_C_MAX 3.4
#define RAISE_D_MAX 4
#define RAISE_E_MAX 5

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

#define STARTING_STACK 20

/*
    Actions:
    - X Check (0)
    - C Call (1)
    - F Fold (2)
    - A ALLIN (3)
    BETS (indices MISC_ACTIONS .. MISC_ACTIONS+NUM_BETS-1)
    - B50  (half pot)
    - B100 (pot)
    - B200 (2x pot)

    RAISE (indices MISC_ACTIONS+NUM_BETS .. MISC_ACTIONS+NUM_BETS+NUM_RAISES-1)
    - R2.2x
    - R2.6x
    - R3x
    - R4x
*/