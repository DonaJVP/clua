#include "lua.hpp"
#include <vector>
#include <cstdint>

#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>

#pragma once

LuaLexFrame makeSingleTable(std::vector<LuaLexFrame> *vct, uint32_t *pos);
std::pair<bool, lua_Table*> _LTABLE_HELPER__buildTable(std::vector<LuaLexFrame> *vct, uint64_t argPtr0);
bool _canBuildTable(std::vector<LuaLexFrame> *vct);
asmjit::x86::Gp lua_genTable__Online(std::vector<LuaLexFrame> *vct, lua_Scope *scope, bool _constTable, lua_Table **tbl);
