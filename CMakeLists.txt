cmake_minimum_required(VERSION 3.16)
project(SandBoxX VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(sandboxx_core STATIC
    src/config/sandbox_config.cpp
    src/runtime/runtime.cpp
    src/process/process_manager.cpp
    src/namespace/namespace_manager.cpp
    src/cgroup/cgroup_manager.cpp
    src/filesystem/filesystem_manager.cpp
    src/security/security_manager.cpp
    src/monitor/monitor.cpp
)
target_include_directories(sandboxx_core PUBLIC include)
target_compile_options(sandboxx_core PRIVATE -Wall -Wextra)

add_executable(sandboxx src/main.cpp)
target_link_libraries(sandboxx PRIVATE sandboxx_core)
target_compile_options(sandboxx PRIVATE -Wall -Wextra)

# Example workloads used to exercise the sandbox (build/examples/<name>).
foreach(example hello cpu_bomb memory_bomb file_test hostname_test)
    add_executable(example_${example} examples/${example}.cpp)
    set_target_properties(example_${example} PROPERTIES
        OUTPUT_NAME ${example}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
endforeach()

enable_testing()
foreach(test runtime config namespace cgroup filesystem security)
    add_executable(${test}_test tests/${test}_test.cpp)
    target_link_libraries(${test}_test PRIVATE sandboxx_core)
    target_compile_options(${test}_test PRIVATE -Wall -Wextra)
    set(test_args "")
    if(test STREQUAL "config")
        file(GLOB test_args ${CMAKE_SOURCE_DIR}/configs/*.json)
    elseif(test STREQUAL "filesystem")
        set(test_args ${CMAKE_SOURCE_DIR}/rootfs)
    endif()
    add_test(NAME ${test}_test COMMAND ${test}_test ${test_args})
    # Namespace, cgroup, filesystem and security tests need Linux and root,
    # and report themselves as skipped otherwise.
    set_tests_properties(${test}_test PROPERTIES SKIP_RETURN_CODE 77)
endforeach()
