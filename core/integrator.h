#pragma once

#include "bodies.h"
#include "octree.h"

// moves the whole sim forward by one timestep with leapfrog kick drift kick
// the accelerations have to be current going in, so call compute_accel once after setting up a scene
void step(Bodies& b, float dt, float eps);

// the same step with the forces coming from the tree, which is what makes big scenes affordable
void step(Bodies& b, Octree& tree, float dt, float eps, float theta);
