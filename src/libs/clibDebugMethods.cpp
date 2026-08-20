// Yeet..

#include "../lua.hpp"
#include <cstdint>
#include <ctime>
#include <string>
#include <iostream>

uint64_t getNow() {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    uint64_t total = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
    return total;
}

static uint64_t time_ = 0;
static bool started = false;
static void calcTime(uint64_t a, uint64_t b, uint64_t c) {
    if (!started) {
        time_ = getNow();
        started = !started;
    } else {
        struct timespec now;
        timespec_get(&now, TIME_UTC);
        uint64_t total = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
        std::cout << "\nTime counted <nanoseconds>: " << std::to_string(total - time_) << std::endl;
        started = !started;
    }
}

void _UD2() {
    __asm__("ud2");
}

// Compile this function
//#ifdef DEBUG
void LIBC__InitializeDebugKit(lua_Table *g) {
    TString *CC = returnCompiledString("CC");
    TString *UD2 = returnCompiledString("ud2");
    _F_ASM_NOTGUARANTEED_SETVALUE(g, returnCompiledString("CC"), lua_makeVar(((void*)calcTime), LuaFunction));
    _F_ASM_NOTGUARANTEED_SETVALUE(g, UD2, lua_makeVar(((void*)_UD2), LuaFunction));
}
//#endif


