#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <future>
#include <random>
#include "cfr_solver.h"
#include "node.h"
#include "game.h"
#include "defines.h"

void CFRSolver::Reset(int newStack0, int newStack1, bool cfrPlus) {
    positionMap.clear();
    iteration = 0;
    stack0 = newStack0;
    stack1 = newStack1;
    useCFRPlus = cfrPlus;
}

void CFRSolver::TrainCFR() {
    iteration++;
    Game g(stack0, stack1);
    g.InitialiseGame(0);
    CFR(g, 1, 1);
}

Node *CFRSolver::GetNode(const string &hash, const vector<bool> &possible_actions) {
    auto it = positionMap.find(hash);
    if (it == positionMap.end()) {
        it = positionMap.emplace(hash, Node(possible_actions)).first;
    }
    // The stored legality mask may not match a later visit's real
    // possible_actions; that's fine - live math always takes the current mask.
    return &it->second;
}

// 2p only. Fixed player-0-perspective utilities (p1/p2 are the players' reach
// probs); avoids the alternate-and-negate scheme that broke at street resets.
double CFRSolver::CFR(Game &g, double p1, double p2) {
    if (g.terminal) {
        return g.GetUtilityForPlayer0();
    }

    // Fixed before any MakeMove below can mutate g.player.
    int actingPlayer = g.player;

    vector<bool> possible_actions = g.GetActions(false);
    string hash = Node::GetHash(g);
    Node *currentNode = GetNode(hash, possible_actions);
    currentNode->visits++;

    currentNode->UpdateStrategy(possible_actions);
    vector<double> strategy = currentNode->strategy;

    vector<double> action_val(NUM_ACTIONS, 0.0);
    double node_utility = 0;

    double normalising_sum = 0.0;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (possible_actions[i]) normalising_sum += strategy[i];
    }

    // Player-0 utility, no per-child negation; only the acting player's reach
    // picks up this action's chance.
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (!possible_actions[i]) continue;

        double chance = strategy[i] / normalising_sum;

        g.MakeMove(i);
        if (actingPlayer == 0) {
            action_val[i] = CFR(g, p1 * chance, p2);
        } else {
            action_val[i] = CFR(g, p1, p2 * chance);
        }
        g.UnmakeMove();
        node_utility += action_val[i] * chance;
    }

    // Regret is in the acting player's perspective (negated for player 1).
    double perspective = (actingPlayer == 0) ? 1.0 : -1.0;
    vector<double> regrets(NUM_ACTIONS, 0.0);
    for (int i = 0; i < NUM_ACTIONS; i++) {
        if (possible_actions[i]) regrets[i] = perspective * (action_val[i] - node_utility);
    }

    // Regret weighted by opponent reach; average strategy by own reach.
    if (actingPlayer == 0) {
        currentNode->UpdateRegret(regrets, possible_actions, /*opp_reach=*/p2, /*own_reach=*/p1, iteration, useCFRPlus);
    } else {
        currentNode->UpdateRegret(regrets, possible_actions, /*opp_reach=*/p1, /*own_reach=*/p2, iteration, useCFRPlus);
    }

    return node_utility;
}

vector<double> CFRSolver::GetAverageStrategy(const string &hash, const vector<bool> &possible_actions) {
    auto it = positionMap.find(hash);
    if (it == positionMap.end()) {
        vector<double> uniform(NUM_ACTIONS, 0.0);
        int n = 0;
        for (int i = 0; i < NUM_ACTIONS; i++) if (possible_actions[i]) n++;
        for (int i = 0; i < NUM_ACTIONS; i++) if (possible_actions[i]) uniform[i] = 1.0 / (double)n;
        return uniform;
    }
    return it->second.GetFinalStrategy(possible_actions);
}

namespace {

// strength[hand][b0][b1] lookup table, built once (card ranks never change).
// Magic-static init keeps the first concurrent callers thread-safe.
int StrengthOf(int c, int b0, int b1) {
    static const vector<int> tab = [] {
        vector<int> t(NUM_CARDS * NUM_CARDS * NUM_CARDS, -1);
        for (int a = 0; a < NUM_CARDS; a++)
            for (int b = 0; b < NUM_CARDS; b++)
                for (int d = 0; d < NUM_CARDS; d++)
                    if (a != b && a != d && b != d)
                        t[(a * NUM_CARDS + b) * NUM_CARDS + d] = Card::getStrength(Card(a), Card(b), Card(d));
        return t;
    }();
    return tab[(c * NUM_CARDS + b0) * NUM_CARDS + b1];
}

// Node::GetHash's flushInfo, recomputed from raw card indices.
int FlushInfoFor(int card, int b0, int b1, int stage) {
    if (stage == FLOP) return (card % NUM_SUITS == b0 % NUM_SUITS) ? 1 : 0;
    if (stage == TURN) {
        bool handSuited = (card % NUM_SUITS == b0 % NUM_SUITS), boardSuited = (b0 % NUM_SUITS == b1 % NUM_SUITS);
        if (handSuited && !boardSuited) return 1;
        if (handSuited && boardSuited) return 2;
        if (!handSuited && boardSuited) return 3;
    }
    return 0;
}

} // namespace

