#include <iostream>
#include <cstdint>
#include <map>
#include "game.h"
using namespace std;

map<int,int> handStrengthMap;
void Initialise() {
    Card::initialiseMap();
}

int main() {
    Initialise();

    int num_players = 2;
    int starting_stack = 200;
    CFRSolver cfr(starting_stack);    

    Game g(starting_stack);
    g.InitialiseGame(0);
    while (!g.terminal) {
        g.GetActions();

        int action;
        cin >> action;
        if (action >= 0 && action <= 18) g.MakeMove(action);
        g.UnmakeMove();
    }
    // while (true) {
    //     Game g(starting_stack);

    //     cout << "How many games: \n";
    //     int iterations;
    //     cin >> iterations;

    //     for (int i = 0; i < iterations; i++) {
    //         if (g.GameEnd()) {
    //             g = Game(starting_stack);
    //         }

    //         g.StartGame(0);
    //     }
    // }


    return 0;

    
}
