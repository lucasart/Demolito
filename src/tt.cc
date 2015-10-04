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
#include "tt.h"

namespace tt {

std::vector<Packed> table(1024 * 1024 / sizeof(Packed));	// default=1MB (min Hash)

UnPacked Packed::unpack() const
{
	UnPacked u;
	u.key = word[0];

	u.score = word[1] & 0xffff;
	u.em = (word[1] >> 16) & 0xffff;
	u.eval = (word[1] >> 32) & 0xffff;
	u.depth = (word[1] >> 48) & 0xff;
	u.bound = (word[1] >> 56) & 0xff;

	return u;
}

Packed UnPacked::pack() const
{
	// TODO: assert bounds checking
	Packed p;
	p.word[0] = key;

	p.word[1] = uint64_t(score);
	p.word[1] |= uint64_t(em) << 16;
	p.word[1] |= uint64_t(eval) << 32;
	p.word[1] |= uint64_t(depth) << 48;
	p.word[1] |= uint64_t(bound) << 56;

	return p;
}

}	// namespace tt
