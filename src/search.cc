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
#include "tt.h"

namespace search {

thread_local int ThreadId;	// set at thread creation, so each thread can know its unique id

// Per thread data
std::vector<zobrist::History> history;
std::vector<uint64_t> nodeCount;

uint64_t nodes()
{
	uint64_t total = 0;
	for (uint64_t n : nodeCount)
		total += n;
	return total;
}

struct Stack {
	Move m;
	int eval;
};
thread_local Stack ss[MAX_PLY+1];	// search stack, per thread

std::atomic<uint64_t> signal;	// signal: bit #i is set if thread #i should stop
enum Abort {
	ABORT_NEXT,	// current thread aborts the current iteration to be scheduled to the next one
	ABORT_STOP	// current thread aborts the current iteration to stop iterating completely
};
std::mutex mtxSchedule;	// protect thread scheduling decisions

const int Tempo = 16;

template <Phase ph>
int recurse(const Position& pos, int ply, int depth, int alpha, int beta, std::vector<move_t>& pv)
{
	assert(history[ThreadId].back() == pos.key());
	assert(alpha < beta);

	const bool PvNode = beta > alpha + 1;
	const int oldAlpha = alpha;
	int bestScore = -INF;
	move_t bestMove = 0;

	const uint64_t s = signal.load(std::memory_order_relaxed);
	if (s) {
		if (s == STOP)
			throw ABORT_STOP;
		else if (bb::test(s, ThreadId))
			throw ABORT_NEXT;
	}

	std::vector<move_t> childPv;
	if (PvNode) {
		childPv.reserve(MAX_PLY - ply);
		pv[0] = 0;
	}

	if (ply > 0 && history[ThreadId].repetition(pos.rule50()))
		return 0;

	// TT probe
	tt::Entry tte;
	if (tt::read(pos.key(), tte)) {
		tte.score = tt::score_from_tt(tte.score, ply);
		if (tte.depth >= depth && ply >= 1) {
			if (tte.score <= alpha && tte.bound >= tt::EXACT)
				return tte.score;
			else if (tte.score >= beta && tte.bound <= tt::EXACT)
				return tte.score;
			else if (alpha < tte.score && tte.score < beta && tte.bound == tt::EXACT)
				return tte.score;
		}
		if (ph == SEARCH && tte.depth <= 0)
			tte.move = 0;
		ss[ply].eval = tte.eval + Tempo;
	} else {
		tte.move = 0;
		ss[ply].eval = pos.checkers() ? -INF : evaluate(pos) + Tempo;
	}

	nodeCount[ThreadId]++;
	if (ply >= MAX_PLY)
		return ss[ply].eval;

	// QSearch stand pat
	if (ph == QSEARCH && !pos.checkers()) {
		bestScore = ss[ply].eval;
		if (bestScore > alpha) {
			alpha = bestScore;
			if (bestScore >= beta)
				return bestScore;
		}
	}

	// Generate and score moves
	Selector S(pos, ph, tte.move);

	size_t moveCount = 0;
	PinInfo pi(pos);

	// Move loop
	while (!S.done() && alpha < beta) {
		int see;
		ss[ply].m = S.select(pos, see);
		if (!ss[ply].m.pseudo_is_legal(pos, pi))
			continue;
		moveCount++;

		// Prune losing captures in the qsearch
		if (ph == QSEARCH && see < 0 && !pos.checkers())
			continue;

		// Play move
		Position nextPos;
		nextPos.set(pos, ss[ply].m);

		// Prune losing captures in the search, near the leaves
		if (ph == SEARCH && depth <= 2 && see < 0
		&& !PvNode && !pos.checkers() && !nextPos.checkers())
			continue;

		history[ThreadId].push(nextPos.key());

		const int ext = see >= 0 && nextPos.checkers();
		const int nextDepth = depth - 1 + ext;

		// Recursion
		int score;
		if (nextDepth <= 0) {
			// Qsearch recursion (alpha/beta)
			if (depth <= MIN_DEPTH && !pos.checkers())
				score = ss[ply].eval + see;	// guard against QSearch explosion
			else
				score = -recurse<QSEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);
		} else {
			// Search recursion (PVS)
			if (PvNode && moveCount == 1)
				score = -recurse<SEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);
			else {
				score = -recurse<SEARCH>(nextPos, ply + 1, nextDepth, -alpha-1, -alpha, childPv);
				if (PvNode && alpha < score && score < beta)
					score = -recurse<SEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);
			}
		}

		// Undo move
		history[ThreadId].pop();

		// New best score
		if (score > bestScore) {
			bestScore = score;
			// New alpha
			if (score > alpha) {
				alpha = score;
				bestMove = ss[ply].m;
				if (PvNode) {
					pv[0] = ss[ply].m;
					for (int i = 0; i < MAX_PLY - ply; i++)
						if (!(pv[i + 1] = childPv[i]))
							break;
				}
			}
		}
	}

	// No legal move: mated or stalemated
	if ((ph == SEARCH || pos.checkers()) && !moveCount)
		return pos.checkers() ? ply - MATE : 0;

	// TT write
	tte.key = pos.key();
	tte.bound = bestScore <= oldAlpha ? tt::UBOUND : bestScore >= beta ? tt::LBOUND : tt::EXACT;
	tte.score = tt::score_to_tt(bestScore, ply);
	tte.eval = pos.checkers() ? -INF : ss[ply].eval - Tempo;
	tte.depth = depth;
	tte.move = bestMove;
	tt::write(tte);

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

void iterate(const Position& pos, const Limits& lim, uci::Info& ui, std::vector<int>& iteration, int threadId)
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
		ui.update(pos, depth, score, nodes(), pv);
	}

	// Max depth completed by current thread. All threads should stop.
	std::lock_guard<std::mutex> lk(mtxSchedule);
	signal = STOP;
}

void bestmove(const Position& pos, const Limits& lim)
{
	using namespace std::chrono;
	const auto start = high_resolution_clock::now();

	uci::Info ui;

	signal = 0;
	std::vector<int> iteration(lim.threads, 0);
	history.resize(lim.threads);
	nodeCount.resize(lim.threads);

	std::vector<std::thread> threads;
	for (int i = 0; i < lim.threads; i++) {
		// Initialize per-thread data
		history[i] = uci::history;
		nodeCount[i] = 0;

		// Start searching thread
		threads.emplace_back(iterate, std::cref(pos), std::cref(lim), std::ref(ui), std::ref(iteration), i);
	}

	do {
		std::this_thread::sleep_for(milliseconds(5));

		// Check for search termination conditions, but only after depth 1 has been
		// completed, to make sure we do not return an illegal move.
		if (ui.last_depth() >= 1) {
			if (lim.nodes && nodes() >= lim.nodes) {
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

	ui.print_bestmove(pos);
}

}	// namespace search
