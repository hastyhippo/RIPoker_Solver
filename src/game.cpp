#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <map>
#include <unordered_map>
#include <cassert>

#include "node.h"
#include "game.h"
#include "defines.h"

// Reduced from the original 6-size grid ({0.33, 0.66, 1, 1.5, 2, 3}) to cut
// down the branching factor (and resulting infoset explosion) of the game tree.
vector<double> bet_sizings = {0.33, 0.75, 1.25};
vector<double> raise_sizings = {2.2, 2.6, 3, 4};
vector<string> move_names = {"Check", "Call", "Fold", "Allin", "B33", "B75", "B125", "R2.2", "R2.6", "R3", "R4"};

Game::Game(int stack0, int stack1) {
    this->deck = Deck();
    // Create the cards that the players receive
    this->hands = vector<Card>(NUM_PLAYERS);
    this->board = vector<Card>(2);
    this->effective_stack = {stack0, stack1};
    this->pot = 0;
    this->abstractHistory = "";
    this->player = 0;
    this->stage = 0;
}

// GetUtility() is from `player`'s (inconsistent) perspective; re-express it
// from a single fixed player-0 perspective by multiplying by player's sign.
int Game::GetUtilityForPlayer0() {
    return (player == 0 ? 1 : -1) * GetUtility();
}

// Total chips in play now: committed pot plus both current-street bets.
int Game::KeyPot() {
    return pot + bet_states[stage][0] + bet_states[stage][1];
}

// Chips the acting player owes to call (0 when not facing a bet).
int Game::KeyBet() {
    return max(0, bet_states[stage][1 - player] - bet_states[stage][player]);
}

string printArray(vector<int>v) {
    string str = "";
    str += "| ";
    for (int a : v) {
        str += to_string(a) + " | ";
    }
    str += "\n";
    return str;
}

string Game::PrintGame(bool print) {
    string str = "";
    str += "\n";
    str += "Stage: " + to_string(stage) + '\n';
    str += "Number of players: " +  to_string(NUM_PLAYERS) +  " | Action on: " + to_string(player) + " | OOP: " + to_string(first_to_act) + " \n";
    str +=  "Board: " + board[0].getName() + " | " + board[1].getName() + '\n'; 
    str += "Pot: " + to_string(pot) + '\n';
    str += "\nMoveHistory: ";
    for (const Move &m : moveHistory) {
        if (m.kind == Move::StreetClose) str += "| ";
        else if (m.kind == Move::Fold) str += to_string(m.player) + "F ";
        else str += to_string(m.player) + "B" + to_string(m.amount) + " ";
    }
    str += "\nAbstractedHistory:" + abstractHistory + "\n";

    for (int i = 0; i < NUM_PLAYERS; i++) {
        str += "Player: " + to_string(i) + " | Effective Stack: " + to_string(effective_stack[i]) + " | Stack: " +  to_string(effective_stack[i])  + " | Card: " + hands[i].getName() + "\n";
    }

    str += "Bet states\n";
    for (int i = 0; i < 3; i++) {
        str += "Stage:  " + to_string(i) + ": ";
        str += printArray(bet_states[i]);
    }
    str += "Terminal: " + to_string(terminal) + "\n\n";
    if(print) cout << str;
    return str;
}

void Game::InitialiseGame(int OOP) {
    deck.Shuffle();
    for (int i = 0; i < NUM_PLAYERS; i++) {
        hands[i] = deck.Draw();
    }

    assert(board.size() == 2);

    board[0] = deck.Draw(); 
    board[1] = deck.Draw();

    bet_states = {{0, 0}, {0,0}, {0,0}};

    // Antes are modelled as live preflop bets rather than dead money     
    effective_stack[OOP] -= ANTE_FIRST_TO_ACT;
    bet_states[PREFLOP][OOP] = ANTE_FIRST_TO_ACT;
    effective_stack[1 - OOP] -= ANTE_SECOND_TO_ACT;
    bet_states[PREFLOP][1 - OOP] = ANTE_SECOND_TO_ACT;

    first_to_act = OOP;
    player = first_to_act;
    stage = 0;
    terminal = false;
    moveHistory = {};
    abstractHistory = "";
}

int Game::GetUtility() {
    assert(terminal == true);
    if (stage != 3 && (bet_states[stage][0] != bet_states[stage][1])) {
        int winner = bet_states[stage][0] > bet_states[stage][1] ? 0 : 1;
        int profit = (pot/2) + bet_states[stage][1-winner];
        return (winner == player ? 1 : -1) * profit;
    } else {
        vector<int>  strengths = {
            Card::getStrength(hands[0], board[0], board[1]) , 
            Card::getStrength(hands[1], board[0], board[1])
        };

        if (strengths[0] == strengths[1]) {
            return 0;
        } else {
            return (strengths[player] < strengths[1-player] ? 1 : -1) * (pot/2);
        }
    }
}

