cmake_minimum_required (VERSION 2.8.11)
project (ttdump)

#
# Perform build type specific configuration.
#
set (CC_DEBUG_OPT "-ggdb3")

set (CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS_DEBUG} ${CC_DEBUG_OPT} -O0")
set (CMAKE_C_FLAGS_RELWITHDEBINFO
    "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CC_DEBUG_OPT} -O2")
set (CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} ${CC_DEBUG_OPT} -O0")
set (CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CC_DEBUG_OPT} -O2")

unset(CC_DEBUG_OPT)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Choose the type of build, options are: None Debug Release"
        FORCE)
endif()

find_program(ECHO echo)
find_program(CAT cat)
find_program(GIT git)
find_program(LD ld)
find_program(CTAGS ctags)

#
# Enable 'make tags' target.
#
list(APPEND tagsExclude "--exclude=.git/*")
list(APPEND tagsExclude "--exclude=.pc/*")
list(APPEND tagsExclude "--exclude=patches/*")
add_custom_target(tags COMMAND ${CTAGS} -R ${tagsExclude} -f tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(ctags DEPENDS tags)

set(HEADER_FILES
	src/constants.h
	src/load.h
	src/msgpuck/msgpuck.h
	)
set(SOURCE_FILES
	src/main.c
	src/constants.c
	src/msgpuck/hints.c
	src/msgpuck/msgpuck.c
	)

add_executable (ttdump ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(ttdump zstd)
