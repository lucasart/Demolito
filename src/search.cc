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
#include "zobrist.h"

namespace search {

thread_local int ThreadId;	// set at thread creation, so each thread can know its unique id

std::vector<zobrist::History> history;	// history of zobrist keys, per thread

struct Stack {
	Move m;
	int eval;
};
thread_local Stack ss[MAX_PLY+1];	// search stack, per thread

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
int recurse(const Position& pos, int ply, int depth, int alpha, int beta, std::vector<move_t>& pv)
{
	assert(history[ThreadId].back() == pos.key());
	int bestScore = -INF;
	const bool inCheck = pos.checkers();

	const uint64_t s = signal;
	if (s) {
		if (s == STOP)
			throw ABORT_STOP;
		else if (bb::test(s, ThreadId))
			throw ABORT_NEXT;
	}

	nodeCount++;
	std::vector<move_t> childPv;
	childPv.reserve(MAX_PLY - ply);
	pv[0] = 0;

	if (ply > 0 && history[ThreadId].repetition(pos.rule50()))
		return 0;

        ss[ply].eval = inCheck ? -INF : evaluate(pos);
	if (ply >= MAX_PLY)
		return ss[ply].eval;

	// QSearch stand pat
	if (ph == QSEARCH && !inCheck) {
		bestScore = ss[ply].eval;
		if (bestScore > alpha) {
			alpha = bestScore;
			if (bestScore >= beta)
				return bestScore;
		}
	}

	// Generate and score moves
	Selector S(pos, ph);

	size_t moveCount = 0;
	PinInfo pi(pos);

	// Move loop
	while (!S.done() && alpha < beta) {
		int see;
		ss[ply].m = S.select(pos, see);
		if (!ss[ply].m.pseudo_is_legal(pos, pi))
			continue;
		moveCount++;

		// Prune losing captures in QSearch
		if (ph == QSEARCH && !inCheck && see < 0)
			continue;

		// Play move
		Position nextPos;
		nextPos.set(pos, ss[ply].m);
		history[ThreadId].push(nextPos.key());

		const int nextDepth = depth - 1 /*+ (pos.checkers() != 0)*/;

		// Recursion
		int score;
		if (depth <= MIN_DEPTH && !inCheck)
			score = ss[ply].eval + see;	// guard against QSearch explosion
		else
			score = nextDepth > 0
				? -recurse<SEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, childPv)
				: -recurse<QSEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);

		// Undo move
		history[ThreadId].pop();

		// Update bestScore and alpha
		if (score > bestScore) {
			bestScore = score;
			pv[0] = ss[ply].m;
			for (int i = 0; i < MAX_PLY - ply; i++)
				if (!(pv[i + 1] = childPv[i]))
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

int aspirate(const Position& pos, int depth, std::vector<move_t>& pv, int score)
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

void iterate(const Position& pos, const Limits& lim, uci::Info& ui, int threadId)
{
	ThreadId = threadId;
	std::vector<move_t> pv(MAX_PLY + 1);
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
			history[ThreadId] = uci::history;	// Restore an orderly state
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
	using namespace std::chrono;
	const auto start = high_resolution_clock::now();

	nodeCount = 0;
	uci::Info ui;

	signal = 0;
	iteration.reserve(lim.threads);
	history.reserve(lim.threads);

	std::vector<std::thread> threads;
	for (int i = 0; i < lim.threads; i++) {
		iteration[i] = 0;
		history[i] = uci::history;
		threads.emplace_back(iterate, std::cref(pos), std::cref(lim), std::ref(ui), i);
	}

	do {
		std::this_thread::sleep_for(milliseconds(5));

		// Check for search termination conditions, but only after depth 1 has been
		// completed, to make sure we do not return an illegal move.
		if (ui.last_depth() >= 1) {
			if (lim.nodes && nodeCount >= lim.nodes) {
				std::lock_guard<std::mutex> lk(mtxSchedule);
				signal = STOP;
			} else if (lim.movetime && duration_cast<milliseconds>
			(high_resolution_clock::now() - start).count() >= lim.movetime) {
				std::lock_guard<std::mutex> lk(mtxSchedule);
				signal = STOP;
			}
		}
	} while (signal != STOP);

	for (auto& t : threads)
		t.join();

	ui.print_bestmove();
}

}	// namespace search
