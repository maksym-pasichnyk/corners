cmake_minimum_required(VERSION 3.26)
project(game)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)

FetchContent_Declare(fmt URL ${CMAKE_CURRENT_SOURCE_DIR}/deps/fmt-10.0.0.zip DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(SDL2 URL ${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL2-2.28.2.zip DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(SDL2)

add_executable(game src/main.cpp src/pch.hpp src/loop.hpp src/stb_image.h src/math.hpp)
target_link_libraries(game PUBLIC fmt::fmt)
target_link_libraries(game PUBLIC SDL2::SDL2)
target_precompile_headers(game PUBLIC src/pch.hpp)

#if (EMSCRIPTEN)
set_target_properties(game PROPERTIES SUFFIX ".html")
set_target_properties(game PROPERTIES LINK_FLAGS "--preload-file assets")
#endif ()