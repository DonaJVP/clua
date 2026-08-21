#pragma once
#include "cllex.hpp"
#include <cstdint>
#include <unordered_map>

struct TString;
struct lua_localSymbol;

// Bidirectional Opcode Manipulation
extern std::vector<lua_biOpCode> lua_B_F_OP(std::vector<LuaLexFrame> *Keys, uint32_t *pos, lua_Scope *bulldozer, bool _ONLYFUNC = false, bool _INSIDEAFUNC = false);

// Build function by BiOp
extern void *luaBundleFunction(std::vector<lua_biOpCode> *_CODE, lua_Scope *THREADRIPPER, bool _online_gen, void *F_MEM_UF, void *F_MEM_SCR, bool Script);

// Some of ASM..


enum _LUA_XMM_REGISTERS: uint8_t {
    xmmU = UINT8_MAX,
    xmm0 = 0,
    xmm1 = 1,
    xmm2 = 2,
    xmm3 = 3,
};

#include <sys/ucontext.h>
#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>

struct _REGISTER_;
struct _XREGISTER_;
typedef void(*_regCallback)(asmjit::x86::Assembler *a, _REGISTER_ *reg);
typedef void(*_regCallbackXMM)(asmjit::x86::Assembler *a, _XREGISTER_ *reg);

enum _R_CONTENTS: uint8_t {
    _R_TRASHDATA = 0,
    _R_FUNC_RESULT = 1,
    _R_AR_RESULT = 4,
    _R_CMP_RESULT = 5,
    _R_FUNC_ARGS = 6,
    _R_FUNC_ARGS_ENTRY = 7,
    _R_CLUATYPE_TAGGED = 2,
    _R_CLUATYPE_UNTAGGED = 3,
    _R_TABLE_POINTER = 8,
};
struct _REGISTER_ {
    greg_t rID = REG_ERR;
    uint64_t regVal = 0x0;
    uint64_t rData0;
    int32_t stackPtrBase = 0;
    uint64_t valType; // Untagged value should be in regVal [On practical zone, theorical=0]
    _R_CONTENTS cntId = _R_TRASHDATA;
    _regCallback onModified;
};
struct _XREGISTER_ {
    uint64_t regDataType = 0x0;
    _LUA_XMM_REGISTERS rID = xmmU;
    _R_CONTENTS cntId = _R_TRASHDATA;
    int32_t stackPtrBase = 0;
    void *rData0 = nullptr;
    _regCallbackXMM onModified;
};
typedef std::unordered_map<greg_t, _REGISTER_> RegistersDataGP;
typedef std::unordered_map<_LUA_XMM_REGISTERS, _XREGISTER_> RegistersDataXMM;
extern RegistersDataGP lua_Registers;
extern RegistersDataXMM lua_RegistersXMM;
extern void initializeRegistersData(void *asmPtr);
void _ASMH__rs_searchInTable(asmjit::x86::Gp tblPTR, std::pair<bool, std::pair<asmjit::x86::Gp, TString*>> key, asmjit::x86::Gp toGp, bool pointer = false);
void _HELPER__runHooksFor(asmjit::Reg rId_, _R_CONTENTS id);
inline void __ASM_callback_nothing_(asmjit::x86::Assembler *a, _REGISTER_ *reg) {}
inline void __ASM_callback_nothingX_(asmjit::x86::Assembler *a, _XREGISTER_ *reg) {}
asmjit::x86::Gp _ASMH__parseVarCacheRef(uint8_t r);
lua_localSymbol *searchSavedGeneralVars(const std::string id);
_LUA_XMM_REGISTERS _CPP_getXMMfromASM(asmjit::Reg rId);

asmjit::x86::Gp CLUA_EvalExprNReturn(std::vector<LuaLexFrame> *k, lua_Scope *scope, std::pair<bool, asmjit::x86::Gp> saveSpecificallyTo, bool getPointerInsteadofRawD = false, bool noTag = false, std::pair<uint32_t*, _Lua_Lex_Keys> middleCheck = {0, _L_NONE});
asmjit::x86::Gp _ASM__getPathToSelGp(std::vector<LuaLexFrame> *vct, asmjit::x86::Gp ret, lua_Scope *aSCP, bool pointer = false, bool preservRegister = false, bool shutCheck = false);
asmjit::x86::Gp _ASM__keyInstRestoreVar(asmjit::x86::Gp toVar);
void _F_ASM_MAKEFUNCTIONARGUMENTS(lua_Expression *Args, asmjit::x86::Assembler *a, lua_Scope *AS, bool give_stackptr, uint32_t stackptrsiz);
void _ASM__movToReg(asmjit::x86::Gp cR, asmjit::x86::Gp b);
extern asmjit::StringLogger qlog0;
extern int32_t stackRegCounter; // Starts from -40.
greg_t _CPP_getRegisterFromASM(asmjit::x86::Gp reg);
void _ASM_DEBUGGER_STOP();
void _lua_Table__initializeAssembler(asmjit::x86::Assembler *ptr);
