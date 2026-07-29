cmake_minimum_required(VERSION 4.1)
project(lua CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(COMMON_CFLAGS)
set(COMMON_LDFLAGS)
set(LIBS)

option(BUILD_CLUA_EXE	"Build CLua executable" ON)
option(BUILD_CLUA_LIB	"Build CLua library" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(COMMON_LDFLAGS "${COMMON_LDFLAGS} -Wl,-E -lm")
	set(DEFAULT_DLOPEN ON)
endif()


if(LUA_USE_DLOPEN)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_DLOPEN")
	if(NOT APPLE)
		set(COMMON_LDFLAGS "${COMMON_LDFLAGS} -ldl ")
	endif(NOT APPLE)
endif(LUA_USE_DLOPEN)

if(DEFAULT_POSIX)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_USE_POSIX")
endif(DEFAULT_POSIX)

if(LUA_ANSI)
	set(COMMON_CFLAGS "${COMMON_CFLAGS} -DLUA_ANSI")
endif(LUA_ANSI)

# COMMON_CFLAGS has no effect without this line
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_CFLAGS}")
set(ASMJIT_STATIC TRUE CACHE BOOL "Build AsmJit static library")
add_subdirectory(lib/asmjit)

add_subdirectory(src build)


