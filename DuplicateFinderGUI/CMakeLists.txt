#cmake -DCMAKE_BUILD_TYPE=Debug
cmake_minimum_required(VERSION 3.0.0)

get_filename_component( basename ${CMAKE_CURRENT_SOURCE_DIR} NAME )

set(CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME_DYNAMIC ${basename}_dynamic)
set(PROJECT_NAME_STATIC ${basename}_static)

project( ${PROJECT_NAME_DYNAMIC} LANGUAGES CXX)
project( ${PROJECT_NAME_STATIC} LANGUAGES CXX)

if (MSVC)
    add_compile_options( /Wall )
    add_compile_options( /MP )
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
    add_definitions(-Wfatal-errors)
endif()

include_directories("inc")
include_directories( SYSTEM "../deps/nativefiledialog-extended/src/include")

include_directories( SYSTEM "../deps/GameEngine/deps/SDL2Wrapper/deps/CUL/deps/sqlite-amalgamation-3340000" )

file( GLOB_RECURSE SOURCES *.h *.hpp *.cpp *.vert *.frag )
message( "${EXEC}: ${SOURCES} " )
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )

add_executable( ${PROJECT_NAME_DYNAMIC} ${SOURCES} )
add_executable( ${PROJECT_NAME_STATIC} ${SOURCES} )

if (MSVC)
    set_target_properties(
        ${PROJECT_NAME_DYNAMIC}
        PROPERTIES
            OUTPUT_NAME ${PROJECT_NAME_DYNAMIC}
            SUFFIX ".exe"
    )
    set_target_properties(
        ${PROJECT_NAME_STATIC}
        PROPERTIES
            OUTPUT_NAME ${PROJECT_NAME_STATIC}
            SUFFIX ".exe"
    )
else()
    set_target_properties(
        ${PROJECT_NAME_DYNAMIC}
        PROPERTIES
            OUTPUT_NAME ${PROJECT_NAME_DYNAMIC}
            SUFFIX ".bin"
    )
    set_target_properties(
        ${PROJECT_NAME_STATIC}
        PROPERTIES
            OUTPUT_NAME ${PROJECT_NAME_STATIC}
            SUFFIX ".bin"
    )
endif()

IF(NOT APPLE AND NOT MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
ENDIF()
set( DEPENDENCIES_STATIC
PRIVATE gameengine_static
PRIVATE AdditionalDeps
nfd
)

set( DEPENDENCIES_DYNAMIC
PRIVATE gameengine_dynamic
PRIVATE AdditionalDeps
nfd
)

if( MSVC )
    target_link_options(${PROJECT_NAME_DYNAMIC} PUBLIC "/INCREMENTAL:NO")
    target_link_options(${PROJECT_NAME_STATIC} PUBLIC "/INCREMENTAL:NO")

    target_link_libraries( ${PROJECT_NAME_DYNAMIC} ${DEPENDENCIES_DYNAMIC} )
    target_link_libraries( ${PROJECT_NAME_STATIC} ${DEPENDENCIES_STATIC} )

    set_target_properties( ${PROJECT_NAME_STATIC} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_target_properties( ${PROJECT_NAME_STATIC} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
else()
    target_link_libraries( ${PROJECT_NAME_DYNAMIC} ${DEPENDENCIES_DYNAMIC} stdc++fs  )
    target_link_libraries( ${PROJECT_NAME_STATIC} ${DEPENDENCIES_STATIC} stdc++fs  )
endif()

target_include_directories(${PROJECT_NAME_DYNAMIC} PRIVATE inc)
target_include_directories(${PROJECT_NAME_DYNAMIC} PRIVATE src)

target_include_directories(${PROJECT_NAME_STATIC} PRIVATE inc)
target_include_directories(${PROJECT_NAME_STATIC} PRIVATE src)

if( MSVC )
    set_property(TARGET ${EXEPROJECT_NAME_STATICC} PROPERTY "VS_DEBUGGER_WORKING_DIRECTORY" "${CMAKE_CURRENT_SOURCE_DIR}")
    set_property(TARGET ${PROJECT_NAME_DYNAMIC} PROPERTY "VS_DEBUGGER_WORKING_DIRECTORY" "${CMAKE_CURRENT_SOURCE_DIR}")
endif()