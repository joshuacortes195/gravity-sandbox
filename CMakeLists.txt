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
add_library(core STATIC core/gravity.cpp core/diagnostics.cpp core/integrator.cpp core/scenes.cpp core/octree.cpp core/threads.cpp)
target_include_directories(core PUBLIC core)
target_compile_options(core PRIVATE -Wall -Wextra)

# native test runner, skipped when building for the browser
if(NOT EMSCRIPTEN)
  enable_testing()
  add_executable(tests tests/test_main.cpp tests/test_gravity.cpp tests/test_diagnostics.cpp tests/test_integrator.cpp tests/test_octree.cpp tests/test_scenes.cpp tests/test_presets.cpp)
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

  # runs one of the browser presets natively and prints its energy drift
  add_executable(preset_check tools/preset_check.cpp)
  target_link_libraries(preset_check PRIVATE core)
  target_compile_options(preset_check PRIVATE -Wall -Wextra)

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
  # three builds of the same code: sim (plain, the baseline for measuring), sim-simd (4 floats per
  # instruction, used when the page cannot share memory) and sim-mt (simd plus threads)
  # threads need every file built with -pthread, so each build gets its own copy of the core
  foreach(variant sim sim-simd sim-mt)
    if(variant STREQUAL "sim")
      set(flags "")
    elseif(variant STREQUAL "sim-simd")
      set(flags -msimd128)
    else()
      set(flags -msimd128 -pthread)
    endif()

    add_library(core-${variant} STATIC core/gravity.cpp core/diagnostics.cpp core/integrator.cpp core/scenes.cpp core/octree.cpp core/threads.cpp)
    target_include_directories(core-${variant} PUBLIC core)
    target_compile_options(core-${variant} PRIVATE -Wall -Wextra ${flags})

    add_executable(${variant} wasm/bindings.cpp)
    target_link_libraries(${variant} PRIVATE core-${variant})
    target_compile_options(${variant} PRIVATE -Wall -Wextra ${flags})
    # fixed size memory that never grows, so the views js holds into it never go dead
    target_link_options(${variant} PRIVATE ${flags}
      -sMODULARIZE=1 -sEXPORT_ES6=1
      -sINITIAL_MEMORY=64MB -sALLOW_MEMORY_GROWTH=0
      -sEXPORTED_RUNTIME_METHODS=HEAPF32 -sFILESYSTEM=0)
    set_target_properties(${variant} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/web/src/wasm)
  endforeach()

  target_link_options(sim PRIVATE -sENVIRONMENT=web)
  target_link_options(sim-simd PRIVATE -sENVIRONMENT=web)
  # the threaded build starts one worker per logical core while it loads, so a thread never has to
  # wait for a worker mid step (the page's main thread is not allowed to sit and wait for one).
  # strict means asking for more threads than that fails loudly instead of hanging
  target_link_options(sim-mt PRIVATE -sENVIRONMENT=web,worker
    -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency -sPTHREAD_POOL_SIZE_STRICT=2)
endif()
