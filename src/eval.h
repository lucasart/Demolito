#pragma once
#include "types.h"
#include "smp.h"

void eval_init();
int evaluate(Worker *worker, const Position *pos);
