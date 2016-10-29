#pragma once
#include <vector>
#include <string>

namespace tune {

// Training position file is a CSV, with lines like "fen,score"
extern std::vector<std::string> fens;
extern std::vector<double> scores;
void load_file(const std::string& fileName);

}    // namespace tune
