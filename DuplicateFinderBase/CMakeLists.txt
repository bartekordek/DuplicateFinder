
set( CMAKE_CXX_STANDARD 20 )

get_filename_component( basename ${CMAKE_CURRENT_SOURCE_DIR} NAME )

set(PROJECT_NAME ${basename})

project(${PROJECT_NAME} LANGUAGES CXX)

set( CMAKE_ENABLE_EXPORTS true )

if( MSVC )
    # warning level 4 and all warnings as errors
    add_compile_options( /Wall )
    #add_compile_options( /WX ) # error on warning.
    add_compile_options( /MP )
    add_compile_options( "/wd4266" )
    add_compile_options( "/wd4820" )
    add_compile_options( "/wd4710" )
    add_compile_options( "/wd5045" )

    find_package( RapidJSON REQUIRED )
    if( RapidJSON_FOUND )
        include_directories( SYSTEM ${RapidJSON_INCLUDE_DIRS} )
    endif( RapidJSON_FOUND )
else()
    # lots of warnings and all warnings as errors
    add_compile_options( -Wall -Wextra -pedantic -Werror -Wno-unused-parameter )
    # -Wno-sized-deallocation
    add_compile_options( -Wno-unused-variable )
    add_definitions( -Wfatal-errors )
endif()

file( GLOB_RECURSE SOURCES inc/*.hpp src/*.hpp src/*.cpp )

source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )

add_library( ${PROJECT_NAME} STATIC ${SOURCES} )

set( DEPENDENCIES
PUBLIC cul_static
)

set_property( TARGET ${PROJECT_NAME} PROPERTY "LINK_LIBRARY_DEPENDENCIES" true )
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS "")

if (MSVC)
    list(APPEND LIBS msvcmrtd )
    target_link_options(${PROJECT_NAME} PUBLIC "/INCREMENTAL:NO")
    target_link_libraries(${PROJECT_NAME} ${DEPENDENCIES})
else()
    target_link_libraries(${PROJECT_NAME} ${DEPENDENCIES})
endif()

if( MSVC )
else()
    if( NOT APPLE )
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    endif()
endif()

target_include_directories(${PROJECT_NAME} PUBLIC inc)
target_include_directories(${PROJECT_NAME} PRIVATE src)
