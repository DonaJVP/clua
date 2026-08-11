
# Lua core source files.
set(LUA_CORE_SRC
	clapi.cpp
	cllex.cpp
	cllex_check.cpp
	cltable.cpp
	cleval.cpp
	cljit.cpp
	cloptimizer.cpp
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
#target_compile_options(lua_interpreter PRIVATE -fsanitize=address -fno-omit-frame-pointer)
#target_link_options(lua_interpreter PRIVATE -fsanitize=address)
target_link_libraries(lua_interpreter PRIVATE asmjit)
endif()




