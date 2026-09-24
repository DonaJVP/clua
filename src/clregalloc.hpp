#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "cljit.hpp"

#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>

typedef uint64_t GR_VALUE_data;

enum GR_Status: uint64_t {
    GR_Maintains = 0,
    GR_Modified = 1, // Should restore before use.
};

struct GeneralRegister {
    std::string name = "";
    std::unordered_map<std::string, GR_Status> status;
    asmjit::x86::Gp GR_ID = asmjit::x86::noReg;
    //_R_CONTENTS CONTENTS = _R_TRASHDATA;
    bool canOccupy = true;
    uint64_t uses = 0;
    uint64_t contentId = 0; // Used in HyperAssembly
    std::unordered_map<std::string, int32_t> savedPtrs;
};

// Register allocator
class CL_RegisterAllocator {
public:
    CL_RegisterAllocator(void *asm_);
    ~CL_RegisterAllocator();
    GeneralRegister *createGR(const std::string &name, bool nonExpansible = false, asmjit::x86::Gp _preferGp = asmjit::x86::noReg);
    bool destroyGR(const std::string &name, bool purge = false); // Does nothing, just puts it to discard table.
    void destroyGR(GeneralRegister *gp); // Does nothing, just puts it to discard table.
    GeneralRegister *searchRegByName(const std::string &name, bool onlyReadIfMemory = false);
    GeneralRegister *searchRegByNameX(const std::string &name);
    void emitCall();
    int32_t getBytesToAlloc() { return bytes; };
    void localsAllocated(int32_t b) { bytes = -b-40; }
    void occupyArgumentsRegisters(GeneralRegister *gp1, GeneralRegister *gp2, GeneralRegister *gp3);
    GeneralRegister *getRegisterRAW(asmjit::x86::Gp gp);
    int32_t getNextQwordNupdate();
    bool existsAtIndex(asmjit::x86::Gp &reg);
    void pushToKnownRegister(const std::string &name);
private:
    GeneralRegister *getFreeRegister();
    int32_t bytes = -40;
    GeneralRegister *getLeastUsedRegister(); // If overflows
    std::vector<GeneralRegister*> m_registers;
    std::unordered_map<std::string, GeneralRegister*> m_registersStatus;
    void *m_asm; // asmjit pointer.
};

asmjit::x86::Gp T(GeneralRegister *gp);
asmjit::x86::Gp S(const std::string &name, bool onlyReadIfMemory = false);
asmjit::x86::Gp SX(const std::string &name); // Only return used GP [does not restore]
asmjit::x86::Mem SM(const std::string &name, bool &status); // Only return memory operand, but show status if failed (Already in register.)
GeneralRegister *X(asmjit::x86::Gp &reg);

static void _CLRA__movWhenNeeded(GeneralRegister *gp0, GeneralRegister *gp1);

// RegisterAllocator
class CL_RegisterAllocator;
extern CL_RegisterAllocator *R;
extern asmjit::x86::Gp RR;
