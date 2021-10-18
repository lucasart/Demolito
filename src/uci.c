/*
 * Demolito, a UCI chess engine. Copyright 2015-2020 lucasart.
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
#include "uci.h"
#include "bitboard.h"
#include "eval.h"
#include "gen.h"
#include "htable.h"
#include "position.h"
#include "search.h"
#include "tune.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define uci_printf(...) printf(__VA_ARGS__), fflush(stdout)
#define uci_puts(str) puts(str), fflush(stdout)

static pthread_t Timer = 0;

size_t uciHash = 2;
int uciLevel = 0;
int64_t uciTimeBuffer = 60;
bool uciChess960 = false;

static void uci_format_score(int score, char str[17]) {
    if (is_mate_score(score))
        sprintf(str, "mate %d", score > 0 ? (MATE - score + 1) / 2 : -(score + MATE + 1) / 2);
    else
        sprintf(str, "cp %d", score / 2);
}

static void intro(void) {
    uci_puts("id name Demolito " VERSION "\nid author lucasart");
    uci_printf("option name Contempt type spin default %d min -100 max 100\n", Contempt);
    uci_printf("option name Hash type spin default %zu min 1 max 1048576\n", uciHash);
    uci_puts("option name Ponder type check default false");
    uci_printf("option name Level type spin default %d min 0 max 15\n", uciLevel);
    uci_printf("option name Threads type spin default %zu min 1 max 256\n", WorkersCount);
    uci_printf("option name Time Buffer type spin default %" PRId64 " min 0 max 1000\n",
               uciTimeBuffer);
    uci_printf("option name UCI_Chess960 type check default %s\n", uciChess960 ? "true" : "false");
#ifdef TUNE
    tune_declare();
#endif
    uci_puts("uciok");
}

static void setoption(char **linePos) {
    const char *token = strtok_r(NULL, " \n", linePos);
    char name[32] = "";

    if (strcmp(token, "name"))
        return;

    while ((token = strtok_r(NULL, " \n", linePos)) && strcmp(token, "value"))
        strcat(name, token);

    token = strtok_r(NULL, " \n", linePos);

    if (!strcmp(name, "UCI_Chess960"))
        uciChess960 = !strcmp(token, "true");
    else if (!strcmp(name, "Hash")) {
        uciHash = (size_t)atoll(token);
        uciHash = 1ULL << bb_msb(uciHash); // must be a power of two
        hash_prepare(uciHash);
    } else if (!strcmp(name, "Threads"))
        workers_prepare((size_t)atoll(token));
    else if (!strcmp(name, "Contempt"))
        Contempt = atoi(token);
    else if (!strcmp(name, "Level")) {
        uciLevel = atoi(token);

        if (uciLevel)
            // Switch on Level feature: discard uciHash and impose level based hash size
            hash_prepare(1ULL << max(uciLevel - 9, 0));
        else
            // Swithcing off Level feature: restore hash size to uciHash
            hash_prepare(uciHash);
    } else if (!strcmp(name, "TimeBuffer"))
        uciTimeBuffer = atoi(token);
    else {
#ifdef TUNE
        tune_parse(name, atoi(token));
#endif
    }
}

static void position(char **linePos) {
    Position pos[NB_COLOR];
    int idx = 0;

    const char *token = strtok_r(NULL, " \n", linePos);
    char fen[MAX_FEN] = "";

    if (!strcmp(token, "startpos")) {
        strcpy(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        strtok_r(NULL, " \n", linePos); // consume "moves" token (if present)
    } else if (!strcmp(token, "fen")) {
        while ((token = strtok_r(NULL, " \n", linePos)) && strcmp(token, "moves"))
            strcat(strcat(fen, token), " ");
    } else
        return;

    pos_set(&pos[idx], fen);
    zobrist_clear(&rootStack);
    zobrist_push(&rootStack, pos[idx].key);

    // Parse moves (if any)
    while ((token = strtok_r(NULL, " \n", linePos))) {
        move_t m = pos_string_to_move(&pos[idx], token);
        pos_move(&pos[idx ^ 1], &pos[idx], m);
        idx ^= 1;
        zobrist_push(&rootStack, pos[idx].key);
    }

    rootPos = pos[idx];
}

static void go(char **linePos) {
    lim = (Limits){.depth = MAX_DEPTH};

    if (uciLevel) {
        lim.depth = uciLevel <= 10 ? uciLevel : 2 * uciLevel - 10;
        lim.nodes = 32ULL << uciLevel;
    }

    const char *token;

    while ((token = strtok_r(NULL, " \n", linePos))) {
        if (!strcmp(token, "depth"))
            lim.depth = atoi(strtok_r(NULL, " \n", linePos));
        else if (!strcmp(token, "nodes"))
            lim.nodes = (uint64_t)atoll(strtok_r(NULL, " \n", linePos));
        else if (!strcmp(token, "movetime"))
            lim.movetime = atoll(strtok_r(NULL, " \n", linePos));
        else if (!strcmp(token, "movestogo"))
            lim.movestogo = atoi(strtok_r(NULL, " \n", linePos));
        else if ((rootPos.turn == WHITE && !strcmp(token, "wtime")) ||
                 (rootPos.turn == BLACK && !strcmp(token, "btime")))
            lim.time = atoll(strtok_r(NULL, " \n", linePos));
        else if ((rootPos.turn == WHITE && !strcmp(token, "winc")) ||
                 (rootPos.turn == BLACK && !strcmp(token, "binc")))
            lim.inc = atoll(strtok_r(NULL, " \n", linePos));
        else if (!strcmp(token, "infinite") || !strcmp(token, "ponder"))
            lim.infinite = true;
    }

    if (Timer) {
        pthread_join(Timer, NULL);
        Timer = 0;
    }

    pthread_create(&Timer, NULL, search_posix, NULL);
}

static void eval(void) {
    char str[17];
    uci_format_score(evaluate(&Workers[0], &rootPos), str);
    uci_printf("score %s\n", str);
}

static void perft(char **linePos) {
    const int depth = atoi(strtok_r(NULL, " \n", linePos));
    const char *last = strtok_r(NULL, " \n", linePos);
    uci_printf("%" PRIu64 "\n", gen_perft(&rootPos, depth, !last || strcmp(last, "div")));
}

Info ui;

void uci_loop() {
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
            memset(HashTable, 0, HashCount * sizeof(HashEntry));
            workers_clear();
            hashDate = 0;
#ifdef TUNE
            tune_refresh();
#endif
        } else if (!strcmp(token, "position"))
            position(&linePos);
        else if (!strcmp(token, "go"))
            go(&linePos);
        else if (!strcmp(token, "stop")) {
            lim.infinite = false;
            Stop = true;
        } else if (!strcmp(token, "ponderhit"))
            lim.infinite = false; // switch from pondering to normal search
        else if (!strcmp(token, "d"))
            pos_print(&rootPos);
        else if (!strcmp(token, "eval"))
            eval();
        else if (!strcmp(token, "perft"))
            perft(&linePos);
        else if (!strcmp(token, "quit")) {
            Stop = true;
            break;
        } else if (!strcmp(token, "load"))
            tune_load(strtok_r(NULL, " \n", &linePos));
        else if (!strcmp(token, "free"))
            tune_free();
        else if (!strcmp(token, "list"))
            tune_param_list();
        else if (!strcmp(token, "get"))
            tune_param_get(strtok_r(NULL, " \n", &linePos));
        else if (!strcmp(token, "set"))
            tune_param_set(strtok_r(NULL, " \n", &linePos), strtok_r(NULL, " \n", &linePos));
        else if (!strcmp(token, "linereg"))
            tune_linereg();
        else if (!strcmp(token, "logitreg"))
            tune_logitreg(strtok_r(NULL, " \n", &linePos));
        else if (!strcmp(token, "fit"))
            tune_param_fit(strtok_r(NULL, " \n", &linePos), atoi(strtok_r(NULL, " \n", &linePos)));
        else
            uci_printf("unknown command: %s\n", line);
    }

    if (Timer) {
        pthread_join(Timer, NULL);
        Timer = 0;
    }
}

void info_create(Info *info) {
    info->lastDepth = 0;
    info->variability = 0.5;
    info->best = info->ponder = 0;
    info->start = system_msec();
    mtx_init(&info->mtx, mtx_plain);
}

void info_destroy(Info *info) { mtx_destroy(&info->mtx); }

void info_update(Info *info, int depth, int score, uint64_t nodes, move_t pv[], bool partial) {
    mtx_lock(&info->mtx);

    if (depth > info->lastDepth) {
        // Print info line all the way to the "pv" token
        char str[17];
        uci_format_score(score, str);
        const int64_t elapsed = system_msec() - info->start;
        uci_printf("info depth %d score %s time %" PRId64 " nodes %" PRIu64 " nps %" PRIu64
                   " hashfull %d pv",
                   depth, str, elapsed, nodes, 1000 * nodes / (uint64_t)max(elapsed, 1),
                   hash_permille());

        // Pring the moves. Because of e1g1 notation when Chess960 = false, we need to play the PV
        // to print it correctly. This is a design flaw of the UCI protocol, which should have
        // encoded castling as e1h1 regardless of Chess960 allowing coherent treatement.
        Position pos[NB_COLOR];
        int idx = 0;
        pos[idx] = rootPos;

        for (int i = 0; pv[i]; i++) {
            pos_move_to_string(&pos[idx], pv[i], str);
            uci_printf(" %s", str);
            pos_move(&pos[idx ^ 1], &pos[idx], pv[i]);
            idx ^= 1;
        }

        uci_puts("");

        // Update variability depending on whether the bestmove has changed or is confirmed
        // - changed: increase variability (rescale for %age of partial updates = f(threads))
        // - confirmed: reduce variability (discard partial)
        info->variability +=
            info->best != pv[0] ? 0.6 * pow(WorkersCount, -0.08) : -0.24 * !partial;

        if (!partial)
            info->lastDepth = depth;

        info->best = pv[0];
        info->ponder = pv[1]; // May be zero (not a bug, inevitable consequence of partial updates)
    }

    mtx_unlock(&info->mtx);
}

void info_print_bestmove(Info *info) {
    mtx_lock(&info->mtx);

    char best[6];
    pos_move_to_string(&rootPos, info->best, best);

    if (info->ponder) {
        char ponder[6];
        Position nextPos;
        pos_move(&nextPos, &rootPos, info->best);
        pos_move_to_string(&nextPos, info->ponder, ponder);
        uci_printf("bestmove %s ponder %s\n", best, ponder);
    } else
        uci_printf("bestmove %s\n", best);

    mtx_unlock(&info->mtx);
}

move_t info_best(Info *info) {
    mtx_lock(&info->mtx);
    const move_t best = info->best;
    mtx_unlock(&info->mtx);

    return best;
}

int info_last_depth(Info *info) {
    mtx_lock(&info->mtx);
    const int lastDepth = info->lastDepth;
    mtx_unlock(&info->mtx);

    return lastDepth;
}

double info_variability(Info *info) {
    mtx_lock(&info->mtx);
    const double variability = info->variability;
    mtx_unlock(&info->mtx);

    return variability;
}
