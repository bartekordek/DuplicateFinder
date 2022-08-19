cmake_minimum_required (VERSION 3.5)

project(duplicate_finder_main)

set( DUP_FIND_ROOT ${CMAKE_CURRENT_SOURCE_DIR} )
set( DUP_FIND_BUILD_ROOT ${CMAKE_SOURCE_DIR} )
if( MSVC )
    set( DUP_FIND_OUTPUT_DIR ${DUP_FIND_BUILD_ROOT}/Build-Windows )
else()
    set( DUP_FIND_OUTPUT_DIR ${DUP_FIND_BUILD_ROOT}/Build-Linux )
endif()

message( "DUP_FIND_OUTPUT_DIR: ${DUP_FIND_OUTPUT_DIR}" )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${DUP_FIND_OUTPUT_DIR} )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${DUP_FIND_OUTPUT_DIR} )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DUP_FIND_OUTPUT_DIR} )

set( DUP_FIND_RUN_DIR ${DUP_FIND_ROOT}/runDir )
set( DUP_FIND_DEPS ${DUP_FIND_ROOT}/deps )

add_subdirectory( deps/GameEngine )
add_subdirectory( DuplicateFinder )
add_subdirectory( DuplicateFinderGUI )