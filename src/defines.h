#define PREFLOP 0
#define FLOP 1
#define TURN 2

#define NUM_CARDS 24
#define NUM_CARD_VALUES 12

#define BET_1_MAX 0.5
#define BET_2_MAX 0.4
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

#define SPR_0_MAX 0.5
#define SPR_1_MAX 1
#define SPR_2_MAX 2
#define SPR_3_MAX 4
#define SPR_4_MAX 6
#define SPR_5_MAX 10
#define SPR_6_MAX 20

#define ANTE 1
#define NUM_ACTIONS 14
#define MISC_ACTIONS 4
#define NUM_BETS 6 
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
    - F Fold (1)
    - C Call (2)
    - A ALLIN (3)
    BETS
    - B33 (NUM_MISC)
    - B66 (.)
    - B80 (.)
    - B100 (.)
    - B150 (.)
    - B200 (.)
    - B300 (.)

    RAISE
    - F Fold (NUM_MISC+NUM_BETS)
    - C Call (.)
    - 1 R 2.2x (.)
    - 2 R 2.6x (.)
    - 3 R 3x (.)
    - 4 R 4x (.)
*/