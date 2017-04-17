/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 lucasart.
 *
 * Demolito is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Demolito is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <sstream>
#include <thread>
#include "uci.h"
#include "eval.h"
#include "search.h"
#include "htable.h"
#include "gen.h"

GameStack gameStack;

static std::thread Timer;

static uint64_t Hash = 1;
static int64_t TimeBuffer = 30;

static void intro()
{
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

    puts("id name Demolito\nid author lucasart");
    printf("option name UCI_Chess960 type check default %s\n", Chess960 ? "true" : "false");
    printf("option name Hash type spin default %" PRIu64 " min 1 max 1048576\n", Hash);
    printf("option name Threads type spin default %d min 1 max 64\n", Threads);
    printf("option name Contempt type spin default %d min -100 max 100\n", Contempt);
    printf("option name Time Buffer type spin default %" PRId64 " min 0 max 1000\n", TimeBuffer);
    puts("uciok");
}

static void setoption(std::istringstream& is)
{
    std::string token, name;

    is >> token;

    if (token != "name")
        return;

    while ((is >> token) && token != "value")
        name += token;

    if (name == "UCI_Chess960")
        is >> std::boolalpha >> Chess960;
    else if (name == "Hash") {
        is >> Hash;
        Hash = 1ULL << bb_msb(Hash);    // must be a power of two
        hash_resize(Hash);
    } else if (name == "Threads")
        is >> Threads;
    else if (name == "Contempt")
        is >> Contempt;
    else if (name == "TimeBuffer")
        is >> TimeBuffer;
}

static void position(std::istringstream& is)
{
    Position p[NB_COLOR];
    int idx = 0;

    std::string token, fen;
    is >> token;

    if (token == "startpos") {
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        is >> token;    // consume "moves" token (if present)
    } else if (token == "fen") {
        while (is >> token && token != "moves")
            fen += token + " ";
    } else
        return;

    pos_set(&p[idx], fen.c_str());
    gs_clear(&gameStack);
    gs_push(&gameStack, p[idx].key);

    // Parse moves (if any)
    while (is >> token) {
        move_t m = string_to_move(&p[idx], token.c_str());
        pos_move(&p[idx^1], &p[idx], m);
        idx ^= 1;
        gs_push(&gameStack, p[idx].key);
    }

    rootPos = p[idx];
}

static void go(std::istringstream& is)
{
    Limits lim;
    memset(&lim, 0, sizeof(lim));
    lim.depth = MAX_DEPTH;
    lim.movestogo = 30;

    std::string token;

    while (is >> token) {
        if (token == "depth")
            is >> lim.depth;
        else if (token == "nodes")
            is >> lim.nodes;
        else if (token == "movetime") {
            is >> lim.movetime;
            lim.movetime -= TimeBuffer;
        } else if (token == "movestogo")
            is >> lim.movestogo;
        else if ((rootPos.turn == WHITE && token == "wtime") || (rootPos.turn == BLACK && token == "btime"))
            is >> lim.time;
        else if ((rootPos.turn == WHITE && token == "winc") || (rootPos.turn == BLACK && token == "binc"))
            is >> lim.inc;
    }

    if (lim.time || lim.inc) {
        lim.movetime = ((lim.movestogo - 1) * lim.inc + lim.time) / lim.movestogo;
        lim.movetime = std::min(lim.movetime, lim.time - TimeBuffer);
    }

    if (Timer.joinable())
        Timer.join();

    Timer = std::thread(search_go, std::cref(lim), std::cref(gameStack));
}

static void eval()
{
    pos_print(&rootPos);
    printf("score %s\n", uci_format_score(evaluate(&rootPos)).c_str());
}

static void perft(std::istringstream& is)
{
    int depth;
    is >> depth;

    pos_print(&rootPos);
    printf("perft = %" PRIu64 "\n", gen_perft(&rootPos, depth));
}

Info ui;

void uci_loop()
{
    std::string command, token;

    while (std::getline(std::cin, command)) {
        std::istringstream is(command);
        is >> token;

        if (token == "uci")
            intro();
        else if (token == "setoption")
            setoption(is);
        else if (token == "isready")
            puts("readyok");
        else if (token == "ucinewgame") {
            hash_resize(Hash);
            memset(HashTable, 0, Hash << 20);
        } else if (token == "position")
            position(is);
        else if (token == "go")
            go(is);
        else if (token == "stop")
            signal = STOP;
        else if (token == "eval")
            eval();
        else if (token == "perft")
            perft(is);
        else if (token == "quit")
            break;
        else
            printf("unknown command: %s\n", command.c_str());
    }

    if (Timer.joinable())
        Timer.join();
}

void info_clear(Info *info)
{
    info->lastDepth = 0;
    info->best = info->ponder = 0;
    clock_gettime(CLOCK_MONOTONIC, &info->start);
}

void info_update(Info *info, int depth, int score, int64_t nodes, move_t pv[], bool partial)
{
    std::lock_guard<std::mutex> lk(info->mtx);

    if (depth > info->lastDepth) {
        info->best = pv[0];
        info->ponder = pv[1];

        if (partial)
            return;

        info->lastDepth = depth;

        printf("info depth %d score %s time %" PRId64 " nodes %" PRId64 " pv",
               depth, uci_format_score(score).c_str(), elapsed_msec(&info->start), nodes);

        // Because of e1g1 notation when Chess960 = false, we need to play the PV, just to
        // be able to print it. This is a defect of the UCI protocol (which should have
        // encoded castling as e1h1 regardless of Chess960 allowing coherent treatement).
        Position p[2];
        int idx = 0;
        p[idx] = rootPos;

        for (int i = 0; pv[i]; i++) {
            char str[6];
            move_to_string(&p[idx], pv[i], str);
            printf(" %s", str);
            pos_move(&p[idx^1], &p[idx], pv[i]);
            idx ^= 1;
        }

        puts("");
    }
}

void info_print_bestmove(const Info *info)
{
    std::lock_guard<std::mutex> lk(info->mtx);
    char best[6], ponder[6];
    move_to_string(&rootPos, info->best, best);
    move_to_string(&rootPos, info->ponder, ponder);
    printf("bestmove %s ponder %s\n", best, ponder);
}

move_t info_best(const Info *info)
{
    std::lock_guard<std::mutex> lk(info->mtx);
    return info->best;
}

int info_last_depth(const Info *info)
{
    std::lock_guard<std::mutex> lk(info->mtx);
    return info->lastDepth;
}

std::string uci_format_score(int score)
{
    std::ostringstream os;

    if (is_mate_score(score)) {
        const int dtm = score > 0
                        ? (MATE - score + 1) / 2
                        : (score - MATE + 1) / 2;
        os << "mate " << dtm;
    } else
        os << "cp " << score * 100 / EP;

    return os.str();
}
