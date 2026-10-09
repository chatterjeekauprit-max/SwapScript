cmake_minimum_required(VERSION 3.16)
project(SwapScript C)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/swapscript_generated.h")
add_custom_command(
    OUTPUT "${GENERATED_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compiler.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/demo.swp" -o "${GENERATED_HEADER}"
    DEPENDS compiler.py demo.swp
    VERBATIM)
add_custom_target(swapscript_rules DEPENDS "${GENERATED_HEADER}")

add_executable(swapscript_demo demo.c "${GENERATED_HEADER}")
add_dependencies(swapscript_demo swapscript_rules)
target_include_directories(swapscript_demo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(swapscript_demo PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)

add_executable(swapscript_runtime_tests test_runtime.c)
target_include_directories(swapscript_runtime_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(swapscript_runtime_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME runtime COMMAND swapscript_runtime_tests)
    add_test(NAME compiler
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests -v)
    set_tests_properties(compiler PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
