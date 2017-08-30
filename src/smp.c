#include "smp.h"
#include "search.h"

Worker *Workers = NULL;
int WorkersCount;

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

uint64_t stack_move_key(const Stack *st)
{
    assert(0 < st->idx && st->idx <= MAX_GAME_PLY);
    return st->idx > 1 ? st->keys[st->idx - 1] ^ st->keys[st->idx - 2] : 0;
}

bool stack_repetition(const Stack *st, int rule50)
{
    // 50 move rule
    if (rule50 >= 100)
        return true;

    // TODO: use 3 repetition past root position
    for (int i = 4; i <= rule50 && i < st->idx; i += 2)
        if (st->keys[st->idx - 1 - i] == st->keys[st->idx - 1])
            return true;

    return false;
}

void smp_resize(int count)
{
    Workers = realloc(Workers, count * sizeof(Worker));
    WorkersCount = count;
}

void smp_destroy()
{
    free(Workers);
}

void smp_new_search()
{
    for (int i = 0; i < WorkersCount; i++) {
        memset(Workers[i].history, 0, sizeof(Workers[i].history));
        memset(Workers[i].refutation, 0, sizeof(Workers[i].refutation));
        memset(Workers[i].killers, 0, sizeof(Workers[i].killers));
        Workers[i].stack = rootStack;
        Workers[i].nodes = 0;
        Workers[i].depth = 0;
        Workers[i].id = i;
    }
}

void smp_new_game()
{
    for (int i = 0; i < WorkersCount; i++)
        memset(Workers[i].pawnHash, 0, sizeof(Workers[i].pawnHash));
}

int64_t smp_nodes()
{
    int64_t total = 0;

    for (int i = 0; i < WorkersCount; i++)
        total += Workers[i].nodes;

    return total;
}
