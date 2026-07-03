#include <iostream>
#include <cstdint>
#include <map>
#include "game.h"

#include "cassert"
#include "defines.h"
#include "display.h"
#include "interactive.h"
#include "export.h"

using namespace std;

void test_make_unmake(Game g);

namespace {
    const long long PROGRESS_EVERY_N_HANDS = 100;
    const long long EXPLOIT_EVERY_N_HANDS = 500;
    const int EXPLOIT_SAMPLES = 200;
    const int TOP_NODES_FOR_REPORT = 25;
    const int MIN_VISITS_FOR_EXPORT = 5;
    const char *EXPORT_PATH = "solver_export.json";
}

void Initialise() {
    Card::initialiseMap();
}

int main() {
    Initialise();
    CFRSolver cfr;

    // Regression guard: confirm MakeMove/UnmakeMove round-trip symmetry
    // before spending any time training.
    {
        Game sanity(STARTING_STACK);
        sanity.InitialiseGame(0);
        test_make_unmake(sanity);
    }

    cout << "Enter number of hands to train (0 to stop training): ";
    long long iterations = 0;
    while (cin >> iterations && iterations > 0) {
        for (long long i = 0; i < iterations; i++) {
            cfr.TrainCFR();

            if (cfr.iteration % PROGRESS_EVERY_N_HANDS == 0) {
                Display::PrintTrainingProgress(i + 1, iterations, cfr.positionMap.size());
            }
            if (cfr.iteration % EXPLOIT_EVERY_N_HANDS == 0) {
                double exploitability = cfr.EstimateExploitability(EXPLOIT_SAMPLES);
                Display::PrintExploitability(cfr.iteration, exploitability);
            }
        }
        cout << "Enter number of hands to train (0 to stop training): ";
    }

    Display::PrintPreflopStrategy(cfr);
    Display::PrintFinalStrategyReport(cfr, TOP_NODES_FOR_REPORT);

    Export::WriteSolverJSON(cfr, EXPORT_PATH, MIN_VISITS_FOR_EXPORT);
    cout << "Exported solver data to " << EXPORT_PATH << " (open web/index.html to browse it)\n";

    cout << "Play against the trained solver? (y/n): ";
    char playAnswer = 'n';
    cin >> playAnswer;
    if (playAnswer == 'y' || playAnswer == 'Y') {
        cout << "Play as player 0 (out of position) or player 1 (in position)? ";
        int humanPlayer = 0;
        cin >> humanPlayer;
        if (humanPlayer != 0 && humanPlayer != 1) humanPlayer = 0;
        PlayVsSolver(cfr, humanPlayer);
    }

    return 0;
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
            g.MakeMove(i);
            g.UnmakeMove();
            string str2 = g.PrintGame(false);

            if (str != str2) {
                cerr << "\n\nDIFFERENCE DETECTED: Problem with: " << i << " action \n\n";
                cerr << str << "  \n -------------------------- \n" << str2 << '\n';
                exit(1);
            }
        }
    }
}
