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
    CFRSolver cfr(num_players);    

    while (true) {
        Game g(starting_stack,num_players);

        cout << "How many games: \n";
        int iterations;
        cin >> iterations;

        for (int i = 0; i < iterations; i++) {
            if (g.GameEnd()) {
                g = Game(starting_stack, num_players);
            }

            g.StartGame(0);
        }
    }


    return 0;

    
}
