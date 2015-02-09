#pragma once
#include <cstdint>

class PRNG {
	uint64_t a, b, c, d;
public:
	PRNG() { init(); }
	void init(uint64_t seed = 0);
	uint64_t rand();
};