vector<bool> Game::GetActions(bool print) {
    vector<bool> actions(NUM_ACTIONS,false);
    if(terminal) return actions;

    if(bet_states[stage][0] == 0 && bet_states[stage][1] == 0) {
        // X is available
        actions[CHECK] = true;
        unordered_map<int, int> values;

        for (int dx = 0; dx <  NUM_BETS; dx++) {
            int i = dx + MISC_ACTIONS;
            int bet_size = pot * bet_sizings[dx];
            if (values.count(bet_size) == 0 && bet_size < effective_stack[player] && bet_size > 0) {
                values[bet_size]++;
                actions[i] = true;
            }
        }
        // allin always available
        if(effective_stack[1 - player] > 0 && effective_stack[player] > 0) {
            actions[ALLIN] = true;
        }
    } else if (bet_states[stage][player] == bet_states[stage][1 - player]) {
        // Preflop option after OOP's limp: IP may check it back or raise.
        actions[CHECK] = true;

        unordered_map<int, int> values;
        for (int dx = 0; dx < NUM_RAISES; dx++) {
            int i = MISC_ACTIONS + NUM_BETS + dx;
            int raise_size = bet_states[stage][1 - player] * raise_sizings[dx];
            int extra_chips = raise_size - bet_states[stage][player];
            if (values.count(raise_size) == 0 && extra_chips < effective_stack[player] && extra_chips > 0) {
                values[raise_size]++;
                actions[i] = true;
            }
        }
        if(effective_stack[1 - player] > 0 && effective_stack[player] > 0) {
            actions[ALLIN] = true;
        }
    } else {
        // Facing a bet/raise - Fold/Call always available
        actions[FOLD] = true;
        actions[CALL] = true;

        unordered_map<int, int> values;

        for (int dx = 0; dx < NUM_RAISES; dx++) {
            int i = MISC_ACTIONS + NUM_BETS + dx;
            int raise_size = bet_states[stage][1 - player] * raise_sizings[dx];
            int extra_chips = raise_size - bet_states[stage][player];
            if (values.count(raise_size) == 0 && extra_chips < effective_stack[player] && extra_chips > 0) {
                values[raise_size]++;
                actions[i] = true;
            }
        }
        if(effective_stack[1 - player] > 0 && effective_stack[player] > 0) {
            actions[ALLIN] = true;
        }
    }

    if(print) {
        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (actions[i]) {
                cout << i << " | " << move_names[i];
                if (i >= MISC_ACTIONS && i < MISC_ACTIONS + NUM_BETS ) {
                    cout << " : " << (int)(pot * bet_sizings[i-MISC_ACTIONS]) << '\n';
                } else if (i >= MISC_ACTIONS + NUM_BETS && i < MISC_ACTIONS + NUM_BETS + NUM_RAISES) {
                    cout << " : " << (int)(bet_states[stage][1 - player] * raise_sizings[i-(MISC_ACTIONS + NUM_BETS)])  << '\n';
                } else {
                    cout << '\n';
                }
            } 
        }
        cout << '\n';
    }

    return actions;
}

