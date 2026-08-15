
set(CLUA_CORE_SRC
	clapi.cpp
	cllex.cpp
	cllex_check.cpp
	cltable.cpp
	cleval.cpp
	cljit.cpp
	cloptimizer.cpp
	clobject.cpp
)
set(CLUA_LIB_HEADERS
	lua.hpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${ASMJIT_INCLUDE_DIRS})
add_subdirectory(libs)

if(BUILD_CLUA_LIB)
add_library(clua STATIC ${CLUA_CORE_SRC} ${ASMJIT_SRC} ${CLUA_CORE_LIBS})
target_link_libraries(clua ${LIBS} )
set(CLUA_STATIC_LIB clua)
set(CLUA_LIBS clua)
endif()

if(BUILD_CLUA_EXE)
set(CLUA_TOTAL ${CLUA_CORE_SRC} ${CLUA_LIB_HEADERS} ${CLUA_CORE_LIBS})
add_executable(clua ${CLUA_TOTAL} ${ASMJIT_SRC})
#target_compile_options(clua PRIVATE -fsanitize=address -fno-omit-frame-pointer)
#target_link_options(clua PRIVATE -fsanitize=address)
target_link_libraries(clua PRIVATE asmjit)
endif()




