/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 Lucas Braesch.
 *
 * Demolito is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Demolito is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include "bitboard.h"
#include "zobrist.h"
#include "position.h"
#include "gen.h"

int main()
{
	bb::init();
	zobrist::init();

	Position pos;
	pos.set_pos("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
	pos.print();

	PinInfo pi(pos);
	uint64_t r = gen::perft<true>(pos, 6);
	std::cout << "perft = " << r << std::endl;
}