void Game::MakeMove(int move_type) {
    // int effective_stack = min(effective_stack[0], effective_stack[1]);
    bool update_stage = false;
    assert(terminal == false);

    if (move_type == CHECK) {
        // Closes the street on check-behind, or on checking the preflop
        // option back (bets matched and nonzero after OOP's limp).
        bool checkedBehind = !moveHistory.empty() &&
            moveHistory.back().kind == Move::Chips && moveHistory.back().amount == 0;
        bool limpOption = bet_states[stage][player] > 0 &&
            bet_states[stage][player] == bet_states[stage][1 - player];
        if (checkedBehind || limpOption) {
            pot += bet_states[stage][0] + bet_states[stage][1]; // 0 unless limp option
            stage++;
            update_stage = true;
        }
        moveHistory.push_back({Move::Chips, (int8_t)player, 0});
        abstractHistory += Node::GetBetAction(0);

    } else if (move_type == FOLD) {
        moveHistory.push_back({Move::Fold, (int8_t)player, 0});
        terminal = true;
        abstractHistory += 'f';
    }
    else if (move_type == CALL) {
        // OOP's opening limp (first move of the hand) matches IP's ante-raise
        // without closing preflop: IP keeps the option to check or raise.
        bool openingLimp = stage == PREFLOP && moveHistory.empty();

        // Capped at own remaining stack (short all-in call). No side-pot
        // accounting: uncontested excess still lands in the shared pot.
        int call_size = min(bet_states[stage][1 - player] - bet_states[stage][player], effective_stack[player]);

        moveHistory.push_back({Move::Chips, (int8_t)player, call_size});
        abstractHistory += 'c';
        effective_stack[player] -= call_size;
        bet_states[stage][player] += call_size;

        // The option needs both players able to act; a limp that leaves
        // either side all-in closes the street like any other call.
        if (!(openingLimp && effective_stack[0] > 0 && effective_stack[1] > 0)) {
            // End state - increase pot size
            pot += bet_states[stage][0] + bet_states[stage][1];
            stage++;
            update_stage = true;

            // If either player is all-in, no further betting is possible
            if (effective_stack[0] == 0 || effective_stack[1] == 0) {
                terminal = true;
            }
        }
    } else if (move_type == ALLIN) {
        // equivalent of betting whole stack
        moveHistory.push_back({Move::Chips, (int8_t)player, effective_stack[player]});
        abstractHistory += 'a';
        assert(effective_stack[player] > 0);
        bet_states[stage][player] += effective_stack[player];
        effective_stack[player] = 0;

    } else if (move_type >= MISC_ACTIONS && move_type < MISC_ACTIONS + NUM_BETS) {
        //Bet
        int bet_size = bet_sizings[move_type - MISC_ACTIONS] * pot;
        assert(bet_size > 0);

        effective_stack[player] -= bet_size;
        bet_states[stage][player] = bet_size;

        moveHistory.push_back({Move::Chips, (int8_t)player, bet_size});
        abstractHistory += Node::GetBetAction(bet_size);
    } else if (move_type >= MISC_ACTIONS + NUM_BETS && move_type <= MISC_ACTIONS + NUM_BETS + NUM_RAISES) {
        //Raise
        int raise_size = bet_states[stage][1 - player] * raise_sizings[move_type - (MISC_ACTIONS + NUM_BETS)];
        assert(raise_size > 0);

        int extra_chips = raise_size - bet_states[stage][player];
        effective_stack[player] -= extra_chips;
        bet_states[stage][player] = raise_size;

        moveHistory.push_back({Move::Chips, (int8_t)player, extra_chips});
        abstractHistory += Node::GetRaiseAction(raise_size);
    }


    if (update_stage) {
        // Indicate new chance node and next stage
        moveHistory.push_back({Move::StreetClose, -1, 0});
        abstractHistory += ",";
        player = first_to_act;

        if (stage == 3 || terminal) {
            terminal = true;
            return;
        }

    } else {
        if(terminal) {
            return;
        }
        // Swap players
        player = 1 - player;
    }
}

// Removes the last abstractHistory token. Exact-mode "[N]"/"{N}" tokens are
// multi-char, so strip back to the opening bracket, not just one char.
void PopAbstractHistoryToken(string &abstractHistory) {
    if (abstractHistory.empty()) return;
    char last = abstractHistory.back();
    if (last == ']') {
        abstractHistory.erase(abstractHistory.rfind('['));
    } else if (last == '}') {
        abstractHistory.erase(abstractHistory.rfind('{'));
    } else {
        abstractHistory.pop_back();
    }
}

void Game::UnmakeMove() {
    assert(!moveHistory.empty());
    terminal = false;
    Move last = moveHistory.back();

    if (last.kind == Move::Fold) {
        moveHistory.pop_back();
        PopAbstractHistoryToken(abstractHistory);
        player = last.player;
        return;
    } else if (last.kind == Move::StreetClose) {
        stage--;
        // Undo the street-close pot bump (bet_states may be unequal after a
        // capped call; the subtraction doesn't depend on equality).
        pot -= bet_states[stage][0] + bet_states[stage][1];
        moveHistory.pop_back();
        PopAbstractHistoryToken(abstractHistory);
        last = moveHistory.back();
    }
    assert(!moveHistory.empty());

    moveHistory.pop_back();
    PopAbstractHistoryToken(abstractHistory);
    player = last.player;
    bet_states[stage][player] -= last.amount;
    effective_stack[player] += last.amount;
}

