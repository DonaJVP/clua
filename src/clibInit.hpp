#pragma once
#include "lua.hpp"

// In/Out library
void LIBC__InitializeStreamLibrary(lua_Table *toSave);
// String library
void LIBC__InitializeStringLibrary(lua_Table *toSave);
// Debug library
void LIBC__InitializeDebugKit(lua_Table *g);
