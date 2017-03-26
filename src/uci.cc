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

static Position rootPos;
static search::Limits lim;
static std::thread Timer;

static uint64_t Hash = 1;
static int TimeBuffer = 30;

static void intro()
{
    std::cout << "id name Demolito\nid author lucasart\n" << std::boolalpha
              << "option name UCI_Chess960 type check default " << Chess960 << '\n'
              << "option name Hash type spin default " << Hash << " min 1 max 1048576\n"
              << "option name Threads type spin default " << search::Threads << " min 1 max 64\n"
              << "option name Contempt type spin default " << search::Contempt << " min -100 max 100\n"
              << "option name Time Buffer type spin default " << TimeBuffer << " min 0 max 1000\n"
              << "uciok" << std::endl;
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
        is >> search::Threads;
    else if (name == "Contempt")
        is >> search::Contempt;
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

    pos_set(&p[idx], fen);
    gs_clear(&gameStack);
    gs_push(&gameStack, p[idx].key);

    // Parse moves (if any)
    while (is >> token) {
        Move m;
        move_from_string(&p[idx], token, &m);
        pos_move(&p[idx^1], &p[idx], m);
        idx ^= 1;
        gs_push(&gameStack, p[idx].key);
    }

    rootPos = p[idx];
}

static void go(std::istringstream& is)
{
    std::string token;
    lim.movestogo = 30;

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

    Timer = std::thread(search::bestmove, std::cref(rootPos), std::cref(lim), std::cref(gameStack));
}

static void eval()
{
    pos_print(&rootPos);
    std::cout << "score " << uci_format_score(evaluate(&rootPos)) << std::endl;
}

static void perft(std::istringstream& is)
{
    int depth;
    is >> depth;

    pos_print(&rootPos);
    std::cout << "score " << gen_perft(&rootPos, depth) << std::endl;
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
        else if (token == "isready") {
            hash_resize(Hash);
            std::cout << "readyok" << std::endl;
        } else if (token == "ucinewgame")
            memset(HashTable, 0, Hash << 20);
        else if (token == "position")
            position(is);
        else if (token == "go")
            go(is);
        else if (token == "stop")
            search::signal = STOP;
        else if (token == "eval")
            eval();
        else if (token == "perft")
            perft(is);
        else if (token == "quit") {
            free(HashTable);
            break;
        } else
            std::cout << "unknown command: " << command << std::endl;
    }

    if (Timer.joinable())
        Timer.join();
}

void info_clear(Info *info)
{
    info->lastDepth = 0;
    info->bestMove = info->ponderMove = 0;
    info->clock.reset();
}

void info_update(Info *info, const Position *pos, int depth, int score, uint64_t nodes,
                 move_t pv[], bool partial)
{
    std::lock_guard<std::mutex> lk(info->mtx);

    if (depth > info->lastDepth) {
        info->bestMove = pv[0];
        info->ponderMove = pv[1];

        if (partial)
            return;

        info->lastDepth = depth;

        std::ostringstream os;
        const auto elapsed = info->clock.elapsed() + 1;  // Prevent division by zero

        os << "info depth " << depth << " score " << uci_format_score(score)
           << " time " << elapsed << " nodes " << nodes
           << " nps " << (1000 * nodes / elapsed) << " pv";

        // Because of e1g1 notation when Chess960 = false, we need to play the PV, just to
        // be able to print it. This is a defect of the UCI protocol (which should have
        // encoded castling as e1h1 regardless of Chess960 allowing coherent treatement).
        Position p[2];
        int idx = 0;
        p[idx] = *pos;

        for (int i = 0; pv[i]; i++) {
            Move m(pv[i]);
            os << ' ' << move_to_string(&p[idx], &m);
            pos_move(&p[idx^1], &p[idx], m);
            idx ^= 1;
        }

        std::cout << os.str() << std::endl;
    }
}

void info_print_bestmove(const Info *info, const Position *pos)
{
    std::lock_guard<std::mutex> lk(info->mtx);
    std::cout << "bestmove " << move_to_string(pos, &info->bestMove)
              << " ponder " << move_to_string(pos, &info->ponderMove) << std::endl;
}

Move info_best_move(const Info *info)
{
    std::lock_guard<std::mutex> lk(info->mtx);
    return info->bestMove;
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
