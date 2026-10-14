#include <algorithm>
#include <cmath>
#include <string>

#include "doctest.h"
#include "bodies.h"
#include "diagnostics.h"
#include "gravity.h"
#include "integrator.h"
#include "octree.h"
#include "scenes.h"
#include "threads.h"

// the many body scenes are what every tree test and the benchmark run on
TEST_CASE("random scenes have the right size, total mass and stay inside the unit ball") {
  for (bool clumpy : {false, true}) {
    const Bodies b = clumpy ? clustered_ball(500, 7) : uniform_ball(500, 7);
    REQUIRE(b.size() == 500);

    double mass = 0, farthest = 0;
    for (std::size_t i = 0; i < b.size(); ++i) {
      mass += b.m[i];
      farthest = std::max(farthest, std::sqrt(double(b.x[i]) * b.x[i] + double(b.y[i]) * b.y[i] + double(b.z[i]) * b.z[i]));
    }
    CHECK(mass == doctest::Approx(1.0));
    CHECK(farthest <= 1.0);
  }
}

// a fixed seed has to give the same scene every time or the benchmark numbers mean nothing
TEST_CASE("the same seed gives the same scene and a different seed does not") {
  const Bodies a = uniform_ball(50, 3), same = uniform_ball(50, 3), other = uniform_ball(50, 4);
  CHECK(a.x == same.x);
  CHECK(a.x != other.x);
}

// clumps should pack bodies much closer to their neighbors than an even spread does
TEST_CASE("the clustered scene is tighter than the uniform one") {
  // average distance from each body to its nearest neighbor
  auto nearest = [](const Bodies& b) {
    double total = 0;
    for (std::size_t i = 0; i < b.size(); ++i) {
      double best = 1e30;
      for (std::size_t j = 0; j < b.size(); ++j) {
        if (i == j) continue;
        const double dx = b.x[j] - b.x[i], dy = b.y[j] - b.y[i], dz = b.z[j] - b.z[i];
        best = std::min(best, dx * dx + dy * dy + dz * dz);
      }
      total += std::sqrt(best);
    }
    return total / b.size();
  };
  CHECK(nearest(clustered_ball(500, 7)) < 0.25 * nearest(uniform_ball(500, 7)));
}

// the root cube covers everything, so it has to carry the whole system's mass and balance point
TEST_CASE("the root node holds the total mass and center of mass") {
  for (bool clumpy : {false, true}) {
    Bodies b = clumpy ? clustered_ball(1000, 11) : uniform_ball(1000, 11);
    // uneven masses so a plain average of positions would give the wrong answer
    for (std::size_t i = 0; i < b.size(); ++i) b.m[i] *= 1.0f + (i % 5);

    // the same totals added up straight from the bodies in 64 bit
    double mass = 0, mx = 0, my = 0, mz = 0;
    for (std::size_t i = 0; i < b.size(); ++i) {
      mass += b.m[i];
      mx += double(b.m[i]) * b.x[i];
      my += double(b.m[i]) * b.y[i];
      mz += double(b.m[i]) * b.z[i];
    }

    Octree tree;
    tree.build(b);
    const Node& root = tree.nodes[0];
    CHECK(root.mass == doctest::Approx(mass).epsilon(1e-5));
    CHECK(std::abs(root.cx - mx / mass) < 1e-5);
    CHECK(std::abs(root.cy - my / mass) < 1e-5);
    CHECK(std::abs(root.cz - mz / mass) < 1e-5);
  }
}

// bodies on the exact same point can never be split apart, the depth limit has to stop the cutting
TEST_CASE("bodies stacked on one point do not split the tree forever") {
  Bodies b;
  for (int i = 0; i < 5; ++i) b.add(0.3f, -0.2f, 0.7f, 0, 0, 0, 2.0f);
  b.add(-1, -1, -1, 0, 0, 0, 1.0f);

  Octree tree;
  tree.build(b);
  CHECK(tree.nodes[0].mass == doctest::Approx(11.0));
  // one split per level at most, so the node count stays small
  CHECK(tree.nodes.size() <= 1 + 8 * Octree::max_depth);
}

// an empty scene and a single body are the smallest trees there are
TEST_CASE("a tree of zero or one bodies builds") {
  Bodies b;
  Octree tree;
  tree.build(b);
  CHECK(tree.nodes[0].mass == 0.0f);

  b.add(1, 2, 3, 0, 0, 0, 4);
  tree.build(b);
  CHECK(tree.nodes.size() == 1);
  CHECK(tree.nodes[0].mass == 4.0f);
  CHECK(tree.nodes[0].cx == 1.0f);
}

