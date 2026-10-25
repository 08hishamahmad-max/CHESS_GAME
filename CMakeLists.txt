# ChessGame - C++17 chess with an SFML user interface.
#
# SFML 2.6.2 is downloaded and built automatically (FetchContent), statically,
# so nothing has to be installed by hand except a compiler and CMake.
#
# Targets:
#   ChessGame   - the game (GUI)
#   perft       - move generator test (console)
#   unit_tests  - rules / notation / PGN / clock / AI tests (console)
cmake_minimum_required(VERSION 3.16)
project(ChessGame VERSION 2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimised build (the AI is much stronger in Release).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CHESS_BUILD_GUI "Build the SFML game (turn off to build only the engine and tests)" ON)

# Warnings for our own code only (not for SFML).
function(chess_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# On MinGW link everything statically (libgcc, libstdc++, winpthread) so the
# .exe runs on double-click without any extra DLLs.
function(chess_static_runtime target)
    if(MINGW)
        target_link_options(${target} PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Engine: pure C++ chess logic, no graphics (used by the game and the tests)
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

add_library(chess_engine STATIC
    src/engine/AsyncSearch.cpp
    src/engine/Board.cpp
    src/engine/ChessClock.cpp
    src/engine/Game.cpp
    src/engine/Move.cpp
    src/engine/MoveGenerator.cpp
    src/engine/Notation.cpp
    src/engine/OpeningBook.cpp
    src/engine/Pgn.cpp
    src/engine/Search.cpp
)
target_include_directories(chess_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(chess_engine PUBLIC Threads::Threads)
chess_set_warnings(chess_engine)

# ---------------------------------------------------------------------------
# Tests (console programs). Run them with `ctest` or directly.
# ---------------------------------------------------------------------------
enable_testing()

add_executable(perft tests/perft.cpp)
target_link_libraries(perft PRIVATE chess_engine)
chess_set_warnings(perft)
chess_static_runtime(perft)
add_test(NAME perft COMMAND perft)

add_executable(unit_tests tests/unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE chess_engine)
chess_set_warnings(unit_tests)
chess_static_runtime(unit_tests)
add_test(NAME unit_tests COMMAND unit_tests)

# ---------------------------------------------------------------------------
# The game (SFML)
# ---------------------------------------------------------------------------
if(CHESS_BUILD_GUI)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)   # use extraction time as file timestamps
    endif()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)        # static SFML
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
    if(MINGW)
        set(SFML_USE_STATIC_STD_LIBS ON CACHE BOOL "" FORCE)
    endif()
    # Downloaded as a zip archive, so Git is not required.
    FetchContent_Declare(SFML
        URL https://github.com/SFML/SFML/archive/refs/tags/2.6.2.zip
        URL_HASH SHA256=19d6dbd9c901c74441d9888c13cb1399f614fe8993d59062a72cfbceb00fed04
    )
    FetchContent_MakeAvailable(SFML)

    add_executable(ChessGame WIN32   # WIN32 = no console window on Windows
        src/main.cpp
        src/gui/App.cpp
        src/gui/Dialogs.cpp
        src/gui/GameScreen.cpp
        src/gui/MenuScreen.cpp
        src/gui/Renderer.cpp
        src/gui/Settings.cpp
        src/gui/SoundManager.cpp
        src/gui/Ui.cpp
    )
    target_link_libraries(ChessGame PRIVATE chess_engine sfml-graphics sfml-audio sfml-window sfml-system)
    if(WIN32)
        target_link_libraries(ChessGame PRIVATE sfml-main)   # provides WinMain -> main
    endif()
    # Fallback location of the assets when the program is started from an IDE.
    target_compile_definitions(ChessGame PRIVATE CHESS_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    # GCC 8 (e.g. the old "MinGW-w64 8.1" installer) keeps std::filesystem in a separate library.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
        target_link_libraries(ChessGame PRIVATE stdc++fs)
    endif()
    chess_set_warnings(ChessGame)
    chess_static_runtime(ChessGame)

    # Copy the assets folder next to the executable after every build
    # (works for single- and multi-config generators, e.g. build/Release/).
    add_custom_command(TARGET ChessGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:ChessGame>/assets
        COMMENT "Copying assets next to the executable")

    # SFML's audio module needs OpenAL; on Windows it is a DLL shipped with SFML.
    if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(CHESS_OPENAL_DLL ${sfml_SOURCE_DIR}/extlibs/bin/x64/openal32.dll)
        else()
            set(CHESS_OPENAL_DLL ${sfml_SOURCE_DIR}/extlibs/bin/x86/openal32.dll)
        endif()
        add_custom_command(TARGET ChessGame POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CHESS_OPENAL_DLL} $<TARGET_FILE_DIR:ChessGame>
            COMMENT "Copying openal32.dll next to the executable")
    endif()
endif()
