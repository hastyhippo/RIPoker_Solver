#pragma once
#include <string>

class CFRSolver;

namespace Export {
    // Groups the trained positionMap by decision point (dropping hole-card
    // identity) and writes it as JSON for the web viewer. Only node-groups
    // with at least minVisits total visits are included.
    void WriteSolverJSON(CFRSolver &cfr, const std::string &path, int minVisits);
}
