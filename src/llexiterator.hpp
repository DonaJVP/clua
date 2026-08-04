#include "lua.hpp"

#pragma once

// Makes _L_PATH
LuaLexFrame makeSinglePath(std::vector<LuaLexFrame> *keys, uint32_t *pos);

// Makes lua_Expression; A vector of vectors full of LuaLexFrame until _L_F_ARGS_END.
LuaLexFrame makeSingleExprP(std::vector<LuaLexFrame> *keys, uint32_t *pos);

// Makes minimized lua_Expression. Just a single vector full of LuaLexFrame until _L_ON_TO_GO_END.
LuaLexFrame makeSingleExprB(std::vector<LuaLexFrame> *keys, uint32_t *pos);
