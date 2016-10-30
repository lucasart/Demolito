#pragma once
#include <vector>
#include <string>

namespace tune {

void load_file(const std::string& fileName);
void run();
double error(double lambda);

}    // namespace tune
