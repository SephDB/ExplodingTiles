# CMakeList.txt : CMake project for ExplodingTiles, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("ExplodingTiles")

find_package(SFML 2.5 COMPONENTS graphics window REQUIRED)

# Add source to this project's executable.
add_executable (ExplodingTiles "src/ExplodingTiles.cpp"  "include/coords.hpp" "include/board.hpp" "include/player.hpp" "include/shapes.hpp" "include/game.hpp" "include/bezier.hpp" "include/vectorops.hpp")

target_include_directories(ExplodingTiles PUBLIC include)

target_compile_features(ExplodingTiles PUBLIC cxx_std_20)
target_link_libraries(ExplodingTiles sfml-window sfml-graphics)

add_executable(ExplodingTiles_AI "src/AITest.cpp")
target_include_directories(ExplodingTiles_AI PUBLIC include)
target_compile_features(ExplodingTiles_AI PUBLIC cxx_std_20)
target_link_libraries(ExplodingTiles_AI sfml-graphics)

# TODO: Add tests and install targets if needed.
