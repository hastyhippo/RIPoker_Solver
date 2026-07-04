#pragma once
#include <string>

class CFRSolver;

namespace Export {
    // Groups positionMap by decision point and writes JSON for the web viewer
    // (only groups with >= minVisits total visits).
    void WriteSolverJSON(CFRSolver &cfr, const std::string &path, int minVisits);
}
