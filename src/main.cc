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
#include <cstdlib>
#include "bitboard.h"
#include "zobrist.h"
#include "test.h"

int main(int argc, char **argv)
{
	bb::init();
	zobrist::init();

	if (argc >= 2) {
		const std::string cmd(argv[1]);
		if (cmd == "see")
			std::cout << "\nSEE: " << (test::see(true) ? "ok" : "failed") << std::endl;
		else if (cmd == "perft" && argc >= 3) {
			const int depth = std::atoi(argv[2]);
			std::cout << "total = " << test::bench(depth) << std::endl;
		}
	}
}
