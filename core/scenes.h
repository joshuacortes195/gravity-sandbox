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

// a heavy body at the origin with n light stars on circular orbits in a flat disk around it
// stars sit between a tenth of the radius and the radius, eps should match the sim's softening
Bodies disk_galaxy(int n, float center_mass, float disk_mass, float radius, float eps, unsigned seed);
