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
#include "zobrist.h"
#include "tt.h"

namespace {

Position pos;
search::Limits lim;
std::thread Timer;

size_t Hash = 1;
int Threads = 1;

void intro()
{
    std::cout << "id name Demolito\nid author lucasart\n" << std::boolalpha
        << "option name UCI_Chess960 type check default " << Chess960 << '\n'
        << "option name Hash type spin default " << Hash << " min 1 max 1048576\n"
        << "option name Threads type spin default " << Threads << " min 1 max 64\n"
        << "uciok" << std::endl;
}

void setoption(std::istringstream& is)
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
        Hash = 1ULL << bb::msb(Hash);    // must be a power of two
        tt::table.resize(Hash * 1024 * (1024 / sizeof(tt::Entry)), 0);
    } if (name == "Threads")
        is >> Threads;
}

void position(std::istringstream& is)
{
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

    Position p[NB_COLOR];
    int idx = 0;
    p[idx].set(fen);
    uci::history.clear();
    uci::history.push(p[idx].key());

    // Parse moves (if any)
    while (is >> token) {
        const Move m(p[idx], token);
        p[idx ^ 1].set(p[idx], m);
        idx ^= 1;
        uci::history.push(p[idx].key());
    }

    pos = p[idx];
}

void go(std::istringstream& is)
{
    std::string token;

    while (is >> token) {
        if (token == "depth")
            is >> lim.depth;
        else if (token == "nodes")
            is >> lim.nodes;
        else if (token == "movetime")
            is >> lim.movetime;
    }

    if (Timer.joinable())
        Timer.join();

    lim.threads = Threads;
    Timer = std::thread(search::bestmove, std::cref(pos), std::cref(lim));
}

void eval()
{
    pos.print();
    std::cout << "score " << uci::format_score(evaluate(pos)) << std::endl;
}

}    // namespace

namespace uci {

zobrist::History history;

void loop()
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
            std::cout << "readyok" << std::endl;
        else if (token == "ucinewgame")
            ;
        else if (token == "position")
            position(is);
        else if (token == "go")
            go(is);
        else if (token == "stop")
            search::signal = STOP;
        else if (token == "eval")
            eval();
        else if (token == "quit")
            break;
        else
            std::cout << "unknown command: " << command << std::endl;
    }

    if (Timer.joinable())
        Timer.join();
}

void Info::update(const Position& pos, int depth, int score, int nodes, std::vector<move_t>& pv)
{
    std::lock_guard<std::mutex> lk(mtx);
    if (depth > lastDepth) {
        lastDepth = depth;
        best = pv[0];
        ponder = pv[1];

        std::ostringstream os;
        os << "info depth " << depth << " score " << format_score(score)
            << " nodes " << nodes << " pv";
        for (int i = 0; pv[i]; i++)
            os << ' ' << Move(pv[i]).to_string(pos);

        std::cout << os.str() << std::endl;
    }
}

void Info::print_bestmove(const Position& pos) const
{
    std::lock_guard<std::mutex> lk(mtx);
    std::cout << "bestmove " << best.to_string(pos)
        << " ponder " << ponder.to_string(pos) << std::endl;
}

std::string format_score(int score)
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

}    // namespace uci