// Terminal payoff accumulation: V[brCard] += sum over consistent opponent
// cards (and any undealt board runouts) of reach * br_player's utility.
void CFRSolver::BREvalTerminal(Game &g, int br_player, const vector<double> &oppReach, int b0, int b1, vector<double> &V) {
    int stage = g.stage;
    int revealed = (b0 >= 0) + (b1 >= 0);

    // Fold/uncalled ending: utility is card-independent (same math as GetUtility).
    if (stage != 3 && g.bet_states[stage][0] != g.bet_states[stage][1]) {
        int winner = g.bet_states[stage][0] > g.bet_states[stage][1] ? 0 : 1;
        double profit = (double)(g.pot / 2 + g.bet_states[stage][1 - winner]);
        double uBr = (winner == br_player) ? profit : -profit;
        // Undealt board slots multiply the count of consistent full deals:
        // each draws from the deck minus hole cards and already-dealt board.
        double mult = 1.0;
        for (int slot = revealed; slot < NUM_BOARD_CARDS; slot++) {
            mult *= NUM_CARDS - NUM_PLAYERS - slot;
        }
        for (int br = 0; br < NUM_CARDS; br++) {
            if (br == b0 || br == b1) continue;
            double sum = 0;
            for (int opp = 0; opp < NUM_CARDS; opp++) {
                if (opp != br) sum += oppReach[opp]; // board cards already zeroed
            }
            V[br] += mult * sum * uBr;
        }
        return;
    }

    // Showdown: enumerate any undealt board cards (all-in before the reveal).
    double half = (double)(g.pot / 2);
    for (int br = 0; br < NUM_CARDS; br++) {
        if (br == b0 || br == b1) continue;
        double acc = 0;
        for (int opp = 0; opp < NUM_CARDS; opp++) {
            if (opp == br || oppReach[opp] == 0) continue;
            double u = 0;
            if (revealed == 2) {
                int sb = StrengthOf(br, b0, b1), so = StrengthOf(opp, b0, b1);
                u = (sb == so) ? 0 : (sb < so ? half : -half);
            } else if (revealed == 1) {
                for (int c1 = 0; c1 < NUM_CARDS; c1++) {
                    if (c1 == br || c1 == opp || c1 == b0) continue;
                    int sb = StrengthOf(br, b0, c1), so = StrengthOf(opp, b0, c1);
                    u += (sb == so) ? 0 : (sb < so ? half : -half);
                }
            } else {
                for (int c0 = 0; c0 < NUM_CARDS; c0++) {
                    if (c0 == br || c0 == opp) continue;
                    for (int c1 = 0; c1 < NUM_CARDS; c1++) {
                        if (c1 == br || c1 == opp || c1 == c0) continue;
                        int sb = StrengthOf(br, c0, c1), so = StrengthOf(opp, c0, c1);
                        u += (sb == so) ? 0 : (sb < so ? half : -half);
                    }
                }
            }
            acc += oppReach[opp] * u;
        }
        V[br] += acc;
    }
}

