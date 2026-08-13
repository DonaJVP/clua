// String librarie.
#include <cstdint>
#include "../lua.hpp"
#include <string>

// CLua String libraries.

// string.len(string)
Values _LibraryC__len(Values RDI, uint64_t a, uint64_t b) {
    LuaType t = lua_getVarType(RDI);
    if (t != LuaString) {
        m_LuaErrorHandler->reportWarning(_lua_es_InvalidUsage, 0, "Tried to get len() for unknown value");
        return 0x0;
    }
    TString *STR = (TString*)lua_getPtr(RDI);
    if (STR != nullptr) {
        Values aX = lua_makeVar(reinterpret_cast<uintptr_t*>(STR->len), LuaInteger);
        return aX;
    } else {
        return 0x0;
    }
}

// lower(string)
Values _LibraryC__lower(Values RDI, uint64_t a, uint64_t b) {
    LuaType t = lua_getVarType(RDI);
    if (t != LuaString) {
        m_LuaErrorHandler->reportWarning(_lua_es_InvalidUsage, 0, "Tried to get lower() for unknown value");
        return 0x0;
    }
    TString *STR = (TString*)lua_getPtr(RDI);
    if (STR) {
        //std::string literal = std::string(STR->data, STR->len);
        char *charset = new char[STR->len];
        for (uint64_t i = 0; i < STR->len; i++) {
            charset[i] = STR->data[i] - 0x14;
        }
        charset[STR->len] = '\0';
        free(STR->data);
        STR->data = charset;
        return lua_makeVar(STR, LuaString);
    } else {
        return 0x0;
    }
}

// upper(string)
Values _LibraryC__upper(Values RDI, uint64_t a, uint64_t b) {
    LuaType t = lua_getVarType(RDI);
    if (t != LuaString) {
        m_LuaErrorHandler->reportWarning(_lua_es_InvalidUsage, 0, "Tried to get upper() for unknown value");
        return 0x0;
    }
    TString *STR = (TString*)lua_getPtr(RDI);
    if (STR) {
        char *charset = new char[STR->len];
        for (uint64_t i = 0; i < STR->len; i++) {
            charset[i] = STR->data[i] + 0x14;
        }
        charset[STR->len] = '\0';
        free(STR->data);
        STR->data = charset;
        return lua_makeVar(STR, LuaString);
    } else {
        return 0x0;
    }
}

// reverse(string)
Values _LibraryC__reverse(Values RDI, uint64_t a, uint64_t b) {
    LuaType t = lua_getVarType(RDI);
    if (t != LuaString) {
        m_LuaErrorHandler->reportWarning(_lua_es_InvalidUsage, 0, "Tried to get reverse() for unknown value");
        return 0x0;
    }
    TString *STR = (TString*)lua_getPtr(RDI);
    if (STR) {
        char *charset = STR->data;
        uint64_t cI = 0;
        for (uint64_t i = STR->len-1; i > 0; i--) {
            charset[i] = STR->data[cI];
            cI++;
        }
        charset[STR->len] = '\0';
        free(STR->data);
        STR->data = charset;
        return lua_makeVar(STR, LuaString);
    } else {
        return 0x0;
    }
}


// Set add.
void LIBC__InitializeStringLibrary(lua_Table *toSave) {
    TString *stringT_S = returnCompiledString("stringT");
    lua_Table *stringT = new lua_Table();
    stringT->hmask = 0xFF;
    stringT->_BOOL_constTable = true; // Can't modify
    stringT->nodes = new Node[0xFF];
    _F_ASM_NOTGUARANTEED_SETVALUE(toSave, stringT_S, lua_makeVar((void*)stringT, LuaTable));
    TString *len = returnCompiledString("len");
    TString *lower = returnCompiledString("lower");
    TString *upper = returnCompiledString("upper");
    TString *reverse = returnCompiledString("reverse");
    _F_ASM_NOTGUARANTEED_SETVALUE(stringT, len, lua_makeVar((void*)_LibraryC__len, LuaFunction));
    _F_ASM_NOTGUARANTEED_SETVALUE(stringT, lower, lua_makeVar((void*)_LibraryC__lower, LuaFunction));
    _F_ASM_NOTGUARANTEED_SETVALUE(stringT, upper, lua_makeVar((void*)_LibraryC__upper, LuaFunction));
    _F_ASM_NOTGUARANTEED_SETVALUE(stringT, reverse, lua_makeVar((void*)_LibraryC__reverse, LuaFunction));
}
