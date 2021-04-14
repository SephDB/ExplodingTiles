# CMakeList.txt : CMake project for ExplodingTiles, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("ExplodingTiles")

find_package(SFML 2.5 COMPONENTS graphics window REQUIRED)

# Add source to this project's executable.
add_executable (ExplodingTiles "src/ExplodingTiles.cpp"  "include/coords.hpp" "include/board.hpp" "include/player.hpp" "include/shapes.hpp")

target_include_directories(ExplodingTiles PUBLIC include)

target_compile_features(ExplodingTiles PUBLIC cxx_std_17)
target_link_libraries(ExplodingTiles sfml-window sfml-graphics)

# TODO: Add tests and install targets if needed.
