#include <iostream>
#include <cstdint>
#include <cmath>
#include <map>
#include "game.h"

#include "cassert"
#include "defines.h"
#include "display.h"
#include "interactive.h"
#include "export.h"
#include "server.h"

using namespace std;

void test_make_unmake(Game g);
void test_strategy_sums_to_one(CFRSolver &cfr);

namespace {
    const long long PROGRESS_EVERY_N_HANDS = 100;
    const long long EXPLOIT_EVERY_N_HANDS = 10000;
    const int TOP_NODES_FOR_REPORT = 25;
    const int MIN_VISITS_FOR_EXPORT = 5;
    const char *EXPORT_PATH = "solver_export.json";
    const int DEFAULT_SERVE_PORT = 8080;
}

void Initialise() {
    Card::initialiseMap();
}

int main(int argc, char **argv) {
    Initialise();

    // Nodes below this many visits are dropped from the JSON export
    // (--export-min-visits N trims size for published artifacts).
    int exportMinVisits = MIN_VISITS_FOR_EXPORT;

    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--serve") {
            int port = (i + 1 < argc) ? atoi(argv[i + 1]) : DEFAULT_SERVE_PORT;
            if (port <= 0) port = DEFAULT_SERVE_PORT;
            CFRSolver cfr;
            Server::Start(cfr, port);
            return 0;
        }
        if (string(argv[i]) == "--export-min-visits" && i + 1 < argc) {
            int v = atoi(argv[i + 1]);
            if (v > 0) exportMinVisits = v;
        }
    }

    CFRSolver cfr;

    // Regression guard: confirm MakeMove/UnmakeMove round-trip symmetry
    // before spending any time training.
    {
        Game sanity(STARTING_STACK, STARTING_STACK);
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
                Display::PrintExploitability(cfr.iteration, cfr.ComputeExploitability());
            }
        }
        cout << "Enter number of hands to train (0 to stop training): ";
    }

    Display::PrintPreflopStrategy(cfr);
    Display::PrintFinalStrategyReport(cfr, TOP_NODES_FOR_REPORT);

    // Quick sanity check before exporting: every trained node's legal-action
    // probabilities should sum to 1.
    test_strategy_sums_to_one(cfr);
    cout << "Verified: strategies sum to 1 across " << cfr.positionMap.size() << " trained nodes.\n";

    Export::WriteSolverJSON(cfr, EXPORT_PATH, exportMinVisits);
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

// MakeMove/UnmakeMove round-trip: one level of make-unmake must be identical.
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

// Regression guard: every trained node's final strategy should sum to ~1.
void test_strategy_sums_to_one(CFRSolver &cfr) {
    const double EPSILON = 1e-3;
    for (auto &entry : cfr.positionMap) {
        Node *node = &entry.second;
        vector<double> strat = node->GetFinalStrategy(node->possible_actions);
        double sum = 0.0;
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (node->possible_actions[i]) sum += strat[i];
        }
        if (fabs(sum - 1.0) > EPSILON) {
            cerr << "\n\nSTRATEGY SUM MISMATCH: node " << entry.first
                 << " summed to " << sum << " (expected 1.0)\n\n";
            exit(1);
        }
    }
}
