cmake_minimum_required(VERSION 3.10)
project(strb_t C)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy
        HINTS "/opt/homebrew/opt/llvm/bin"
    )

    if(CLANG_TIDY_EXE)
        # 1. Base Clang-Tidy call configuration
        set(CLANG_TIDY_ARGS "${CLANG_TIDY_EXE};-checks=-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.*")

        # 2. On Apple machines, append the current platform SDK directory path
        if(APPLE)
            execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE MAC_SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
            list(APPEND CLANG_TIDY_ARGS "--extra-arg=-isysroot" "--extra-arg=${MAC_SDK_PATH}")
        endif()

        # 3. Apply the modified list to CMake
        set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_ARGS}")
        message(STATUS "Clang-Tidy Static Analyzer enabled automatically.")
    else()
        message(STATUS "clang-tidy binary not found. Skipping analysis pass.")
    endif()
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

