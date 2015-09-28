/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 Lucas Braesch.
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
#include <thread>
#include <vector>
#include <chrono>
#include "search.h"
#include "sort.h"
#include "eval.h"
#include "uci.h"

namespace search {

using namespace std::chrono;

// This is thread local, set at thread creation, so each thread can know its unique id
thread_local int ThreadId;

std::vector<int> iteration;	// iteration[i] is the iteration on which thread #i is working
std::atomic<uint64_t> signal;	// signal: bit #i is set if thread #i should stop
enum Abort {
	ABORT_NEXT,	// current thread aborts the current iteration to be scheduled to the next one
	ABORT_STOP	// current thread aborts the current iteration to stop iterating completely
};
std::mutex mtxSchedule;	// protect thread scheduling decisions

// Global node counter
std::atomic<uint64_t> nodeCount;

template <Phase ph>
int recurse(const Position& pos, int ply, int depth, int alpha, int beta, Move *pv)
{
	int bestScore = -INF;
	const bool inCheck = pos.checkers();

	nodeCount++;
	Move subtreePv[MAX_PLY - ply];
	pv[0].clear();

	const uint64_t s = signal;
	if (s) {
		if (s == STOP)
			throw ABORT_STOP;
		else if (bb::test(s, ThreadId))
			throw ABORT_NEXT;
	}

	const int eval = inCheck ? -INF : evaluate(pos);

	if (ply >= MAX_PLY)
		return eval;

	// QSearch stand pat
	if (ph == QSEARCH && !inCheck) {
		bestScore = eval;
		if (bestScore > alpha) {
			alpha = bestScore;
			if (bestScore >= beta)
				return bestScore;
		}
	}

	// Generate and score moves
	Selector S(pos, ph);

	Move m;
	int see;
	size_t moveCount = 0;
	Position nextPos;
	PinInfo pi(pos);

	// Move loop
	while (!S.done() && alpha < beta) {
		m = S.select(pos, see);
		if (!m.pseudo_is_legal(pos, pi))
			continue;
		moveCount++;

		// Prune losing captures in QSearch
		if (ph == QSEARCH && !inCheck && see < 0)
			continue;

		// Play move
		nextPos.set(pos, m);

		const int nextDepth = depth - 1 /*+ (pos.checkers() != 0)*/;

		// Recursion
		int score;
		if (depth <= MIN_DEPTH && !inCheck)
			score = eval + see;	// guard against QSearch explosion
		else
			score = nextDepth > 0
				? -recurse<SEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, subtreePv)
				: -recurse<QSEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, subtreePv);

		// Update bestScore and alpha
		if (score > bestScore) {
			bestScore = score;
			pv[0] = m;
			for (int i = 0; i < MAX_PLY - ply; i++)
				if ((pv[i + 1] = subtreePv[i]).null())
					break;
			if (score > alpha)
				alpha = score;
		}
	}

	// No legal move: mated or stalemated
	if (ph == SEARCH && !moveCount)
		return inCheck ? ply - MATE : 0;

	return bestScore;
}

int aspirate(const Position& pos, int depth, Move *pv, int score)
{
	int delta = 32;
	int alpha = score - delta;
	int beta = score + delta;

	for ( ; ; delta += delta) {
		score = recurse<SEARCH>(pos, 0, depth, alpha, beta, pv);
		if (score <= alpha) {
			beta = (alpha + beta) / 2;
			alpha -= delta;
		} else if (score >= beta) {
			alpha = (alpha + beta) / 2;
			beta += delta;
		} else
			return score;
	}
}

void iterate(const Position& pos, const Limits& lim, UCI::Info& ui, int threadId)
{
	ThreadId = threadId;
	Move pv[MAX_PLY + 1];
	int score;

	for (int depth = 1; depth <= lim.depth; depth++) {
		{
			std::lock_guard<std::mutex> lk(mtxSchedule);

			// If enough of the other threads are searching this iteration, move on to
			// the next one. Special cases where this does not apply:
			// depth == 1: we want all threads to finish depth == 1 asap.
			// depth == lim.depth: there is no next iteration.
			if (lim.threads >= 2 && depth >= 2 && depth < lim.depth) {
				int cnt = 0;
				for (int i = 0; i < lim.threads; i++)
					cnt += i != ThreadId && iteration[i] == depth;
				if (cnt >= lim.threads / 2 && depth < lim.depth)
					continue;
			}

			iteration[ThreadId] = depth;
			if (signal == STOP)
				return;
			signal &= ~(1ULL << ThreadId);
		}

		try {
			score = depth <= 1
				? recurse<SEARCH>(pos, 0, depth, -INF, +INF, pv)
				: aspirate(pos, depth, pv, score);

			// Iteration was completed normally. Now we need to see who is working on
			// obsolete iterations, and raise the appropriate signal, to make them move
			// on to the next iteration.
			{
				std::lock_guard<std::mutex> lk(mtxSchedule);
				assert(!bb::test(signal, ThreadId));
				uint64_t s = 0;
				for (int i = 0; i < lim.threads; i++)
					if (i != ThreadId && iteration[i] == depth)
						bb::set(s, i);
				signal |= s;
			}
		} catch (const Abort e) {
			assert(bb::test(signal, ThreadId));
			if (e == ABORT_STOP)
				break;
			else {
				assert(e == ABORT_NEXT);
				continue;
			}
		}
		ui.update(depth, score, nodeCount, pv);
	}

	// Max depth completed by current thread. All threads should stop.
	std::lock_guard<std::mutex> lk(mtxSchedule);
	signal = STOP;
}

void bestmove(const Position& pos, const Limits& lim)
{
	const auto start = high_resolution_clock::now();

	nodeCount = 0;
	UCI::Info ui;

	signal = 0;
	iteration.resize(lim.threads, 0);

	std::vector<std::thread> threads;
	for (int i = 0; i < lim.threads; i++)
		threads.emplace_back(iterate, std::cref(pos), std::cref(lim), std::ref(ui), i);

	do {
		std::this_thread::sleep_for(milliseconds(5));

		// Check for search termination conditions
		if (lim.nodes && nodeCount >= lim.nodes) {
			std::lock_guard<std::mutex> lk(mtxSchedule);
			signal = STOP;
		} else if (lim.movetime && duration_cast<milliseconds>
		(high_resolution_clock::now() - start).count() >= lim.movetime) {
			std::lock_guard<std::mutex> lk(mtxSchedule);
			signal = STOP;
		}
	} while (signal != STOP);

	for (auto& t : threads)
		t.join();

	ui.print_bestmove();
}

}	// namespace search
