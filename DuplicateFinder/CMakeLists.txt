#cmake -DCMAKE_BUILD_TYPE=Debug
cmake_minimum_required(VERSION 3.0.0)

get_filename_component( basename ${CMAKE_CURRENT_SOURCE_DIR} NAME )
project( ${basename} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(EXEC ${basename})

if (MSVC)
    add_compile_options( /W2 )
    add_compile_options( /MP )
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
    add_definitions(-Wfatal-errors)
endif()

include_directories(".")

file( GLOB_RECURSE SOURCES *.h *.hpp *.cpp *.vert *.frag )
message( "${EXEC}: ${SOURCES} " )
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )

add_executable( ${EXEC} ${SOURCES} )

if (MSVC)
    set_target_properties(
        ${EXEC}
        PROPERTIES
            OUTPUT_NAME ${EXEC}
            SUFFIX ".exe"
    )
else()
    set_target_properties(
        ${EXEC}
        PROPERTIES
            OUTPUT_NAME ${EXEC}
            SUFFIX ".bin"
    )
endif()

IF(NOT APPLE AND NOT MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
ENDIF()

set( DEPENDENCIES cul )

if( MSVC )
    target_link_libraries( ${EXEC} ${DEPENDENCIES} )
else()
    target_link_libraries( ${EXEC} ${DEPENDENCIES} stdc++fs  )
endif()

target_include_directories(${EXEC} PRIVATE inc)
target_include_directories(${EXEC} PRIVATE src)

if( MSVC )
    set_property(TARGET ${EXEC} PROPERTY "VS_DEBUGGER_WORKING_DIRECTORY" "${CMAKE_CURRENT_SOURCE_DIR}")
endif()