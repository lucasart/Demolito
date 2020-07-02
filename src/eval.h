#pragma once
#include "position.h"
#include "workers.h"

void eval_init(void);
int evaluate(Worker *worker, const Position *pos);