// how far the tree's answer is from brute force, as a fraction of each body's true acceleration
struct TreeError {
  double mean, worst;
};
static TreeError tree_error(Bodies b, float eps, float theta) {
  Bodies exact = b;
  compute_accel(exact, eps);
  Octree tree;
  compute_accel_tree(b, tree, eps, theta);

  TreeError e{0, 0};
  for (std::size_t i = 0; i < b.size(); ++i) {
    const double dx = b.ax[i] - exact.ax[i], dy = b.ay[i] - exact.ay[i], dz = b.az[i] - exact.az[i];
    const double size = std::sqrt(double(exact.ax[i]) * exact.ax[i] + double(exact.ay[i]) * exact.ay[i] + double(exact.az[i]) * exact.az[i]);
    const double off = std::sqrt(dx * dx + dy * dy + dz * dz) / size;
    e.mean += off / b.size();
    e.worst = std::max(e.worst, off);
  }
  return e;
}

// same hand worked pair as the brute force test: mass 2 at the origin, mass 3 at (2, 3, 6), distance 7
TEST_CASE("the tree gives the hand worked force between two bodies") {
  Bodies b;
  b.add(0, 0, 0, 0, 0, 0, 2);
  b.add(2, 3, 6, 0, 0, 0, 3);
  Octree tree;
  compute_accel_tree(b, tree, 0.0f, 0.5f);

  CHECK(b.ax[0] == doctest::Approx(6.0 / 343.0));
  CHECK(b.ay[0] == doctest::Approx(9.0 / 343.0));
  CHECK(b.az[0] == doctest::Approx(18.0 / 343.0));
  CHECK(b.ax[1] == doctest::Approx(-4.0 / 343.0));
}

// theta 0 never takes the shortcut, so only the order of the additions differs from brute force
TEST_CASE("the tree matches brute force at theta 0") {
  for (bool clumpy : {false, true}) {
    const TreeError e = tree_error(clumpy ? clustered_ball(1000, 5) : uniform_ball(1000, 5), 0.01f, 0.0f);
    MESSAGE(std::string(clumpy ? "clustered" : "uniform") << " theta 0: mean error " << e.mean << ", worst " << e.worst);
    CHECK(e.worst < 1e-4);
  }
}

// a smaller theta opens more cubes, so the answer has to get closer to brute force every time
TEST_CASE("the tree's force error shrinks as theta shrinks") {
  for (bool clumpy : {false, true}) {
    const Bodies b = clumpy ? clustered_ball(1000, 5) : uniform_ball(1000, 5);
    double last = 1e30;
    for (float theta : {1.0f, 0.7f, 0.5f, 0.3f, 0.0f}) {
      const TreeError e = tree_error(b, 0.01f, theta);
      MESSAGE(std::string(clumpy ? "clustered" : "uniform") << " theta " << theta << ": mean error " << e.mean << ", worst " << e.worst);
      CHECK(e.mean < last);
      last = e.mean;

      // and the size of the error has to be about right, a wrong opening rule or a wrong
      // center of mass still shrinks with theta but lands outside this window
      if (theta == 0.5f) {
        CHECK(e.mean > 3e-3);
        CHECK(e.mean < 1.2e-2);
      }
    }
  }
}

// stacked bodies share a leaf at the depth limit and still have to pull on everything else correctly
TEST_CASE("bodies stacked on one point get the same force from the tree as from brute force") {
  Bodies b = uniform_ball(50, 9);
  for (int i = 0; i < 4; ++i) b.add(0.1f, 0.2f, 0.3f, 0, 0, 0, 0.05f);

  for (float eps : {0.0f, 0.01f}) {
    const TreeError e = tree_error(b, eps, 0.0f);
    CHECK(e.worst < 1e-4);
  }
}

// mass 1 at the origin and mass 3 at (1, 1, 1): the root cube's center of mass is far enough from
// the light body to pass the theta test, but that cube contains the light body itself
TEST_CASE("a body never uses a cube it is sitting inside as one far away body") {
  Bodies b;
  b.add(0, 0, 0, 0, 0, 0, 1);
  b.add(1, 1, 1, 0, 0, 0, 3);
  Octree tree;
  compute_accel_tree(b, tree, 0.0f, 1.0f);

  // distance sqrt(3), so the pull on the light body is 3 / 3 = 1 along (1, 1, 1) / sqrt(3)
  CHECK(b.ax[0] == doctest::Approx(1.0 / std::sqrt(3.0)));
  CHECK(b.ay[0] == doctest::Approx(1.0 / std::sqrt(3.0)));
  CHECK(b.az[0] == doctest::Approx(1.0 / std::sqrt(3.0)));
}

