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
