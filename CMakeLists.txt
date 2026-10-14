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
add_library(core STATIC core/gravity.cpp core/diagnostics.cpp core/integrator.cpp core/scenes.cpp core/octree.cpp)
target_include_directories(core PUBLIC core)
target_compile_options(core PRIVATE -Wall -Wextra)

# native test runner, skipped when building for the browser
if(NOT EMSCRIPTEN)
  enable_testing()
  add_executable(tests tests/test_main.cpp tests/test_gravity.cpp tests/test_diagnostics.cpp tests/test_integrator.cpp tests/test_octree.cpp tests/test_scenes.cpp)
  target_include_directories(tests SYSTEM PRIVATE third_party/doctest)
  target_link_libraries(tests PRIVATE core)
  target_compile_options(tests PRIVATE -Wall -Wextra)
  add_test(NAME tests COMMAND tests)

  # small cli that runs a scene and prints energy drift
  add_executable(run_scene tools/run_scene.cpp)
  target_link_libraries(run_scene PRIVATE core)
  target_compile_options(run_scene PRIVATE -Wall -Wextra)

  # checks whether a disk galaxy stays a disk, used to pick the browser's disk mass
  add_executable(disk_check tools/disk_check.cpp)
  target_link_libraries(disk_check PRIVATE core)
  target_compile_options(disk_check PRIVATE -Wall -Wextra)

  # times brute force against the tree and writes the tables in results/
  add_executable(benchmark tools/benchmark.cpp)
  target_link_libraries(benchmark PRIVATE core)
  target_compile_options(benchmark PRIVATE -Wall -Wextra)

  # hands the benchmark the build type and its flags so they end up in the results file
  string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
  target_compile_definitions(benchmark PRIVATE BENCH_FLAGS="${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${build_type_upper}}")
endif()

# browser build: the core plus the bindings, written straight into the web app as an es module and a .wasm file
if(EMSCRIPTEN)
  add_executable(sim wasm/bindings.cpp)
  target_link_libraries(sim PRIVATE core)
  target_compile_options(sim PRIVATE -Wall -Wextra)
  # fixed size memory that never grows, so the views js holds into it never go dead
  target_link_options(sim PRIVATE
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web
    -sINITIAL_MEMORY=64MB -sALLOW_MEMORY_GROWTH=0
    -sEXPORTED_RUNTIME_METHODS=HEAPF32 -sFILESYSTEM=0)
  set_target_properties(sim PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/web/src/wasm)
endif()
