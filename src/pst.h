#pragma once
#include "types.h"

namespace pst {

extern eval_t table[NB_COLOR][NB_PIECE][NB_SQUARE];

void init(int verbosity = 0);

}	// namespace pst
