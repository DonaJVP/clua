// CLUA SIGSEGV, SIGILL handler.
#include "lua.hpp"
#include <cstdint>
#include <iomanip>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sstream>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <ucontext.h>

void crashHandler(int sig, siginfo_t *info, void *data) {
    ucontext_t *regs = static_cast<ucontext_t*>(data);
    // Always x86_64.
    uintptr_t IP = regs->uc_mcontext.gregs[REG_RIP];
    // Show next errs.
    m_LuaErrorHandler->reportError(sig == SIGSEGV ? _lua_es_segmentationfault : _lua_es_illegalinstruction, 0, "Crashed!");
    m_LuaErrorHandler->reportError(sig == SIGSEGV ? _lua_es_segmentationfault : _lua_es_illegalinstruction, 0, "Faulty <r>IP: "+std::to_string(IP));
    m_LuaErrorHandler->reportError(sig == SIGSEGV ? _lua_es_segmentationfault : _lua_es_illegalinstruction, 0, "Faulty address: "+std::to_string(reinterpret_cast<uintptr_t>(info->si_addr)));
    m_LuaErrorHandler->setFatal(true);
    // Print hexadecimal bytes.
    std::stringstream bytes;
    uint8_t *ptr = reinterpret_cast<uint8_t*>(IP-8);
    for (uint64_t i = 0; i < 24; i++) { // Show a range of -8 to 16 bytes
        if (i == 8) {
            bytes << "[";
        }
        bytes << std::setw(2) << std::setfill('0') << std::hex;
        bytes << static_cast<int>(ptr[i]);
        if (i == 8) {
            bytes << "] ";
        } else {
            bytes << " ";
        }
    }
    bytes << std::dec;
    m_LuaErrorHandler->reportError(sig == SIGSEGV ? _lua_es_segmentationfault : _lua_es_illegalinstruction, 0, bytes.str());
    // Reported some errors.
    // Must print meanwhile fifo []
    lua_ErrHandler *p = m_LuaErrorHandler->getPipe();
    std::cout << "\033[3;31m" << p->reason << "\033[0m" << std::endl;
    std::exit(sig);
}

void setCrashHandler() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crashHandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
}
