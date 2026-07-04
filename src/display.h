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

    // Interactive-play helpers. PrintBoard only shows cards dealt as of g.stage.
    void PrintBoard(Game &g, int humanPlayer);
    void PrintActionMenu(const std::vector<bool> &actions, Game &g);
    void PrintHandResult(Game &g, int humanPlayer, int utilityForActingPlayer);

    // The trained strategy at the current decision point, legal actions only.
    void PrintOptimalStrategy(const std::vector<double> &strategy, const std::vector<bool> &possible_actions, bool isHumanTurn);

    // Human label for a raw move_names entry (which stays the stable key).
    std::string ActionDisplayLabel(const std::string &moveName);
}
