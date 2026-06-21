cmake_minimum_required(VERSION 3.5)
project(duplicate_finder_main)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -ggdb")


set( DUP_FIND_ROOT ${CMAKE_CURRENT_SOURCE_DIR} )
set( DUP_FIND_BUILD_ROOT ${CMAKE_SOURCE_DIR} )

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set( DUP_FIND_RUN_DIR ${DUP_FIND_ROOT}/runDir )
set( DUP_FIND_DEPS ${DUP_FIND_ROOT}/deps )

add_subdirectory( deps/GameEngine )

set(wxBUILD_SAMPLES ALL CACHE BOOL "WX SAMPLES" FORCE)
add_subdirectory(deps/wxWidgets)
add_subdirectory(DuplicateFinderBase)
add_subdirectory(DuplicateFinderGUI)
add_subdirectory(DuplicateFinderWxWidgets)