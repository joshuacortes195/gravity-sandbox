cmake_minimum_required(VERSION 3.20)
project(gravity_sandbox CXX)

# c++17 everywhere, no compiler specific extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# default to an optimized build so tests and benchmarks run at real speed
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# the physics core, shared by the tests, the cli and later the browser build
add_library(core STATIC core/gravity.cpp core/diagnostics.cpp core/integrator.cpp core/scenes.cpp)
target_include_directories(core PUBLIC core)
target_compile_options(core PRIVATE -Wall -Wextra)

# native test runner, skipped when building for the browser
if(NOT EMSCRIPTEN)
  enable_testing()
  add_executable(tests tests/test_main.cpp tests/test_gravity.cpp tests/test_diagnostics.cpp tests/test_integrator.cpp)
  target_include_directories(tests SYSTEM PRIVATE third_party/doctest)
  target_link_libraries(tests PRIVATE core)
  target_compile_options(tests PRIVATE -Wall -Wextra)
  add_test(NAME tests COMMAND tests)

  # small cli that runs a scene and prints energy drift
  add_executable(run_scene tools/run_scene.cpp)
  target_link_libraries(run_scene PRIVATE core)
  target_compile_options(run_scene PRIVATE -Wall -Wextra)
endif()
