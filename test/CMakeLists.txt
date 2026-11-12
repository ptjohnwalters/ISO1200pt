cmake_minimum_required(VERSION 3.16)
project(iso1200pt_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(
  unit_tests
  calibration_settings_tests.cpp
  ui_state_tests.cpp
  lockout_feedback_tests.cpp
  mcp23017_output_safety_tests.cpp)
target_include_directories(
  unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../examples/1200PT/include)
target_link_libraries(unit_tests PRIVATE GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(unit_tests)