namespace {

// Decodes one exact-history token into a move index for g's current state
// and advances i past it. Sets ok=false when a token can't be decoded.
int DecodeToken(Game &g, const string &history, size_t &i, bool &ok) {
    char c = history[i];
    int move_type = -1;
    size_t tokenLen = 1;
    if (c == '0') {
        move_type = CHECK;
    } else if (c == 'f') {
        move_type = FOLD;
    } else if (c == 'c') {
        move_type = CALL;
    } else if (c == 'a') {
        move_type = ALLIN;
    } else if (c == '[') {
        size_t end = history.find(']', i);
        if (end == string::npos) { ok = false; return -1; }
        int amt = stoi(history.substr(i + 1, end - i - 1));
        tokenLen = end - i + 1;
        for (int dx = 0; dx < NUM_BETS; dx++) {
            if ((int)(bet_sizings[dx] * g.pot) == amt) { move_type = MISC_ACTIONS + dx; break; }
        }
    } else if (c == '{') {
        size_t end = history.find('}', i);
        if (end == string::npos) { ok = false; return -1; }
        int amt = stoi(history.substr(i + 1, end - i - 1));
        tokenLen = end - i + 1;
        int facing = g.bet_states[g.stage][1 - g.player];
        for (int dx = 0; dx < NUM_RAISES; dx++) {
            if ((int)(raise_sizings[dx] * facing) == amt) { move_type = MISC_ACTIONS + NUM_BETS + dx; break; }
        }
    } else {
        ok = false;
        return -1;
    }

    if (move_type == -1) { ok = false; return -1; }
    i += tokenLen;
    return move_type;
}

// Snapshot of g's live decision point: legal moves + resolved chip sizes.
TrailStep DecisionStepFor(Game &g) {
    TrailStep s;
    s.isReveal = false;
    s.revealSlot = -1;
    s.player = g.player;
    s.stack = g.effective_stack[g.player];
    s.chosen = -1;
    vector<bool> acts = g.GetActions(false);
    int facing = g.bet_states[g.stage][1 - g.player];
    for (int m = 0; m < NUM_ACTIONS; m++) {
        if (!acts[m]) continue;
        int size = -1;
        if (m == ALLIN) size = g.effective_stack[g.player];
        else if (m >= MISC_ACTIONS && m < MISC_ACTIONS + NUM_BETS) size = (int)(bet_sizings[m - MISC_ACTIONS] * g.pot);
        else if (m >= MISC_ACTIONS + NUM_BETS) size = (int)(raise_sizings[m - (MISC_ACTIONS + NUM_BETS)] * facing);
        s.legal.push_back(m);
        s.chipSize.push_back(size);
        // Play the move on a scratch copy to capture the history it leads to,
        // so the frontend can navigate there on click.
        g.MakeMove(m);
        s.gotoHistory.push_back(g.abstractHistory);
        g.UnmakeMove();
    }
    return s;
}

} // namespace

Game Game::ReplayExactHistory(int stack0, int stack1, const string &history, bool &ok) {
    Game g(stack0, stack1);
    g.InitialiseGame(0);
    ok = true;

    size_t i = 0;
    while (i < history.size() && ok) {
        if (history[i] == ',') { i++; continue; } // street transition, not its own token
        int move_type = DecodeToken(g, history, i, ok);
        if (!ok) break;
        g.MakeMove(move_type);
    }
    return g;
}

vector<DecisionRecord> Game::ReplayDecisions(int stack0, int stack1, const string &history, bool &ok) {
    Game g(stack0, stack1);
    g.InitialiseGame(0);
    ok = true;
    vector<DecisionRecord> recs;

    size_t i = 0;
    while (i < history.size() && ok && !g.terminal) {
        if (history[i] == ',') { i++; continue; }
        DecisionRecord r;
        r.player = g.player;
        r.stage = g.stage;
        r.pot = g.KeyPot();
        r.bet = g.KeyBet();
        r.history = g.abstractHistory;
        vector<bool> acts = g.GetActions(false);
        r.legalCount = 0;
        for (bool b : acts) if (b) r.legalCount++;
        int move_type = DecodeToken(g, history, i, ok);
        if (!ok) break;
        r.moveType = move_type;
        recs.push_back(r);
        g.MakeMove(move_type);
    }
    return recs;
}

vector<TrailStep> Game::BuildTrail(int stack0, int stack1, const string &history, bool &ok) {
    Game g(stack0, stack1);
    g.InitialiseGame(0);
    ok = true;
    vector<TrailStep> trail;

    size_t i = 0;
    while (i < history.size() && ok && !g.terminal) {
        if (history[i] == ',') { i++; continue; }
        TrailStep step = DecisionStepFor(g);
        int stageBefore = g.stage;
        int move_type = DecodeToken(g, history, i, ok);
        if (!ok) break;
        step.chosen = move_type;
        trail.push_back(step);
        g.MakeMove(move_type);
        // A street close reveals the next board card between decisions.
        if (!g.terminal && g.stage != stageBefore) {
            TrailStep reveal;
            reveal.isReveal = true;
            reveal.revealSlot = g.stage - 1;
            reveal.player = -1;
            reveal.chosen = -1;
            reveal.pot = g.pot;                 // committed pot entering the new street
            reveal.history = g.abstractHistory; // click a card -> start of that street
            trail.push_back(reveal);
        }
    }
    // The last step is the live decision the strategy table is showing.
    if (ok && !g.terminal) trail.push_back(DecisionStepFor(g));
    return trail;
}