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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitboard.h"
#include "eval.h"
#include "gen.h"
#include "htable.h"
#include "move.h"
#include "position.h"
#include "search.h"
#include "uci.h"

#define uci_printf(...) printf(__VA_ARGS__), fflush(stdout)
#define uci_puts(str) puts(str), fflush(stdout)

static pthread_t Timer = 0;

uint64_t uciHash = 2;
int64_t uciTimeBuffer = 60;
bool uciChess960 = false;

static void uci_format_score(int score, char str[17])
{
    if (is_mate_score(score))
        sprintf(str, "mate %d", score > 0 ? (MATE - score + 1) / 2 : -(score + MATE + 1) / 2);
    else
        sprintf(str, "cp %d", score * 100 / EP);
}

static void intro()
{
    uci_puts("id name Demolito " VERSION "\nid author lucasart");
    uci_printf("option name Contempt type spin default %d min -100 max 100\n", Contempt);
    uci_printf("option name Hash type spin default %" PRIu64 " min 1 max 1048576\n", uciHash);
    uci_puts("option name Ponder type check default false");
    uci_printf("option name Threads type spin default %d min 1 max 63\n", WorkersCount);
    uci_printf("option name Time Buffer type spin default %" PRId64 " min 0 max 1000\n", uciTimeBuffer);
    uci_printf("option name UCI_Chess960 type check default %s\n", uciChess960 ? "true" : "false");
    uci_puts("uciok");
}

static void setoption(char **linePos)
{
    const char *token = strtok_r(NULL, " \n", linePos);
    char name[32] = "";

    if (strcmp(token, "name"))
        return;

    while ((token = strtok_r(NULL, " \n", linePos)) && strcmp(token,"value"))
        strcat(name, token);

    if (!strcmp(name, "UCI_Chess960"))
        uciChess960 = !strcmp(strtok_r(NULL, " \n", linePos), "true");
    else if (!strcmp(name, "Hash")) {
        uciHash = atoi(strtok_r(NULL, " \n", linePos));
        uciHash = 1ULL << bb_msb(uciHash);  // must be a power of two
        hash_prepare(uciHash);
    } else if (!strcmp(name, "Threads"))
        smp_prepare(atoi(strtok_r(NULL, " \n", linePos)));
    else if (!strcmp(name, "Contempt"))
        Contempt = atoi(strtok_r(NULL, " \n", linePos));
    else if (!strcmp(name, "TimeBuffer"))
        uciTimeBuffer = atoi(strtok_r(NULL, " \n", linePos));
}

