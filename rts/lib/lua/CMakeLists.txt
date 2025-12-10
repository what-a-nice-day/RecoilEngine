# Spring supplied CMake build file
cmake_minimum_required(VERSION 3.10)
project(LuaJITSpring CXX C)


set(LUAJIT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LUAJIT_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_program(MAKE_EXE make)
set(LUAJIT_BUILD_DIR ${CMAKE_BINARY_DIR}/luajit_build)
set(LUAJIT_OUTPUT ${LUAJIT_BUILD_DIR}/src/libluajit.a)

file(COPY ${LUAJIT_SRC_DIR}/ DESTINATION ${LUAJIT_BUILD_DIR})

# DLUAJIT_DISABLE_JIT
add_custom_command(
    OUTPUT ${LUAJIT_OUTPUT}
    COMMAND ${MAKE_EXE}
            -C ${LUAJIT_BUILD_DIR}
            BUILDMODE=static
            CC=gcc-13
            "XCFLAGS=-DLUAJIT_SECURITY_PRNG=0 -DLUAJIT_SECURITY_STRHASH=0 -fPIC -DPIC"
    WORKING_DIRECTORY ${LUAJIT_BUILD_DIR}
    COMMENT "Building LuaJIT using its Makefile"
    VERBATIM
)
add_custom_target(build_luajit ALL
    DEPENDS ${LUAJIT_OUTPUT}
)
add_library(luajit_core STATIC IMPORTED GLOBAL)
set_target_properties(luajit_core PROPERTIES
    IMPORTED_LOCATION ${LUAJIT_OUTPUT}
    INTERFACE_INCLUDE_DIRECTORIES ${LUAJIT_INC_DIR}
)

add_dependencies(luajit_core build_luajit)

add_library(luajit_spring STATIC
    ${LUAJIT_INC_DIR}/LuaUser.cpp
    ${LUAJIT_INC_DIR}/lua_priviledges.cpp
)
target_include_directories(luajit_spring PUBLIC
    ${LUAJIT_INC_DIR}
)

add_library(lua INTERFACE)
target_include_directories(lua INTERFACE
    ${LUAJIT_INC_DIR}       
)
target_link_libraries(lua
    INTERFACE luajit_core
    INTERFACE luajit_spring
)


if (UNIX)
	SET_TARGET_PROPERTIES(lua PROPERTIES COMPILE_FLAGS "-DLUA_USE_MKSTEMP ${PIC_FLAG}")
else (UNIX)
	SET_TARGET_PROPERTIES(lua PROPERTIES COMPILE_FLAGS "${PIC_FLAG}")
endif (UNIX)
