# Spring supplied CMake build file
cmake_minimum_required(VERSION 3.10)
project(LuaJITSpring CXX C)

set(LUAJIT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LUAJIT_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LUAJIT_OUTPUT ${LUAJIT_SRC_DIR}/libluajit.a)

add_custom_command(
    OUTPUT ${LUAJIT_OUTPUT}
    COMMAND $(MAKE)
            -C ${LUAJIT_SRC_DIR}
            BUILDMODE=static
            XCFLAGS=-DLUAJIT_SECURITY_PRNG=0\ -DLUAJIT_DISABLE_JIT
    WORKING_DIRECTORY ${LUAJIT_SRC_DIR}
    COMMENT "Building LuaJIT using its Makefile"
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
)
target_include_directories(luajit_spring PUBLIC
    ${LUAJIT_INC_DIR}
)

add_library(luajit INTERFACE)
target_link_libraries(luajit
    INTERFACE luajit_core
    INTERFACE luajit_spring
)
target_include_directories(luajit INTERFACE
    ${LUAJIT_INC_DIR}
)


add_executable(luajit-cli ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(luajit-cli PRIVATE luajit)

if (UNIX)
	SET_TARGET_PROPERTIES(luajit PROPERTIES COMPILE_FLAGS "-DLUA_USE_MKSTEMP ${PIC_FLAG}")
else (UNIX)
	SET_TARGET_PROPERTIES(luajit PROPERTIES COMPILE_FLAGS "${PIC_FLAG}")
endif (UNIX)
