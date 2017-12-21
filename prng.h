#ifndef prng_H
#define prng_H

#include <string>
#include <cstdlib>
#include <random>

std::mt19937* seeded_prng (const std::string& seed);

#endif // prng_H
