
# Lua core source files.
set(LUA_CORE_SRC
	lOvars.cpp
	lVarControl.cpp
	lua_interpreter.cpp
	ltable.cpp
	lbundlefunc.cpp
	lapi.cpp
	lua_assembly.cpp
	l_eval.cpp
)
set(LUA_LIB_HEADERS
	lua.hpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${ASMJIT_INCLUDE_DIRS})



if(BUILD_CLUA_LIB)
add_library(lua STATIC ${LUA_CORE_SRC} ${ASMJIT_SRC})
target_link_libraries(lua ${LIBS} )
set(LUA_STATIC_LIB lua)
set(LUA_LIBS lua)
endif()

if(BUILD_CLUA_EXE)
set(LUA_TOTAL ${LUA_CORE_SRC} ${LUA_LIB_HEADERS})
add_executable(lua_interpreter ${LUA_TOTAL} ${ASMJIT_SRC})
target_link_libraries(lua_interpreter PRIVATE asmjit)
endif()




