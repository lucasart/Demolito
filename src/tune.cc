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
#include <cstring>    // std::memset()
#include <fstream>
#include <iostream>
#include <thread>
#include <cmath>
#include "eval.h"
#include "search.h"
#include "tt.h"
#include "tune.h"
#include "types.h"

namespace {

std::vector<std::string> fens;
std::vector<double> scores;
std::vector<double> qsearches;

void idle_loop(int threadId)
{
    search::ThreadId = threadId;
    std::memset(PawnHash, 0, sizeof(PawnHash));

    Position pos;
    std::vector<move_t> pv(MAX_PLY + 1);

    for (size_t i = threadId; i < fens.size(); i += search::Threads) {
        pos.set(fens[i]);
        search::gameStack[threadId].clear();
        search::gameStack[threadId].push(pos.key());

        qsearches[i] = search::recurse<true>(pos, 0, 0, -INF, INF, pv);
    }
}

}    // namespace

namespace tune {

void load_file(const std::string& fileName)
{
    Clock c;
    c.reset();

    std::ifstream f(fileName);
    std::string s;

    while (std::getline(f, s)) {
        const auto i = s.find(',');

        if (i != std::string::npos) {
            fens.push_back(s.substr(0, i));
            scores.push_back(std::stod(s.substr(i + 1)));
        }
    }

    std::cout << "loaded " << fens.size() << " training positions in " << c.elapsed()
              << "ms" << std::endl;
}

void qsearch()
{
    Clock c;
    c.reset();

    qsearches.resize(fens.size());

    tt::clear();
    search::signal = 0;
    search::gameStack.resize(search::Threads);
    search::nodeCount.resize(search::Threads);
    std::vector<std::thread> threads;

    for (int i = 0; i < search::Threads; i++) {
        search::nodeCount[i] = 0;
        threads.emplace_back(idle_loop, i);
    }

    for (auto& t : threads)
        t.join();

    std::cout << "qsearched " << fens.size() << " positions in " << c.elapsed()
              << "ms" << std::endl;
}

double error(double lambda)
{
    double sum = 0;

    for (size_t i = 0; i < fens.size(); i++) {
        const double logistic = 1 / (1.0 + std::exp(-lambda * qsearches[i]));
        sum += (scores[i] - logistic) * (scores[i] - logistic);
    }

    return sum / fens.size();
}

void logistic()
{
    double lambda0 = 0.0013;
    double h = lambda0 / 100;

    const double e0 = error(lambda0);
    const double ep = error(lambda0 + h);
    const double em = error(lambda0 - h);
    const double e1 = (ep - em) / (2 * h);
    const double e2 = (ep - 2 * e0 + em) / (h * h);
    const double lambda = (e2 * lambda0 - e1) / e2;

    std::cout << "lambda0 = " << lambda0 << ", error(lambda0) = " << e0 << '\n'
              << "lambda = " << lambda << ", error(lambda) = " << error(lambda) << std::endl;
}

}    // namespace tune
