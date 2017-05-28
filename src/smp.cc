#include "smp.h"
#include "search.h"

thread_local Worker *thisWorker = NULL;
Worker *Workers = NULL;
int WorkersCount = 1;

void smp_init()
{
    Workers = (Worker *)calloc(WorkersCount, sizeof(Worker));  // FIXME: C++ needs cast
    thisWorker = &Workers[0];  // master thread needs pawnHash to call evaluate()
}

void smp_resize(int count)
{
    Workers = (Worker *)realloc(Workers, count * sizeof(Worker));  // FIXME: C++ needs cast
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
