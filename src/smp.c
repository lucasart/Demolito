#include "smp.h"
#include "search.h"

Worker *Workers = NULL;
int WorkersCount;

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
