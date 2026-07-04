#pragma once
#include <string>
#include <vector>

class Game;
class CFRSolver;

namespace Display {
    // Training progress, printed periodically (not every iteration).
    void PrintTrainingProgress(long long iteration, long long totalIterations, size_t numNodes);
    void PrintExploitability(long long iteration, double exploitability);

    // Final report: the time-averaged strategy for the topN most-visited
    // information sets, formatted as a table.
    void PrintFinalStrategyReport(CFRSolver &cfr, int topN);

    // Basic preflop strategy: the first-to-act decision (empty history) for
    // every possible hole card, lowest to highest.
    void PrintPreflopStrategy(CFRSolver &cfr);

    // Interactive-play helpers. PrintBoard only shows cards that have
    // actually been dealt as of g.stage - never reveals future cards.
    // (Game/Card aren't const-correct anywhere in this codebase, so these
    // take a plain reference rather than introducing const just here.)
    void PrintBoard(Game &g, int humanPlayer);
    void PrintActionMenu(const std::vector<bool> &actions, Game &g);
    void PrintHandResult(Game &g, int humanPlayer, int utilityForActingPlayer);

    // Prints the solver's time-averaged ("optimal") strategy for the current
    // decision point, restricted to legal actions - used during interactive
    // play so the human can see what the trained strategy recommends here.
    void PrintOptimalStrategy(const std::vector<double> &strategy, const std::vector<bool> &possible_actions, bool isHumanTurn);

    // Translates a raw move_names entry ("B200", "R2.2", ...) into a clearer
    // label for display (the raw form stays the stable identifier used for
    // legality-array indexing/color-keying elsewhere).
    std::string ActionDisplayLabel(const std::string &moveName);
}
