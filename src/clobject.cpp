#include "lua.hpp"
#include "ltable.hpp"
#include <cstdint>
#include "clobject.hpp"

/*
 * This includes instructions lookup for lua objects.
 * Likely this:
 * object:set_hp(20) -- From Luanti
 // * ^-----------^
 *      \|/
 * [Static straight pointer.]
 * A lua_ObjectT is always a lua_Table with _BOOL_constTable set to true.
 */

// Rule inside CLua: Every object should remain known, even if they can't.

// ID: 0x7
// Mainclass: object

std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> ObjectFuncIds = std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>>();

// object.make(aTable) [[OBSOLETE]]
Values _LibraryC__straightTable__make(Values Name, Values TablePTR, uint64_t b) {
    LuaType typ = lua_getVarType(TablePTR);
    LuaType nameT = lua_getVarType(Name);
    if (nameT != LuaString) {
        m_LuaErrorHandler->reportError(_lua_es_InvalidType, 0, "Tried to seek other value <"+std::to_string(nameT)+"> rather than LuaString");
        return 0;
    }
    if (typ != LuaTable) {
        m_LuaErrorHandler->reportError(_lua_es_InvalidType, 0, "Tried to seek other value <"+std::to_string(typ)+"> rather than LuaTable");
        return 0;
    }
    lua_Table *tab = (lua_Table*)lua_getPtr(TablePTR);
    TString *str = (TString*)lua_getVarType(Name);
    lua_ObjectT *obj = new lua_ObjectT;
    obj->name = std::string(str->data, str->len);
    // initialize table set
    obj->data = tab;
    return lua_makeVar(obj, LuaObject);
}

lua_ObjectT *_LibraryCPP__make(const std::string name, std::unordered_map<std::string, uint64_t> functions, lua_Table *ptr) {
    // Make a object and then return it.
    lua_ObjectT *obj = new lua_ObjectT();
    obj->name = name;
    ObjectFuncIds[name] = std::move(functions);
    obj->_funcs = &ObjectFuncIds.at(name);
    if (ptr == nullptr) {
        obj->data = new lua_Table();
        lua_Table *LNK = obj->data;
        LNK->nodes = new Node[0xFF];
        LNK->hmask = 0xFF;
        LNK->array = new Values[0xFF];
        LNK->asize = 0xFF;
        LNK->_BOOL_constTable = true;
    } else {
        obj->data = ptr;
    }
    return obj;
}

// Every function inside a object should get the correct amount of arguments.
// As we go, first argument should had the object pointer.

// Section TEST.
#include <iostream>
Values _TEST__dowhatever(Values test, Values RSI, Values *RR) {
    TString *str = (TString*)lua_getPtr(test);
    TString *str2 = (TString*)lua_getPtr(RSI);
    std::cout << std::string(str->data, str->len) << " : " << std::string(str2->data, str2->len) << std::endl;
    return 0;
}

void _TEST__registerObject(lua_Table *gen) {
    // Make a single object, unique thing to do.
    // holy...
    TString *ye = returnCompiledString("thingToTest");
    TString *ye1 = returnCompiledString("HELLO THERE!");
    _F_ASM_NOTGUARANTEED_SETVALUE(gen, ye, lua_makeVar((void*)ye1, LuaString));
    // Now objects.
    std::unordered_map<std::string, uint64_t> _ENUM;
    _ENUM["dowhatever"] = (uint64_t)_TEST__dowhatever;
    ObjectFuncIds["testObj"] = _ENUM;
}

