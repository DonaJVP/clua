#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

struct lua_Table;

struct lua_ObjectT {
    lua_Table *data;
    std::string name;
    uintptr_t Pointer;
    std::unordered_map<std::string, uint64_t> *_funcs; // I want to save this local to a 8byte slot, not 32bytes
};

extern std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> ObjectFuncIds;
void _TEST__registerObject(lua_Table *gen); // TEST
