
set( CMAKE_CXX_STANDARD 20 )

get_filename_component( basename ${CMAKE_CURRENT_SOURCE_DIR} NAME )

set(PROJECT_NAME_DYNAMIC ${basename}_dynamic)
set(PROJECT_NAME_STATIC ${basename}_static)

project( ${PROJECT_NAME_DYNAMIC} LANGUAGES CXX)
project( ${PROJECT_NAME_STATIC} LANGUAGES CXX)

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

    add_compile_options("$<$<CONFIG:RelWithDebInfo>:/Ot>")
    add_compile_options("$<$<CONFIG:RelWithDebInfo>:/Oi>")

    add_compile_options("$<$<CONFIG:Release>:/Ot>")
    add_compile_options("$<$<CONFIG:Release>:/Oi>")

    find_package( RapidJSON REQUIRED )
    if( RapidJSON_FOUND )
        include_directories( SYSTEM ${RapidJSON_INCLUDE_DIRS} )
    endif( RapidJSON_FOUND )
else()
    # lots of warnings and all warnings as errors
    add_compile_options( -Wall -Wextra -pedantic -Wno-unused-parameter )
    # -Wno-sized-deallocation
    add_compile_options( -Wno-unused-variable )
    add_definitions( -Wfatal-errors )
endif()

file( GLOB_RECURSE SOURCES inc/*.hpp src/*.hpp src/*.cpp )

source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )

add_library( ${PROJECT_NAME_STATIC} STATIC ${SOURCES} )
add_library( ${PROJECT_NAME_DYNAMIC} SHARED ${SOURCES} )

set( DEPENDENCIES_STATIC
PUBLIC gameengine_static
PRIVATE AdditionalDeps
PUBLIC nfd
)

set( DEPENDENCIES_DYNAMIC
PUBLIC gameengine_dynamic
PUBLIC AdditionalDeps
PUBLIC nfd
)

set_property( TARGET ${PROJECT_NAME_STATIC} PROPERTY "LINK_LIBRARY_DEPENDENCIES" true )
set_property( TARGET ${PROJECT_NAME_DYNAMIC} PROPERTY "LINK_LIBRARY_DEPENDENCIES" true )


if (MSVC)
    list(APPEND LIBS msvcmrtd )
    target_link_options(${PROJECT_NAME_STATIC} PUBLIC "/INCREMENTAL:NO")
    target_link_options(${PROJECT_NAME_DYNAMIC} PUBLIC "/INCREMENTAL:NO")

    target_link_libraries(${PROJECT_NAME_STATIC} ${DEPENDENCIES_STATIC})
    target_link_libraries(${PROJECT_NAME_DYNAMIC} ${DEPENDENCIES_DYNAMIC})
else()
    target_link_libraries(${PROJECT_NAME_STATIC} ${DEPENDENCIES_STATIC})
    target_link_libraries(${PROJECT_NAME_DYNAMIC} ${DEPENDENCIES_DYNAMIC})
endif()

if( MSVC )
else()
    if( NOT APPLE )
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    endif()
endif()

target_compile_definitions(${PROJECT_NAME_STATIC} PRIVATE DUP_FIN_BASE_STATIC=1)

target_include_directories(${PROJECT_NAME_STATIC} PUBLIC inc)
target_include_directories(${PROJECT_NAME_STATIC} PRIVATE src)

target_include_directories(${PROJECT_NAME_DYNAMIC} PUBLIC inc)
target_include_directories(${PROJECT_NAME_DYNAMIC} PRIVATE src)
