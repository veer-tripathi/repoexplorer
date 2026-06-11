cmake_minimum_required(VERSION 3.16)
project(repoexplorer VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    add_compile_options(/W3 /EHsc)
else()
    add_compile_options(-Wall -Wextra)
endif()

# All module sources except main.cpp (shared by the app and the test).
set(MODULE_SOURCES
    src/utils.cpp
    src/scanner.cpp
    src/parser.cpp
    src/graph.cpp
    src/analyzer.cpp
    src/view.cpp
    src/exporter.cpp
    src/tui.cpp)

# ---- Application -----------------------------------------------------------
add_executable(repoexplorer src/main.cpp ${MODULE_SOURCES})
target_include_directories(repoexplorer PRIVATE src)

# Optional ncurses TUI (auto-detected; falls back to a message if missing).
find_package(Curses QUIET)
if(CURSES_FOUND)
    message(STATUS "ncurses found - building interactive TUI")
    target_compile_definitions(repoexplorer PRIVATE REPOEXPLORER_HAS_NCURSES=1)
    target_include_directories(repoexplorer PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(repoexplorer PRIVATE ${CURSES_LIBRARIES})
else()
    message(STATUS "ncurses NOT found - TUI prints a fallback message")
endif()

# ---- Tests (plain executable, no framework) --------------------------------
enable_testing()
add_executable(repoexplorer_tests test/test.cpp ${MODULE_SOURCES})
target_include_directories(repoexplorer_tests PRIVATE src)
add_test(NAME repoexplorer_tests COMMAND repoexplorer_tests)
