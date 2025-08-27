cmake_minimum_required(VERSION 3.28)
project(MemoriaDB)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB MEMORIA_PUBLIC_HEADERS CONFIGURE_DEPENDS
        src/memoria/*.h)
file(GLOB MEMORIA_PRIVATE_HEADERS CONFIGURE_DEPENDS
        src/memoria_internal/*.h)
file(GLOB MEMORIA_SOURCES CONFIGURE_DEPENDS
        lib/*.cpp extras/io/*.cpp)   # include persistence when present

add_library(memoriadb STATIC
        ${MEMORIA_PUBLIC_HEADERS}
        ${MEMORIA_PRIVATE_HEADERS}
        ${MEMORIA_SOURCES}
        src/memoria/StatementReader.h
        lib/StatementReader.cpp
        src/app/main.cpp
)

target_include_directories(memoriadb
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(memoriadb_cli
        src/app/main.cpp
)
target_link_libraries(memoriadb_cli PRIVATE memoriadb)
set_target_properties(memoriadb_cli PROPERTIES OUTPUT_NAME memoriadb)


# ---- GoogleTest
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # for MSVC
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)