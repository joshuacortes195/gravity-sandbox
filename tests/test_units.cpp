#include "doctest.h"
#include "units.h"

// proves the test runner builds, links against the core and runs
TEST_CASE("gravity constant is 1 in sim units") {
  CHECK(G == 1.0f);
}
