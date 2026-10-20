# Host unit tests: hardware-free components compiled and run on the PC.
#   cmake -S test/host -B build/host && cmake --build build/host && ctest --test-dir build/host --output-on-failure
cmake_minimum_required(VERSION 3.22)
project(tiny_ecus_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)
# Unity v2.7.0 (2026-07-16), pinned by commit so the tag can't move under us.
FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        b6763fbd9cedfacaa89e2ad9fd00d615a234e355)
FetchContent_MakeAvailable(unity)

set(COMPONENTS_DIR ${CMAKE_CURRENT_LIST_DIR}/../../components)

# Our code only: strict warnings plus AddressSanitizer and UndefinedBehaviorSanitizer,
# which abort the test on buffer overruns and undefined behavior. CI always runs
# them; locally they need the runtime libraries (Fedora: libasan libubsan).
option(SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" ON)
set(STRICT_FLAGS -Wall -Wextra -Werror -g)
set(SANITIZE_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all)
if(SANITIZE)
    list(APPEND STRICT_FLAGS ${SANITIZE_FLAGS})
endif()

add_library(tiny_cmd STATIC
    ${COMPONENTS_DIR}/tiny_cmd/tiny_cmd.c
    ${COMPONENTS_DIR}/tiny_cmd/tiny_line.c)
target_include_directories(tiny_cmd PUBLIC ${COMPONENTS_DIR}/tiny_cmd/include)
target_compile_options(tiny_cmd PRIVATE ${STRICT_FLAGS})
if(SANITIZE)
    target_link_options(tiny_cmd PUBLIC ${SANITIZE_FLAGS})
endif()
target_link_libraries(tiny_cmd PUBLIC m)

enable_testing()
foreach(t test_tiny_cmd test_tiny_line)
    add_executable(${t} ${t}.c)
    target_compile_options(${t} PRIVATE ${STRICT_FLAGS})
    target_link_libraries(${t} PRIVATE tiny_cmd unity)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
