cmake_minimum_required(VERSION 3.10)
project(strb_t C)
enable_testing()

option(USE_OPTIONAL "Enable the _Optional qualifier" OFF)
option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)

if(USE_OPTIONAL)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR
            "USE_OPTIONAL requires Christopher Bazley's Clang fork")
    endif()

    add_compile_definitions(USE_OPTIONAL)

    if(CMAKE_GENERATOR STREQUAL "Xcode")
        set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_ANALYZER_OTHER_FLAGS
            "-analyzer-checker optionality,core -analyzer-max-loop 32")
    endif()
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4 /wd4200 /wd4996 /wd4130)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -Wsign-compare -pedantic)
endif()

if(ENABLE_CLANG_TIDY)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "ENABLE_CLANG_TIDY requires Clang")
    endif()

    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy
        HINTS "/opt/homebrew/opt/llvm/bin"
    )

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR
            "ENABLE_CLANG_TIDY is ON, but clang-tidy was not found")
    endif()

    set(CMAKE_C_CLANG_TIDY
        "${CLANG_TIDY_EXE}"
        "--checks=-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.*"
        "--warnings-as-errors=clang-analyzer-optionality.*"
    )

    message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
    message(STATUS "_Optional static analysis enabled")
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /wd4200 /wd4996 /wd4130)
else()
    add_compile_options(-Wall -Wextra -Wsign-conversion -pedantic)
endif()

set(SOURCES strb.c)
file(GLOB PUBLIC_HEADERS "*.h")

# Helper function to create library + corresponding test
function(add_strb_variant suffix definitions)
    set(lib_target "strb${suffix}")

    if("${suffix}" STREQUAL "")
        set(test_target "test_default")
    else()
        set(test_target "test_${suffix}")
    endif()

    # Create the Library
    add_library(${lib_target} STATIC ${SOURCES} ${PUBLIC_HEADERS})
    target_compile_definitions(${lib_target} PRIVATE ${definitions})
    target_include_directories(${lib_target} PUBLIC .)
    
    set_target_properties(${lib_target} PROPERTIES
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
        DEBUG_POSTFIX "dbg"
        OUTPUT_NAME "strb${suffix}"
    )

    # Create the Test Executable
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test.c")
        add_executable(${test_target} test.c)
        target_link_libraries(${test_target} PRIVATE ${lib_target})
        # Ensure the test executable also gets the variant definitions
        target_compile_definitions(${test_target} PRIVATE ${definitions})

        # Register the test so CTest can run it
        add_test(NAME "check_${test_target}" COMMAND ${test_target})
    endif()

    install(TARGETS ${lib_target}
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include
    )
endfunction()

## Build all variants
add_strb_variant("" "")
add_strb_variant("static" "STRB_STATIC_ALLOC")
add_strb_variant("freestanding" "STRB_FREESTANDING")

# Optimization and Debug settings
add_compile_definitions(
    $<$<CONFIG:Debug>:DEBUGOUT=1>
)

