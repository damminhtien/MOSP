cmake_minimum_required(VERSION 3.16)
project(MOSP_SOPMOA LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -Wall")

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(Boost_NO_BOOST_CMAKE ON)

include_directories("inc")
file(GLOB SOURCES "src/algorithms/*.cpp" "src/problem/*.cpp" "src/utils/*.cpp")

find_package(Boost REQUIRED COMPONENTS program_options)
include_directories( ${Boost_INCLUDE_DIRS} )

message(${CMAKE_BINARY_DIR})

add_executable(main src/main.cpp ${SOURCES} ${GCL_NDT_SOURCES})
target_link_libraries(main PRIVATE ${Boost_LIBRARIES})

if(APPLE)
    target_compile_definitions(main PRIVATE OS_MAC)
endif()

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(main PUBLIC OpenMP::OpenMP_CXX)
else()
    message(WARNING "OpenMP not found; building without OpenMP runtime support")
endif()

# sudo apt install libtbb-dev
find_package(TBB REQUIRED)
target_link_libraries(main PUBLIC TBB::tbb)

if(BUILD_TESTING)
    add_executable(
        linkedlist_smoke
        tests/linkedlist_smoke.cpp
    )
    target_include_directories(linkedlist_smoke PRIVATE inc)
    add_test(NAME linkedlist_smoke COMMAND linkedlist_smoke)

    add_executable(
        pq_bucket_smoke
        tests/pq_bucket_smoke.cpp
    )
    target_include_directories(pq_bucket_smoke PRIVATE inc)
    add_test(NAME pq_bucket_smoke COMMAND pq_bucket_smoke)

    add_executable(
        benchmark_metrics_smoke
        tests/benchmark_metrics_smoke.cpp
        ${SOURCES}
    )
    target_include_directories(benchmark_metrics_smoke PRIVATE inc)
    target_link_libraries(benchmark_metrics_smoke PRIVATE ${Boost_LIBRARIES} TBB::tbb)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(benchmark_metrics_smoke PRIVATE OpenMP::OpenMP_CXX)
    endif()
    add_test(NAME benchmark_metrics_smoke COMMAND benchmark_metrics_smoke)

    add_executable(
        correctness_harness
        tests/correctness_harness.cpp
        ${SOURCES}
    )
    target_include_directories(correctness_harness PRIVATE inc)
    target_link_libraries(correctness_harness PRIVATE ${Boost_LIBRARIES} TBB::tbb)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(correctness_harness PRIVATE OpenMP::OpenMP_CXX)
    endif()
    add_test(NAME correctness_harness COMMAND correctness_harness)

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME cli_validation_smoke
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/cli_validation_smoke.py $<TARGET_FILE:main>
    )
endif()
