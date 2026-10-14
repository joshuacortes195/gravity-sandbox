#pragma once

#include "bodies.h"

// fills in the acceleration of every body from the pull of all the others
void compute_accel(Bodies& b, float eps);