static void position(char **linePos)
{
    Position pos[NB_COLOR];
    int idx = 0;

    const char *token = strtok_r(NULL, " \n", linePos);
    char fen[MAX_FEN] = "";

    if (!strcmp(token, "startpos")) {
        strcpy(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        strtok_r(NULL, " \n", linePos);  // consume "moves" token (if present)
    } else if (!strcmp(token, "fen")) {
        while ((token = strtok_r(NULL, " \n", linePos)) && strcmp(token, "moves"))
            strcat(strcat(fen, token), " ");
    } else
        return;

    pos_set(&pos[idx], fen);
    stack_clear(&rootStack);
    stack_push(&rootStack, pos[idx].key);

    // Parse moves (if any)
    while ((token = strtok_r(NULL, " \n", linePos))) {
        move_t m = string_to_move(&pos[idx], token);
        pos_move(&pos[idx^1], &pos[idx], m);
        idx ^= 1;
        stack_push(&rootStack, pos[idx].key);
    }

    rootPos = pos[idx];
}

static void go(char **linePos)
{
    memset(&lim, 0, sizeof(lim));
    lim.depth = MAX_DEPTH;

    const char *token;

    while ((token = strtok_r(NULL, " \n", linePos))) {
        if (!strcmp(token, "depth"))
            lim.depth = atoi(strtok_r(NULL, " \n", linePos));
        else if (!strcmp(token, "nodes"))
            lim.nodes = atoll(strtok_r(NULL, " \n", linePos));
        else if (!strcmp(token, "movetime"))
            lim.movetime = atoll(strtok_r(NULL, " \n", linePos));
        else if (!strcmp(token, "movestogo"))
            lim.movestogo = atoi(strtok_r(NULL, " \n", linePos));
        else if ((rootPos.turn == WHITE && !strcmp(token, "wtime"))
                || (rootPos.turn == BLACK && !strcmp(token, "btime")))
            lim.time = atoll(strtok_r(NULL, " \n", linePos));
        else if ((rootPos.turn == WHITE && !strcmp(token, "winc"))
                || (rootPos.turn == BLACK && !strcmp(token, "binc")))
            lim.inc = atoll(strtok_r(NULL, " \n", linePos));
        else if (!strcmp(token, "infinite") || !strcmp(token, "ponder"))
            lim.infinite = true;
    }

    if (Timer) {
        pthread_join(Timer, NULL);
        Timer = 0;
    }

    pthread_create(&Timer, NULL, (void*(*)(void*))search_go, NULL);
}

static void eval()
{
    char str[17];
    uci_format_score(evaluate(&Workers[0], &rootPos), str);
    uci_printf("score %s\n", str);
}

static void perft(char **linePos)
{
    const int depth = atoi(strtok_r(NULL, " \n", linePos));
    const char *last = strtok_r(NULL, " \n", linePos);
    uci_printf("%" PRIu64 "\n", gen_perft(&rootPos, depth, !last || strcmp(last, "div")));
}

Info ui;

void uci_loop()
{
    char line[8192], *linePos;

    while (fgets(line, 8192, stdin)) {
        const char *token = strtok_r(line, " \n", &linePos);

        if (!strcmp(token, "uci"))
            intro();
        else if (!strcmp(token, "setoption"))
            setoption(&linePos);
        else if (!strcmp(token, "isready"))
            uci_puts("readyok");
        else if (!strcmp(token, "ucinewgame")) {
            memset(HashTable, 0, uciHash << 20);
            smp_clear();
            hashDate = 0;
        } else if (!strcmp(token, "position"))
            position(&linePos);
        else if (!strcmp(token, "go"))
            go(&linePos);
        else if (!strcmp(token, "stop")) {
            lim.infinite = false;
            Signal = STOP;
        } else if (!strcmp(token, "ponderhit"))
            lim.infinite = false;  // switch from pondering to normal search
        else if (!strcmp(token, "d"))
            pos_print(&rootPos);
        else if (!strcmp(token, "eval"))
            eval();
        else if (!strcmp(token, "perft"))
            perft(&linePos);
        else if (!strcmp(token, "quit")) {
            Signal = STOP;
            break;
        } else
            uci_printf("unknown command: %s\n", line);
    }

    if (Timer) {
        pthread_join(Timer, NULL);
        Timer = 0;
    }
}

void info_create(Info *info)
{
    info->lastDepth = 0;
    info->variability = 0.5;
    info->best = info->ponder = 0;
    info->start = system_msec();
    mtx_init(&info->mtx, mtx_plain);
}

void info_destroy(Info *info)
{
    mtx_destroy(&info->mtx);
}

void info_update(Info *info, int depth, int score, int64_t nodes, move_t pv[], bool partial)
{
    mtx_lock(&info->mtx);

    if (depth > info->lastDepth) {
        // Print info line all the way to the "pv" token
        char str[17];
        uci_format_score(score, str);
        uci_printf("info depth %d score %s time %" PRId64 " nodes %" PRId64 " hashfull %d pv",
            depth, str, system_msec() - info->start, nodes, hash_permille());

        // Pring the moves. Because of e1g1 notation when Chess960 = false, we need to play the PV
        // to print it correctly. This is a design flaw of the UCI protocol, which should have
        // encoded castling as e1h1 regardless of Chess960 allowing coherent treatement.
        Position pos[NB_COLOR];
        int idx = 0;
        pos[idx] = rootPos;

        for (int i = 0; pv[i]; i++) {
            move_to_string(&pos[idx], pv[i], str);
            uci_printf(" %s", str);
            pos_move(&pos[idx ^ 1], &pos[idx], pv[i]);
            idx ^= 1;
        }

        uci_puts("");

        // Compensate for relative frequency of partial updates
        const double smpRescaling[] = {1.0, pow(WorkersCount, -0.4)};

        info->variability += info->best != pv[0]
            ? 0.6 * smpRescaling[partial]  // best move changed: increase variability
            : -0.24 * smpRescaling[partial];  // best move confirmed: lower variability

        if (!partial)
            info->lastDepth = depth;

        info->best = pv[0];
        info->ponder = pv[1];  // May be zero (not a bug, inevitable consequence of partial updates)
    }

    mtx_unlock(&info->mtx);
}

void info_print_bestmove(Info *info)
{
    mtx_lock(&info->mtx);

    char best[6];
    move_to_string(&rootPos, info->best, best);

    if (info->ponder) {
        char ponder[6];
        Position nextPos;
        pos_move(&nextPos, &rootPos, info->best);
        move_to_string(&nextPos, info->ponder, ponder);
        uci_printf("bestmove %s ponder %s\n", best, ponder);
    } else
        uci_printf("bestmove %s\n", best);

    mtx_unlock(&info->mtx);
}

move_t info_best(Info *info)
{
    mtx_lock(&info->mtx);
    const move_t best = info->best;
    mtx_unlock(&info->mtx);

    return best;
}

int info_last_depth(Info *info)
{
    mtx_lock(&info->mtx);
    const int lastDepth = info->lastDepth;
    mtx_unlock(&info->mtx);

    return lastDepth;
}

double info_variability(Info *info)
{
    mtx_lock(&info->mtx);
    const double variability = info->variability;
    mtx_unlock(&info->mtx);

    return variability;
}
