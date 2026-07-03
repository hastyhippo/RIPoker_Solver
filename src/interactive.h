#pragma once

class CFRSolver;

// Lets a human play hands against the solver's trained average strategy
// from the terminal. humanPlayer is 0 or 1 (0 = out of position).
void PlayVsSolver(CFRSolver &cfr, int humanPlayer);
