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
#include <fstream>
#include <iostream>
#include "tune.h"
#include "types.h"

namespace tune {

std::vector<std::string> fens;
std::vector<double> scores;

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

}    // namespace tune
