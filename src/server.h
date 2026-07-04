#pragma once

class CFRSolver;

namespace Server {
    // Blocks, serving the live web UI and JSON API until the process is killed.
    void Start(CFRSolver &cfr, int port);
}
