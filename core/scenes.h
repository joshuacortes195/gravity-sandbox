#pragma once

#include "bodies.h"

// two bodies orbiting their shared center in the xy plane
// speed_scale 1 is a circular orbit, lower starts them at the far point of a stretched orbit
Bodies two_body_orbit(float m1, float m2, float separation, float speed_scale);
