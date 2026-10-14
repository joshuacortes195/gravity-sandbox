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

// the figure-eight three body orbit: three equal masses chasing each other round one curve
// published starting numbers (chenciner and montgomery 2000, computed by simo), one period is about 6.3259
Bodies figure_eight();

// a sun with five planets on circular orbits in the xy plane, the last one jupiter sized
Bodies solar_system();

// two disk galaxies of n stars each, starting r_a apart on an orbit that brings their centers to r_p
// the second disk is tilted out of the first one's plane
Bodies galaxy_collision(int n, float r_a, float r_p, float eps, unsigned seed);

// a black hole and a ball of n stars that holds itself up with random motion,
// starting r_a from the hole on an orbit that brings it to r_p, close enough for tides to pull it apart
Bodies black_hole_cluster(int n, float r_a, float r_p, unsigned seed);
