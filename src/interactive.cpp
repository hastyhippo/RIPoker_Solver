#include <iostream>
#include <random>
#include <vector>
#include <string>

#include "interactive.h"
#include "cfr_solver.h"
#include "node.h"
#include "game.h"
#include "display.h"
#include "defines.h"

using namespace std;

namespace {

int SampleAction(const vector<double> &strategy, const vector<bool> &possible_actions, mt19937 &rng) {
    vector<int> legalIndices;
    vector<double> weights;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (!possible_actions[i]) continue;
        legalIndices.push_back(i);
        weights.push_back(max(0.0, strategy[i]));
    }
    discrete_distribution<int> dist(weights.begin(), weights.end());
    return legalIndices[dist(rng)];
}

// Returns the chosen action index, or -1 if input is exhausted (EOF/Ctrl-D)
// so the caller can end the session instead of looping forever.
int ReadHumanMove(const vector<bool> &possible_actions) {
    while (true) {
        cout << "Your move: ";
        int move;
        if (!(cin >> move)) {
            if (cin.eof()) return -1;
            cin.clear();
            cin.ignore(10000, '\n');
            cout << "Please enter a number.\n";
            continue;
        }
        if (move >= 0 && move < NUM_ACTIONS && possible_actions[move]) return move;
        cout << "That action isn't available right now.\n";
    }
}

}

void PlayVsSolver(CFRSolver &cfr, int humanPlayer) {
    random_device rd;
    mt19937 rng(rd());

    while (true) {
        Game g(STARTING_STACK);
        g.InitialiseGame(0);
        Display::PrintBoard(g, humanPlayer);

        int lastStage = g.stage;
        while (!g.terminal) {
            vector<bool> possible_actions = g.GetActions(false);
            string hash = Node::GetHash(g);
            vector<double> strat = cfr.GetAverageStrategy(hash, possible_actions);
            int move;

            if (g.player == humanPlayer) {
                Display::PrintOptimalStrategy(strat, possible_actions, true);
                Display::PrintActionMenu(possible_actions, g);
                move = ReadHumanMove(possible_actions);
                if (move < 0) return; // input exhausted - end the session
            } else {
                Display::PrintOptimalStrategy(strat, possible_actions, false);
                move = SampleAction(strat, possible_actions, rng);
                cout << "Opponent " << move_names[move] << "s.\n";
            }

            g.MakeMove(move);
            if (g.stage != lastStage && !g.terminal) {
                Display::PrintBoard(g, humanPlayer);
                lastStage = g.stage;
            }
        }

        int utility = g.GetUtility();
        Display::PrintHandResult(g, humanPlayer, utility);

        cout << "Play another hand? (y/n): ";
        char again = 'n';
        if (!(cin >> again) || (again != 'y' && again != 'Y')) break;
    }
}
