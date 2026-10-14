#pragma once

#include "bodies.h"

// two bodies orbiting their shared center in the xy plane
// speed_scale 1 is a circular orbit, lower starts them at the far point of a stretched orbit
Bodies two_body_orbit(float m1, float m2, float separation, float speed_scale);

// n equal bodies at rest spread evenly through a ball of radius 1, total mass 1
// the same seed always gives the same scene
Bodies uniform_ball(int n, unsigned seed);

// n equal bodies at rest packed into 8 small clumps inside the unit ball, total mass 1
Bodies clustered_ball(int n, unsigned seed);
