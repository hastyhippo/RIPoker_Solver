#pragma once
#include <string>

class CFRSolver;

// Builds a position's full JSON payload (stage/board/pot/bet/history/visits/
// trail/rows). Shared by the live server and the static exporter so the
// deployed page and the local server render byte-identical positions.
namespace Position {
    // rows are split per (hand value, flush category) - see BuildJSON.
    std::string BuildJSON(CFRSolver &cfr, int stage, int board0Val, int board1Val, const std::string &history);

    // Rank helpers (shared so server/export/position agree on card labels).
    std::string RankName(int value);
    int RankValue(const std::string &label);

    // Human flush label for a (stage, flushInfo); "" preflop (no board).
    std::string FlushLabel(int stage, int flushInfo);
}
