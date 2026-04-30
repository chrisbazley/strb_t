cmake_minimum_required(VERSION 3.10)
project(strb_t C)

set(CMAKE_C_STANDARD 11)

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -Wsign-conversion -pedantic)
endif()

set(SOURCES strb.c)
file(GLOB PUBLIC_HEADERS "*.h")

add_library(strb STATIC ${SOURCES} ${PUBLIC_HEADERS})

set_target_properties(strb PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    DEBUG_POSTFIX "dbg"
)

target_include_directories(strb PUBLIC .)

target_compile_definitions(strb PRIVATE
    $<$<CONFIG:Debug>:DEBUGOUT=1>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_options(strb PRIVATE
    $<$<CONFIG:Release>:-O3>
)

install(TARGETS strb
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
