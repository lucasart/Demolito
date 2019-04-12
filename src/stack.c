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
#include "stack.h"
#include "gen.h"

void stack_clear(Stack *st)
{
    st->idx = 0;
}

void stack_push(Stack *st, uint64_t key)
{
    assert(0 <= st->idx && st->idx < MAX_GAME_PLY);
    st->keys[st->idx++] = key;
}

void stack_pop(Stack *st)
{
    assert(0 < st->idx && st->idx <= MAX_GAME_PLY);
    st->idx--;
}

uint64_t stack_back(const Stack *st)
{
    assert(0 < st->idx && st->idx <= MAX_GAME_PLY);
    return st->keys[st->idx - 1];
}

uint64_t stack_move_key(const Stack *st, int back)
{
    assert(0 < st->idx && st->idx <= MAX_GAME_PLY);
    return st->idx - 1 - back > 0
        ? st->keys[st->idx - 1 - back] ^ st->keys[st->idx - 2 - back]
        : 0;
}

bool stack_repetition(const Stack *st, const Position *pos)
{
    // 50 move rule
    if (pos->rule50 >= 100) {
        // If're not mated here, it's draw
        move_t mList[MAX_MOVES];
        return !pos->checkers || gen_check_escapes(pos, mList, false) != mList;
    }

    // TODO: use 3 repetition past root position
    for (int i = 4; i <= pos->rule50 && i < st->idx; i += 2)
        if (st->keys[st->idx - 1 - i] == st->keys[st->idx - 1])
            return true;

    return false;
}
