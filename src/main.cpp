#include <iostream>
#include <cstdint>
#include <map>
#include "game.h"

#include "cassert"
using namespace std;

map<int,int> handStrengthMap;
void test_make_unmake(Game g);

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
    test_make_unmake(g);

    while (!g.terminal) {
        g.PrintGame(true);

        vector<bool> possible_actions = g.GetActions(true);

        int action;
        cin >> action;
        if (action >= 0 && action <= 18 && !g.terminal && possible_actions[action]) {
            g.MakeMove(action);
        } else if (action == -1 && !g.moveHistory.empty()) {
            g.UnmakeMove();
        } else {
            break;
        }
        test_make_unmake(g);
    }
    cout << "GAMEOVER!\n";
    while(!g.moveHistory.empty()) {
        cout << g.moveHistory.top() << " ";
        g.moveHistory.pop();
    }

    return 0;

    
}

// game.cpp make_move and unmake_move tests
    // By induction, we just have to show all game state properties are
    // identical for one level of make-unmake
void test_make_unmake(Game g) {
    string str = g.PrintGame(false);

    vector<bool> possible_actions = g.GetActions(false);
    for (int i = 0; i <= 18; i++) {
        if (possible_actions[i]) {
            g.MakeMove(i);
            g.UnmakeMove();
            string str2 = g.PrintGame(false);
            if (str != str2) {
                cout << "\n\nDIFFERENCE DETECTED: Problem with: " << i << " action \n\n";
                cout << str << "  \n -------------------------- \n" << str2 << '\n';
                exit(0); 
            }
        }
    }

    return;
}