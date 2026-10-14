#pragma once

#include <vector>

#include "bodies.h"

// one cube of space in the tree
struct Node {
  float cx, cy, cz;  // center of mass of everything inside the cube
  float mass;        // total mass inside the cube
  float mx, my, mz;  // middle of the cube itself
  float size;        // width of the cube
  int child;         // index of the first of its 8 child cubes, -1 if it was never cut up
  int body;          // first body sitting in this cube when it is a leaf, -1 if none
};

// the whole tree, node 0 is the root cube that covers every body
// kept around between steps so its memory is reused instead of allocated again
struct Octree {
  // how many times a cube can be cut, past this bodies just share a leaf
  static constexpr int max_depth = 24;

  std::vector<Node> nodes;
  std::vector<int> next;  // next body sharing the same leaf, -1 at the end of the chain

  // throws the old tree away and sorts every body into a fresh one
  void build(const Bodies& b);
};

// same job as compute_accel but through the tree: rebuilds it, then walks it once per body
// theta 0 gives the brute force answer, a bigger theta is faster and rougher
void compute_accel_tree(Bodies& b, Octree& tree, float eps, float theta);
