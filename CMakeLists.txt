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

# the physics core, header only for now, source files get added in phase 1
add_library(core INTERFACE)
target_include_directories(core INTERFACE core)

# native test runner, skipped when building for the browser
if(NOT EMSCRIPTEN)
  enable_testing()
  add_executable(tests tests/test_main.cpp tests/test_units.cpp)
  target_include_directories(tests SYSTEM PRIVATE third_party/doctest)
  target_link_libraries(tests PRIVATE core)
  target_compile_options(tests PRIVATE -Wall -Wextra)
  add_test(NAME tests COMMAND tests)
endif()
