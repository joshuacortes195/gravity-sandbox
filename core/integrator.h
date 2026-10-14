#pragma once

#include "bodies.h"

// moves the whole sim forward by one timestep with leapfrog kick drift kick
// the accelerations have to be current going in, so call compute_accel once after setting up a scene
void step(Bodies& b, float dt, float eps);
