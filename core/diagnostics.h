#pragma once

#include <array>

#include "bodies.h"

// energy stored in how close the bodies are to each other, always negative
double potential_energy(const Bodies& b, float eps);

// energy of motion, half m v squared added over every body
double kinetic_energy(const Bodies& b);

// the number that should stay flat while the sim runs
double total_energy(const Bodies& b, float eps);

// mass times velocity added over every body, as x y z
std::array<double, 3> momentum(const Bodies& b);

// how much the whole system is spinning about the origin, as x y z
std::array<double, 3> angular_momentum(const Bodies& b);
