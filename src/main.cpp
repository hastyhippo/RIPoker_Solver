#include <iostream>
#include <cstdint>
#include <map>
#include "game.h"

#include "cassert"
#include "defines.h"

using namespace std;

map<int,int> handStrengthMap;
void test_make_unmake(Game g);

void Initialise() {
    Card::initialiseMap();
}

int main() {
    Initialise();    
    CFRSolver cfr;  
    while(true) {
        int iterations = 0;
        cin >> iterations;
        if (iterations > 0) {
            while (iterations--) {
                cfr.TrainCFR();
            }
        } else break; 
    }

    for (auto a : cfr.positionMap) {
        if(cfr.positionCount[a.first] < 3) continue;
        Node::ReverseHash(a.first);
        cout << " | Count: " << cfr.positionCount[a.first] << "\n" ;
        for (auto b : a.second->strategy) {
            cout << b << " | ";
        }
        cout << "\n";
        for (auto b : a.second->regret_sum) {
            cout << b << " | ";
        }
        cout << "\n\n";
    }
    // Game g(STARTING_STACK);
    // g.InitialiseGame(0);
    // g.PrintGame(true);

    // // test_make_unmake(g);

    // while (!g.terminal) {
    //     g.PrintGame(true);

    //     vector<bool> possible_actions = g.GetActions(true);
    //     test_make_unmake(g);

    //     int action;
    //     cin >> action;
    //     if (action >= 0 && action <= 18 && !g.terminal && possible_actions[action]) {
    //         g.MakeMove(action);
    //     } else if (action == -1 && !g.moveHistory.empty()) {
    //         g.UnmakeMove();
    //     } else {
    //         break;
    //     }

    //     g.PrintGame(true);
    //     if(g.terminal) {
    //         cout << "player " << g.player << " | UTILITY: " << g.GetUtility() << "\n";
    //     }
    //     // test_make_unmake(g);
    // }
    // for (auto s : g.moveHistory) {
    //     cout << s << " ";
    // }

    // return 0;
}

// game.cpp make_move and unmake_move tests
    // By induction, we just have to show all game state properties are
    // identical for one level of make-unmake
void test_make_unmake(Game g) {
    if (g.bet_states.size() != 3 || g.bet_states[0].size() != 2) {
        cerr << "bet_states not initialised properly\n";
        exit(1);
    }

    string str = g.PrintGame(false);

    vector<bool> possible_actions = g.GetActions(false);
    for (int i = 0; i < NUM_ACTIONS; i++) {

        if (possible_actions[i]) {
            cout << "i: " << i;
            g.MakeMove(i);
            //  g.PrintGame(true);

            g.UnmakeMove();
            string str2 = g.PrintGame(false);

            if (str != str2) {
                cerr << "\n\nDIFFERENCE DETECTED: Problem with: " << i << " action \n\n";
                cerr << str << "  \n -------------------------- \n" << str2 << '\n';
                exit(0); 
            }
            cout << " passed | ";
        }
        
    }
    cout << "\n";
    return;
}