// Public-tree walk. br_player sees only its card + public state (chance is
// branched at the reveal, so it is NOT clairvoyant about runout or opponent).
vector<double> CFRSolver::BRWalk(Game &g, int br_player, const vector<double> &oppReach, int b0, int b1) {
    vector<double> V(NUM_CARDS, 0.0);
    vector<bool> acts = g.GetActions(false);
    int actor = g.player;
    int stageBefore = g.stage;

    // Opponent node: average strategy per infoset, deduped by (value, flushInfo).
    vector<vector<double>> strat;
    if (actor != br_player) {
        strat.assign(NUM_CARDS, {});
        for (int opp = 0; opp < NUM_CARDS; opp++) {
            if (oppReach[opp] == 0 || opp == b0 || opp == b1) continue;
            int fi = FlushInfoFor(opp, b0, b1, stageBefore);
            int slot = (opp / NUM_SUITS) * NUM_SUITS + fi;
            if (!strat[slot].empty()) continue;
            string key = Node::BuildKey(opp / NUM_SUITS, b0 >= 0 ? b0 / NUM_SUITS : 0, b1 >= 0 ? b1 / NUM_SUITS : 0,
                                        fi, stageBefore, g.KeyPot(), g.KeyBet(), g.abstractHistory);
            strat[slot] = GetAverageStrategy(key, acts);
        }
    }

    bool firstAction = true;
    for (int a = 0; a < NUM_ACTIONS; a++) {
        if (!acts[a]) continue;

        // The opponent mixes into this action; the BR player's reach is fixed.
        vector<double> childReach = oppReach;
        if (actor != br_player) {
            double total = 0;
            for (int opp = 0; opp < NUM_CARDS; opp++) {
                if (childReach[opp] == 0) continue;
                childReach[opp] *= strat[(opp / NUM_SUITS) * NUM_SUITS + FlushInfoFor(opp, b0, b1, stageBefore)][a];
                total += childReach[opp];
            }
            // The opponent never plays this line: exact-zero contribution.
            if (total == 0) continue;
        }

        g.MakeMove(a);
        vector<double> child(NUM_CARDS, 0.0);
        if (g.terminal) {
            BREvalTerminal(g, br_player, childReach, b0, b1, child);
        } else if (g.stage != stageBefore) {
            // Chance node: branch over the revealed card, removing it from play.
            // brChanceSamples > 0 draws a weighted uniform subset (Monte Carlo).
            vector<int> cands;
            for (int c = 0; c < NUM_CARDS; c++) {
                if (c != b0 && c != b1) cands.push_back(c);
            }
            double weight = 1.0;
            if (brChanceSamples > 0 && (int)cands.size() > brChanceSamples) {
                static thread_local mt19937 rng{RandomSeed()};
                // Partial Fisher-Yates: first k entries become the sample.
                for (int i = 0; i < brChanceSamples; i++) {
                    uniform_int_distribution<int> d(i, (int)cands.size() - 1);
                    swap(cands[i], cands[d(rng)]);
                }
                weight = (double)cands.size() / brChanceSamples;
                cands.resize(brChanceSamples);
            }

            // The flop fan-out runs in parallel; deeper levels stay sequential.
            // WASM on GitHub Pages has no SharedArrayBuffer (no COOP/COEP), so
            // std::async threads aren't available - run fully sequential there.
#ifdef __EMSCRIPTEN__
            bool parallel = false;
#else
            bool parallel = (stageBefore == PREFLOP);
#endif
            vector<pair<int, future<vector<double>>>> futs;
            for (int c : cands) {
                vector<double> reachC = childReach;
                reachC[c] = 0;
                int nb0 = b0 < 0 ? c : b0, nb1 = b0 < 0 ? b1 : c;
                if (parallel) {
                    futs.push_back({c, async(launch::async, [this, g, reachC, br_player, nb0, nb1]() mutable {
                        return BRWalk(g, br_player, reachC, nb0, nb1);
                    })});
                } else {
                    vector<double> sub = BRWalk(g, br_player, reachC, nb0, nb1);
                    for (int br = 0; br < NUM_CARDS; br++) {
                        if (br != c) child[br] += weight * sub[br];
                    }
                }
            }
            for (auto &f : futs) {
                vector<double> sub = f.second.get();
                for (int br = 0; br < NUM_CARDS; br++) {
                    if (br != f.first) child[br] += weight * sub[br];
                }
            }
        } else {
            child = BRWalk(g, br_player, childReach, b0, b1);
        }
        g.UnmakeMove();

        if (actor == br_player) {
            // Max per own card: each brCard is its own infoset here.
            if (firstAction) V = child;
            else for (int br = 0; br < NUM_CARDS; br++) V[br] = max(V[br], child[br]);
        } else {
            for (int br = 0; br < NUM_CARDS; br++) V[br] += child[br];
        }
        firstAction = false;
    }
    return V;
}

double CFRSolver::ExactBestResponseValue(int br_player) {
    Game g(stack0, stack1);
    g.InitialiseGame(0); // dealt cards are dummies; only money state is used
    vector<double> oppReach(NUM_CARDS, 1.0);
    vector<double> V = BRWalk(g, br_player, oppReach, -1, -1);
    double total = 0;
    for (double v : V) total += v;
    // Normalize by the count of ordered deals (br, opp, board0, board1).
    return total / ((double)NUM_CARDS * (NUM_CARDS - 1) * (NUM_CARDS - 2) * (NUM_CARDS - 3));
}

double CFRSolver::ComputeExploitability(int chanceSamples) {
    // Each term is that player's own best-response EV; the sum is 0 at equilibrium.
    brChanceSamples = chanceSamples;
    double e = (ExactBestResponseValue(0) + ExactBestResponseValue(1)) / 2.0;
    brChanceSamples = 0;
    return e;
}
