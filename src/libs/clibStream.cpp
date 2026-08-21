#include "../lua.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include "../ltable.hpp"

void _sendToBuf(const char* a, uint64_t dataLen) {
    std::cout.write(a, dataLen);
    std::cout.write("\n", 1);
}

void _getBuffInput(char **to) {
    std::string buff;
    std::getline(std::cin, buff);
    char *sUB = new char[buff.size()];
    memcpy(sUB, buff.data(), buff.size());
    sUB[buff.size()] = '\0';
    *to = sUB;
}

Values _LibraryC__print(Values RDI, Values RSI, Values *Bunch) {
    void *ptr = lua_getPtr(RDI);
    if (lua_getVarType(RDI) == LuaString) {
        TString *a = (TString*)ptr;
        _sendToBuf(a->data, a->len);
    } else {
        std::string UA = std::to_string(RDI);
        _sendToBuf(UA.data(), UA.length());
    }
    return RDI;
}

Values _LibraryC__getUserInput(uint64_t a, uint64_t b, uint64_t c) {
    char *r;
    _getBuffInput(&r);
    TString *cmpF = returnCompiledString(std::string(r));
    return lua_makeVar(cmpF, LuaString);
}

void LIBC__InitializeStreamLibrary(lua_Table *toSave) {
    TString *print = returnCompiledString("print");
    TString *getUserInput = returnCompiledString("getUserInput");
    _F_ASM_NOTGUARANTEED_SETVALUE(toSave, print, lua_makeVar((void*)_LibraryC__print, LuaFunction));
    _F_ASM_NOTGUARANTEED_SETVALUE(toSave, getUserInput, lua_makeVar((void*)_LibraryC__getUserInput, LuaFunction));
}
