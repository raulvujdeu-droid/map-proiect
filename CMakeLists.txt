cmake_minimum_required(VERSION 3.22)
project(map_project CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# cpp-httplib links against OpenSSL, zlib and brotli when it finds them.
# The gcc build image has them, debian-slim does not, so the binary would fail
# to start in the final image. None of them are needed here, so they stay off.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE    OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE  OFF CACHE BOOL "" FORCE)

# Header-only HTTP server. No package manager needed.
FetchContent_Declare(httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.3
  GIT_SHALLOW TRUE)

# Header-only JSON library.
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(httplib json)

add_executable(server src/main.cpp)
target_include_directories(server PRIVATE src)
target_link_libraries(server PRIVATE httplib::httplib nlohmann_json::nlohmann_json)

# The C++ runtime is linked statically so the final image does not need libstdc++.
# The build image and the runtime image are different environments.
target_link_options(server PRIVATE -static-libstdc++ -static-libgcc)

# Tests are built only when explicitly requested, so the Docker build stays lean.
option(BUILD_TESTS "Build the unit tests" OFF)
if(BUILD_TESTS)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(doctest)

  add_executable(tests tests/test_app.cpp)
  target_include_directories(tests PRIVATE src)
  target_link_libraries(tests PRIVATE doctest::doctest nlohmann_json::nlohmann_json)

  enable_testing()
  add_test(NAME unit_tests COMMAND tests)
endif()
