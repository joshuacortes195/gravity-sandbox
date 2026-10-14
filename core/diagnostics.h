#pragma once

#include <array>
#include <cstddef>

#include "bodies.h"

// energy stored in how close the bodies are to each other, always negative
double potential_energy(const Bodies& b, float eps);

// the same sum but only for the pairs whose first body is in rows [begin, end)
// lets the page spread one big sum over several frames, the chunks add up to potential_energy
double potential_rows(const Bodies& b, float eps, std::size_t begin, std::size_t end);

// energy of motion, half m v squared added over every body
double kinetic_energy(const Bodies& b);

// the number that should stay flat while the sim runs
double total_energy(const Bodies& b, float eps);

// mass times velocity added over every body, as x y z
std::array<double, 3> momentum(const Bodies& b);

// how much the whole system is spinning about the origin, as x y z
std::array<double, 3> angular_momentum(const Bodies& b);