// the tree version of step is the same leapfrog, only the force pass is swapped
TEST_CASE("stepping with the tree at theta 0 follows the brute force run") {
  Bodies brute = uniform_ball(200, 2), viatree = brute;
  Octree tree;
  compute_accel(brute, 0.05f);
  compute_accel_tree(viatree, tree, 0.05f, 0.0f);

  for (int s = 0; s < 50; ++s) {
    step(brute, 0.01f, 0.05f);
    step(viatree, tree, 0.01f, 0.05f, 0.0f);
  }

  double apart = 0;
  for (std::size_t i = 0; i < brute.size(); ++i) {
    apart = std::max({apart, double(std::abs(brute.x[i] - viatree.x[i])), double(std::abs(brute.y[i] - viatree.y[i])),
                      double(std::abs(brute.z[i] - viatree.z[i]))});
  }
  CHECK(apart < 1e-4);
}

// tree forces are slightly off, so energy is not held as tightly as brute force, but it must stay close
TEST_CASE("energy stays close when a ball of bodies falls inward under tree forces") {
  Bodies b = uniform_ball(500, 2);
  Octree tree;
  compute_accel_tree(b, tree, 0.05f, 0.5f);
  const double e0 = total_energy(b, 0.05f);

  double worst = 0;
  for (int s = 0; s < 500; ++s) {
    step(b, tree, 0.002f, 0.05f, 0.5f);
    worst = std::max(worst, std::abs((total_energy(b, 0.05f) - e0) / e0));
  }
  MESSAGE("worst energy drift with tree forces at theta 0.5: " << worst);
  CHECK(worst < 2e-2);
}

// adding 30000 small masses one after another in 32 bit loses digits, summing cube by cube does not
TEST_CASE("the root mass stays exact with a lot of bodies") {
  const Bodies b = uniform_ball(30000, 1);
  Octree tree;
  tree.build(b);
  CHECK(tree.nodes[0].mass == doctest::Approx(1.0).epsilon(1e-5));
}

// five bodies a hair apart and far from the origin, where the middles of the small cubes round off
// a check based on cube edges put body positions just outside their own cube here
TEST_CASE("a body does not count itself when cube edges round off") {
  Bodies b;
  b.add(100.000031f, 100.000031f, 100.000122f, 0, 0, 0, 1);
  b.add(100.000099f, 100.000092f, 100.000061f, 0, 0, 0, 1);
  b.add(100.000031f, 100.000031f, 100.000114f, 0, 0, 0, 1);
  b.add(100.000023f, 100.000145f, 100.000008f, 0, 0, 0, 1);
  b.add(100.000023f, 100.000031f, 100.000053f, 0, 0, 0, 1);

  const TreeError e = tree_error(b, 0.0f, 0.7f);
  MESSAGE("worst error with rounded cube edges: " << e.worst);
  CHECK(e.worst < 0.3);
}

TEST_CASE("splitting the tree walk across threads gives the exact same pull as one thread") {
  // a few bodies (fewer than threads) and a lot, spread out and clumped up
  for (bool clumpy : {false, true}) {
    for (int n : {3, 1001}) {
      Bodies one = clumpy ? clustered_ball(n, 11) : uniform_ball(n, 11);
      Octree tree;
      set_threads(1);
      compute_accel_tree(one, tree, 0.01f, 0.7f);

      // each body walks the tree on its own in the same order, so every bit should match
      for (int k : {2, 3, 4}) {
        // starts from garbage, so a body no thread got to cannot pass with the old answer
        Bodies many = one;
        for (auto* a : {&many.ax, &many.ay, &many.az}) a->assign(a->size(), std::nanf(""));
        set_threads(k);
        compute_accel_tree(many, tree, 0.01f, 0.7f);
        CAPTURE(clumpy);
        CAPTURE(n);
        CAPTURE(k);
        CHECK(many.ax == one.ax);
        CHECK(many.ay == one.ay);
        CHECK(many.az == one.az);
      }
    }
  }
  set_threads(1);
}
