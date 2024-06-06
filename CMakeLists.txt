cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,cppcoreguidelines-*)
endif()


project(threadpoolexecutor VERSION 0.0.3
    DESCRIPTION "A simple C++ implementation of thread pool"
    LANGUAGES CXX)

if (PROJECT_BINARY_DIR STREQUAL PROJECT_SOURCE_DIR)
    message(WARNING "The binary directory of cmake cannot be the same as source directory")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

if (MSVC)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES)
endif()

set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

add_library(compiler_flags INTERFACE)
target_compile_options(compiler_flags INTERFACE
    "$<${gcc_like_cxx}:-Wall;-Wextra;-Wshadow;-Wconversion;-Wpedantic;-Werror;-fno-rtti;-Wnon-virtual-dtor;-Wmisleading-indentation;-march=native>"
    "$<${msvc_cxx}:/W3>"
)

find_package(Threads REQUIRED)

# a header only libaray
add_library(tpexecutor INTERFACE)
target_link_libraries(tpexecutor INTERFACE Threads::Threads)
target_include_directories(tpexecutor INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

add_subdirectory(tests)

