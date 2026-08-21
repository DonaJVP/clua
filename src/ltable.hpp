#include "cllex.hpp"
#include <vector>
#include <cstdint>

#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>

#pragma once

typedef uint64_t Values;
struct Node;
class lua_Table;
struct TString;

LuaLexFrame makeSingleTable(std::vector<LuaLexFrame> *vct, uint32_t *pos);
std::pair<bool, lua_Table*> _LTABLE_HELPER__buildTable(std::vector<LuaLexFrame> *vct, uint64_t argPtr0);
bool _canBuildTable(std::vector<LuaLexFrame> *vct);
asmjit::x86::Gp lua_genTable__Online(std::vector<LuaLexFrame> *vct, lua_Scope *scope, bool _constTable, lua_Table **tbl);

extern Values _F_ASM_NOTGUARANTEED_GETVALUE(lua_Table *t, TString *k, Values nullPtr);
extern Node *_F_ASM_MAKETABLENREHASH(lua_Table *_T, uint32_t s_);
extern Values *_F_ASM_NOTGUARANTEED_SETVALUE(lua_Table *t, TString *key, Values v);
extern void *_F_ASM_NOTGUARANTEED_GETPTR(lua_Table *t, TString *k);
extern void *_F_ASM_NOTGUARANTEED_GETPTR_NOALLOC(lua_Table *t, TString *k, void *NPTR);
