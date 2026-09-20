#include "clregalloc.hpp"
#include "lua.hpp"
#include "ltable.hpp"
#include <asmjit/x86/x86operand.h>
#include "cljit.hpp"
#include <cstddef>
#include <cstdint>
#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>
#include <stdexcept>
#include <sys/ucontext.h>
#include "clobject.hpp"

using namespace asmjit;

static _REGISTER_ *clogReg = nullptr;
int32_t stackRegCounter = 0; // Starts from -40.
RegistersDataGP lua_Registers = RegistersDataGP();
RegistersDataXMM lua_RegistersXMM = RegistersDataXMM();
static bool _xmmUsed = false;

// This calculates with "high precision" about contents in the registers. Eval.

void nothing(x86::Assembler *a, _REGISTER_ *reg) {}
void nothingX(x86::Assembler *a, _XREGISTER_ *reg) {}

void initializeRegistersData(void *asmPtr) {
    // Registers between r12 - r15 are banned to be used in a normal routime.
    // General purpose registers.
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RAX, _REGISTER_{REG_RAX,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RSI, _REGISTER_{REG_RSI,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RCX, _REGISTER_{REG_RCX,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RDI, _REGISTER_{REG_RDI,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RDX, _REGISTER_{REG_RDX,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R10, _REGISTER_{REG_R10,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R11, _REGISTER_{REG_R11,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R12, _REGISTER_{REG_R12,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R13, _REGISTER_{REG_R13,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R14, _REGISTER_{REG_R14,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R15, _REGISTER_{REG_R15,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R8, _REGISTER_{REG_R8,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R9, _REGISTER_{REG_R9,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    //lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R10, _REGISTER_{REG_R10,0x0,nullptr,LuaUnknown,_R_TRASHDATA}));
    //lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R11, _REGISTER_{REG_R11,0x0,nullptr,LuaUnknown,_R_TRASHDATA}));
    //lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RBX, _REGISTER_{_R_TRASHDATA,REG_RBX,0x0,nullptr,LuaUnknown}));
    // XMM registers
    lua_RegistersXMM.insert(std::pair<_LUA_XMM_REGISTERS, _XREGISTER_>(xmm0, _XREGISTER_{0x0,xmm0,_R_TRASHDATA,0,nullptr,nothingX}));
    lua_RegistersXMM.insert(std::pair<_LUA_XMM_REGISTERS, _XREGISTER_>(xmm1, _XREGISTER_{0x0,xmm1,_R_TRASHDATA,0,nullptr,nothingX}));
    lua_RegistersXMM.insert(std::pair<_LUA_XMM_REGISTERS, _XREGISTER_>(xmm2, _XREGISTER_{0x0,xmm2,_R_TRASHDATA,0,nullptr,nothingX}));
    lua_RegistersXMM.insert(std::pair<_LUA_XMM_REGISTERS, _XREGISTER_>(xmm3, _XREGISTER_{0x0,xmm3,_R_TRASHDATA,0,nullptr,nothingX}));
    // Save assembler.
    a = (x86::Builder*)asmPtr;
}

enum _LUATYPES_64BITS: uint64_t {
    L_ASM_Func = 0x0006000000000000ULL,
    L_ASM_Numb = 0x0009000000000000ULL,
    L_ASM_Inte = 0x0008000000000000ULL,
    L_ASM_Stri = 0x0004000000000000ULL,
    L_ASM_Obje = 0x0007000000000000ULL,
    L_ASM_Tabl = 0x0005000000000000ULL,
    L_ASM_Nil0 = 0x0001000000000000ULL,
    L_ASM_Bool = 0x0003000000000000ULL,
    L_ASM_Unkn = 0x0002000000000000ULL,
};

enum errCodes: uint8_t {
    notFunction = 0,
};

//GEM DETECTED

greg_t _CPP_getRegisterFromASM(x86::Gp reg) {
    if (reg == x86::rax) {
        return REG_RAX;
    } else if (reg == x86::rcx) {
        return REG_RCX;
    } else if (reg == x86::rdx) {
        return REG_RDX;
    } else if (reg == x86::rdi) {
        return REG_RDI;
    } else if (reg == x86::rsi) {
        return REG_RSI;
    } else if (reg == x86::r9) {
        return REG_R9;
    } else if (reg == x86::r8) {
        return REG_R8;
    } else if (reg == x86::r10) {
        return REG_R10;
    } else if (reg == x86::r11) {
        return REG_R11;
    }
    return 0;
}

void _CPPH_setRegisterStatus(x86::Gp reg, _R_CONTENTS cnt) {
    if (lua_Registers.find(_CPP_getRegisterFromASM(reg)) == lua_Registers.end()) 
        return;
    lua_Registers.at(_CPP_getRegisterFromASM(reg)).cntId = cnt;
}

void _CPP__turnRegistersAfterCall() {
    lua_Registers.at(REG_RDI).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RSI).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RCX).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RDX).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RAX).cntId = _R_FUNC_RESULT;
    lua_Registers.at(REG_R10).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_R11).cntId = _R_TRASHDATA;
    //lua_Registers.at(REG_R12).cntId = _R_TRASHDATA;
    //lua_Registers.at(REG_R13).cntId = _R_TRASHDATA;
    //lua_Registers.at(REG_R14).cntId = _R_TRASHDATA;
    //lua_Registers.at(REG_R15).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_R8).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_R9).cntId = _R_TRASHDATA;
    // GUH
    lua_RegistersXMM.at(xmm0).cntId = _R_TRASHDATA;
    lua_RegistersXMM.at(xmm1).cntId = _R_TRASHDATA;
    lua_RegistersXMM.at(xmm2).cntId = _R_TRASHDATA;
    lua_RegistersXMM.at(xmm3).cntId = _R_TRASHDATA;
}

struct _Warning {
    uint64_t m0;
    uint64_t m2;
    uint64_t m3;
    uint64_t m4;
};
#include <deque>
static std::deque<_Warning> queue;

std::vector<std::vector<LuaLexFrame>> _CPP__insertToFirstPosition(std::vector<LuaLexFrame> toPush, lua_Expression *expr) {
    std::vector<std::vector<LuaLexFrame>> *e = (std::vector<std::vector<LuaLexFrame>>*)expr;
    std::vector<std::vector<LuaLexFrame>> toC;
    toC.push_back(toPush);
    for (std::vector<LuaLexFrame> &VCT: *e) {
        toC.push_back(VCT);
    }
    //toC.insert(toC.cend(), e->begin(), e->end());
    return toC;
}

x86::Gp _CPP__getGeneralPurposeRegister(greg_t reg) {
    switch (reg) {
        case REG_RAX: {
            return x86::rax;
        }
        case REG_RSI: {
            return x86::rsi;
        }
        case REG_RCX: {
            return x86::rcx;
        }
        case REG_RDI: {
            return x86::rdi;
        }
        case REG_RDX: {
            return x86::rdx;
        }
        case REG_R10: {
            return x86::r10;
        }
    }
    return x86::noReg;
}

void _ASM__crash(lua_ErrSignals errCode) {
    m_LuaErrorHandler->reportError(errCode, 0, "During execution: Not a function.");
}

void _ASM_DEBUGGER_STOP() {
    //a->mov(x86::rax, 34);
    //a->syscall();
}

void _ASM__movToReg(x86::Gp cR, x86::Gp b) {
    if (cR == b)
        return;
    a->mov(cR, b);
}

void _ASM__untagReg(x86::Gp cR) {
    greg_t _r = _CPP_getRegisterFromASM(cR);
    if (_r != 0) {
        lua_Registers.at(_r).cntId = _R_CLUATYPE_UNTAGGED;
    }
    a->mov(x86::r11, (uint64_t)PTR_MASK);
    a->and_(cR, x86::r11);
}

void _ASM__insertNull(x86::Gp toGp) {
    a->mov(toGp, 0);
}

x86::Gp _ASM__verifyRegisterIfType(_REGISTER_ *reg, LuaType T) {
    if (reg->valType == T) {
        return x86::noReg;
    } else {
        return x86::di; // meh.
    }
}

void _ASM_CLBK__saveRcxRegisterIfModified() {
    if (clogReg->cntId != _R_TRASHDATA) {
        clogReg->cntId = _R_TRASHDATA;
        // Save in stack.
        clogReg->stackPtrBase = -40+stackRegCounter;
        stackRegCounter -= 8;
        a->mov(x86::qword_ptr(x86::rbp, clogReg->stackPtrBase), x86::rcx);
    }
}

void _ASM_CLBK__saveFirstXMMregister() {
    if (lua_RegistersXMM.at(xmm0).cntId != _R_TRASHDATA) {
        lua_RegistersXMM.at(xmm0).cntId = _R_TRASHDATA;
        // Save in stack.
        lua_RegistersXMM.at(xmm0).stackPtrBase = -40+stackRegCounter;
        stackRegCounter -= 8;
        a->movd(x86::r8, x86::xmm0);
        a->mov(x86::qword_ptr(x86::rbp, lua_RegistersXMM.at(xmm0).stackPtrBase), x86::r8);
    }
}

uint64_t _ASMH__makeCoolTypeCMPROR(LuaType type) {
    // Pass a strong and cool uinteger just to compare with the TAG_MASK
    uint64_t base = 0x7FF0000000000000ULL | type << 48;
    return base;
}

x86::Gp _ASM__cmpVarType(x86::Gp base, LuaType toType) {
    uint64_t tCMP = _ASMH__makeCoolTypeCMPROR(toType);
    uint64_t tCMP_ = 0xFFFF000000000000ULL;
    GeneralRegister *t0 = R->createGR("000");
    GeneralRegister *t1 = R->createGR("001");
    a->mov(S("000"), base);
    a->mov(S("001"), tCMP_);
    a->and_(S("000"), S("001"));
    a->mov(S("001"), tCMP);
    a->cmp(S("000"), S("001"));
    R->destroyGR(t0);
    R->destroyGR(t1);
    return base;
}

void _ASM__getFromTableIndex(uint8_t mode, const std::string &regNameTable, uint64_t baseTable1, const std::string &regCounter, uint32_t regCounter0, const std::string &regNameRet, bool getPtr = false) {
    if (mode == 1) { // Registers based
        Label _of = a->new_label();
        Label _end = a->new_label();
        GeneralRegister *helperTmpReg = R->createGR("_ASM__getFromTableIndex::multiplierReg");
        GeneralRegister *raxReg = R->createGR("_ASM__getFromTableIndex::raxReg", false, x86::rax);
        a->dec(S(regCounter));
        a->mov(x86::r11, x86::qword_ptr(S(regNameTable), offsetof(lua_Table, used_on_amap)));
        a->cmp(S(regCounter), x86::r11);
        a->jae(_of);
        a->mov(x86::r11, x86::qword_ptr(S(regNameTable), offsetof(lua_Table, array)));
        a->mov(S("_ASM__getFromTableIndex::multiplierReg"), 0x8);
        a->movzx(S("_ASM__getFromTableIndex::raxReg"), S(regCounter));
        a->mul(S("_ASM__getFromTableIndex::multiplierReg"));
        if (!getPtr)
            a->mov(S(regNameRet), x86::qword_ptr(x86::r11, x86::rax));
        else
            a->lea(S(regNameRet), x86::qword_ptr(x86::r11, x86::rax));
        a->jmp(_end);
        a->bind(_of);
        a->mov(S(regNameRet), 0);
        a->bind(_end);
    } else {
        // meth.
        Label _of = a->new_label();
        Label _end = a->new_label();
        a->movabs(x86::r10, baseTable1);
        a->mov(x86::r8, regCounter0);
        a->mov(x86::r11, x86::qword_ptr(x86::r10, offsetof(lua_Table, used_on_amap)));
        a->cmp(x86::r8, x86::r11); // Inserted as an imm32
        a->jae(_of);
        a->mov(x86::r11, x86::qword_ptr(S(regNameTable), offsetof(lua_Table, array)));
        a->mov(x86::r10, regCounter0*8);
        if (!getPtr)
            a->mov(S(regNameRet), x86::qword_ptr(x86::r11, x86::r10));
        else
            a->lea(S(regNameRet), x86::qword_ptr(x86::r11, x86::r10));
        a->jmp(_end);
        a->bind(_of);
        a->mov(S(regNameRet), 0);
        a->bind(_end);
    }
}

void _ASMH__doLuaNumberToInteger(x86::Gp reg) {
    // FPU?
    // Nope.
    a->movd(x86::xmm0, reg);
    a->cvttsd2si(reg, x86::xmm0);
}

_REGISTER_ *_ASM__callFunc(_REGISTER_ *reg) {
    x86::Gp occupied_reg = _ASM__verifyRegisterIfType(reg, LuaFunction);
    if (occupied_reg == x86::noReg) {
        _ASM_CLBK__saveRcxRegisterIfModified();
        a->call(_CPP__getGeneralPurposeRegister(reg->rID));
        _CPP__turnRegistersAfterCall();
    } else { // Must compare.
        Label itIsFunc = a->new_label();
        _ASM_CLBK__saveRcxRegisterIfModified();
        _ASM__movToReg(x86::r10, _CPP__getGeneralPurposeRegister(reg->rID));
        // Compare.
        a->mov(x86::r11, (uint64_t)0x000F000000000000ULL);
        a->and_(x86::r10, x86::r11);
        a->mov(x86::r11, (uint64_t)L_ASM_Func);
        a->cmp(x86::r10, x86::r11);
        a->je(itIsFunc);
        a->mov(x86::rdi, notFunction);
        a->call((uint64_t)_ASM__crash);
        a->bind(itIsFunc);
        a->call(_CPP__getGeneralPurposeRegister(reg->rID));
        _CPP__turnRegistersAfterCall();
    }
    return reg;
}

void _ASM__keyInstSaveVarOnEnv(x86::Gp varToSave) {
    //greg_t _r = _CPP_getRegisterFromASM(varToSave);
    // rcx is a register which can be used or use stack pos [40 - 128, 8+ allocations.]
    clogReg = &lua_Registers.at(REG_RCX);
    clogReg->cntId = _R_CLUATYPE_TAGGED;
    _ASM_DEBUGGER_STOP();
    _ASM__movToReg(x86::rcx, varToSave);
}

x86::Gp _ASMH__parseVarCacheRef(uint8_t r) {
    x86::Gp toReg = x86::noReg;
    switch (r) {
        case 1: {
            toReg = x86::r12;
            break;
        }
        case 2: {
            toReg = x86::r13;
            break;
        }
        case 3: {
            toReg = x86::r14;
            break;
        }
        case 4: {
            toReg = x86::r15;
            break;
        }
    }
    return toReg;
}

static Node *alloc_node() {
    return new Node();
}

// PRC0
void _ASM__booleanProc(x86::Gp p0, x86::Gp p1, _Lua_Lex_Keys op) {
    
}

// MATH PROCCESORS
void _ASM__vectorialProc(x86::Vec p0, x86::Vec p1, _Lua_Lex_Keys op) {
    switch (op) {
        case _L_SYNTAX_DEC: {
            a->subsd(p0, p1);
            break;
        }
        case _L_SYNTAX_SUM: {
            a->addsd(p0, p1);
            break;
        }
        case _L_SYNTAX_DIV: {
            a->divsd(p0, p1);
            break;
        }
        case _L_SYNTAX_MUL: {
            a->mulsd(p0, p1);
            break;
        }
        default: {
            m_LuaErrorHandler->reportError(_lua_es_NonFunction, 0, std::string("Unknown key: " + std::to_string(static_cast<int>(op))));
            break;
        }
    }
}
void _ASM__GPR_Proc(x86::Gp p0, x86::Gp p1, _Lua_Lex_Keys op) {
    switch (op) {
        case _L_SYNTAX_DEC: {
            a->sub(p0, p1);
            break;
        }
        case _L_SYNTAX_SUM: {
            a->add(p0, p1);
            break;
        }
        case _L_SYNTAX_DIV: {
            _ASM__movToReg(x86::rax, p1);
            a->idiv(p0);
            _ASM__movToReg(p0, x86::rax);
            break;
        }
        case _L_SYNTAX_MUL: {
            _ASM__movToReg(x86::rax, p1);
            a->imul(p0);
            _ASM__movToReg(p0, x86::rax);
            break;
        }
        default: {
            m_LuaErrorHandler->reportError(_lua_es_NonFunction, 0, std::string("Unknown key: " + std::to_string(static_cast<int>(op))));
            break;
        }
    }
}

#include <iostream>

static char* drt(uint8_t s, const char *t1, TString *k) {
    std::string h1 = std::string(k->data, k->len);
    char *map = (char*)malloc(s + k->len);
    memcpy(map, t1, s);
    memcpy(&map[s], k->data, k->len);
    return map;
}

extern "C" {
    void __attribute__((naked)) _CPP_P_ASM_searchInTable() {
        __asm__ (
            ".intel_syntax noprefix\n\t"
            
            "xor eax, eax\n\t"
            "inc eax\n\t"
            
            // CRITICAL: You must manually manage the exit.
            // If the caller JUMPED here, you must JUMP back or use a manual ret/iret.
            "ret\n\t" 
            
            ".att_syntax\n\t"
        );
    }
}

void _ASMH__rs_searchInTable(x86::Gp X, std::pair<bool, std::pair<x86::Gp, TString*>> key, x86::Gp toGp, bool pointer) {
    
    qlog0._log2("start::searchInTable\n", 21);
    // DEBUG SECTION.
    if (pointer)
        qlog0._log2("#_SAVE_# for variable[");
    else
        qlog0._log2("#_LOAD_# for variable[");
    if (key.second.second != nullptr) {
        qlog0._log2(key.second.second->data, key.second.second->len);
        qlog0._log2("; ");
        qlog0._log2(std::to_string(key.second.second->IDX).c_str());
    } else {
        qlog0._log2("<register>");
    }
    qlog0._log2("]\n");
    Label _nf = a->new_label();
    Label _f = a->new_label();
    Label _loop = a->new_label();
    //Label _insert = a->new_named_label("insert");
    Label _end = a->new_label();
    
    GeneralRegister *slot_TMP0 = R->createGR("?RSI__SIT");
    GeneralRegister *keyContainer = R->createGR("kContainer__SIT");
    GeneralRegister *keyData = R->createGR("key__SIT");
    GeneralRegister *IC__keyRawPointer = nullptr;
    
    // r8 = hash
    if (key.first) {
        a->mov(S("kContainer__SIT"), key.second.first);
    } else {
        a->mov(S("kContainer__SIT"), key.second.second->IDX);
    }
    
    
    //Adjustment.
    //a->mov(x86::r11, x86::r8);
    
    if (!pointer)
        a->mov(x86::rdi, x86::r8);
    else {
        IC__keyRawPointer = R->createGR("kRaw__SIT");
        a->mov(S("kRaw__SIT"), (uint64_t)key.second.second);
        a->mov(S("key__SIT"), x86::qword_ptr(S("kRaw__SIT")));
    }
    
    
    a->and_(S("kContainer__SIT"), x86::qword_ptr(X, offsetof(lua_Table, hmask)));
    a->mov(S("?RSI__SIT"), x86::qword_ptr(X, offsetof(lua_Table, nodes)));
    a->shl(S("kContainer__SIT"), 5);
    //a->add(x86::rsi, x86::r8);
    a->bind(_loop);
    a->mov(x86::r11, x86::qword_ptr(S("?RSI__SIT"), x86::r8)); // r11 = nodes[r8]
    a->test(x86::r11, x86::r11);
    a->jz(_nf);
    a->cmp(S("key__SIT"), x86::qword_ptr(x86::r11));
    a->je(_f);
    // next
    a->add(S("kContainer__SIT"), offsetof(Node, next));
    a->mov(S("kContainer__SIT"), x86::qword_ptr(S("?RSI__SIT"), x86::r8));
    a->test(S("?RSI__SIT"), S("?RSI__SIT"));
    a->jz(_nf);
    // Modify rsi to handle each other object.
    a->xor_(S("kContainer__SIT"), S("kContainer__SIT"));
    a->jmp(_loop);
    a->bind(_nf);
    
    if (!pointer) {
        a->mov(toGp, 0);
        a->jmp(_end);
    } else {
        GeneralRegister *ARG0 = R->createGR("rsi00", false, x86::rsi);
        GeneralRegister *ARG1 = R->createGR("rdi00", false, x86::rdi);
        GeneralRegister *ARG2 = R->createGR("rdx00", false, x86::rdx);
        a->mov(S("rsi00"), S("key__SIT"));
        a->mov(S("rdi00"), X);
        a->xor_(S("rdx00"), S("rdx00"));
        R->emitCall();
        R->destroyGR("rsi00");
        R->destroyGR("rdi00");
        R->destroyGR("rdx00");
        a->call((uint64_t)_F_ASM_NOTGUARANTEED_SETVALUE);
        a->mov(toGp, x86::rax);
        a->jmp(_end);
    }
    // =========================
    // FOUND
    // =========================
    a->bind(_f);
    if (!pointer) {
        a->add(S("kContainer__SIT"), offsetof(Node, val));
        a->mov(toGp, x86::qword_ptr(S("kContainer__SIT"), S("kContainer__SIT")));
    } else {
        a->add(S("kContainer__SIT"), offsetof(Node, val));
        a->lea(toGp, x86::qword_ptr(S("kContainer__SIT"), S("kContainer__SIT")));
    }
    //endzone:
    a->bind(_end);
    qlog0._log2("end::searchInTable\n", 19);
    // DEBUG SECTION.
    if (pointer)
        qlog0._log2("END; #_SAVE_# for variable[");
    else
        qlog0._log2("END; #_LOAD_# for variable[");
    if (key.second.second != nullptr) {
        qlog0._log2(key.second.second->data, key.second.second->len);
    } else {
        qlog0._log2("<register>");
    }
    qlog0._log2("]\n");
    // Clear registers..
    R->destroyGR(slot_TMP0);
    R->destroyGR(keyContainer);
    R->destroyGR(keyData);
    if (IC__keyRawPointer != nullptr)
        R->destroyGR(IC__keyRawPointer);
    
}

void _ASMH__rs_searchInTable(const std::string &kT_ST, std::pair<bool, std::pair<x86::Gp, TString*>> key, const std::string &toGp, bool pointer) {
    qlog0._log2("start::searchInTable\n", 21);
    // DEBUG SECTION.
    if (pointer)
        qlog0._log2("#_SAVE_# for variable[");
    else
        qlog0._log2("#_LOAD_# for variable[");
    if (key.second.second != nullptr) {
        qlog0._log2(key.second.second->data, key.second.second->len);
        qlog0._log2("; ");
        qlog0._log2(std::to_string(key.second.second->IDX).c_str());
    } else {
        qlog0._log2("<register>");
    }
    qlog0._log2("]\n");
    Label _nf = a->new_label();
    Label _f = a->new_label();
    Label _loop = a->new_label();
    //Label _insert = a->new_named_label("insert");
    Label _end = a->new_label();
    
    GeneralRegister *slot_TMP0 = R->createGR("?RSI__SIT");
    GeneralRegister *keyContainer = R->createGR("kContainer__SIT");
    GeneralRegister *keyData = R->createGR("key__SIT");
    GeneralRegister *IC__keyRawPointer = nullptr;
    
    // r8 = hash
    if (key.first) {
        a->mov(S("kContainer__SIT"), key.second.first);
    } else {
        a->mov(S("kContainer__SIT"), key.second.second->IDX);
    }
    
    if (!pointer) {
        a->mov(S("key__SIT"), S("kContainer__SIT"));
    } else {
        IC__keyRawPointer = R->createGR("kRaw__SIT");
        a->mov(S("kRaw__SIT"), (uint64_t)key.second.second);
        a->mov(S("key__SIT"), x86::qword_ptr(S("kRaw__SIT")));
    }
    
    a->and_(S("kContainer__SIT"), x86::qword_ptr(S(kT_ST), offsetof(lua_Table, hmask)));
    a->mov(S("?RSI__SIT"), x86::qword_ptr(S(kT_ST), offsetof(lua_Table, nodes)));
    a->shl(S("kContainer__SIT"), 5);
    //a->add(x86::rsi, x86::r8);
    a->bind(_loop);
    a->mov(x86::r11, x86::qword_ptr(S("?RSI__SIT"), S("kContainer__SIT"))); // r11 = nodes[r8]
    a->test(x86::r11, x86::r11);
    a->jz(_nf);
    a->cmp(S("key__SIT"), x86::qword_ptr(x86::r11));
    a->je(_f);
    // next
    a->add(S("kContainer__SIT"), offsetof(Node, next));
    a->mov(S("kContainer__SIT"), x86::qword_ptr(S("?RSI__SIT"), S("kContainer__SIT")));
    a->test(S("?RSI__SIT"), S("?RSI__SIT"));
    a->jz(_nf);
    // Modify rsi to handle each other object.
    a->xor_(S("kContainer__SIT"), S("kContainer__SIT"));
    a->jmp(_loop);
    a->bind(_nf);
    
    if (!pointer) {
        a->mov(SX(toGp), 0);
        a->jmp(_end);
    } else {
        GeneralRegister *ARG0 = R->createGR("rsi00", false, x86::rsi);
        GeneralRegister *ARG1 = R->createGR("rdi00", false, x86::rdi);
        GeneralRegister *ARG2 = R->createGR("rdx00", false, x86::rdx);
        a->mov(S("rsi00"), S("kRaw__SIT"));
        R->destroyGR("kRaw__SIT");
        IC__keyRawPointer = nullptr;
        a->mov(S("rdi00"), S(kT_ST));
        a->xor_(S("rdx00"), S("rdx00"));
        R->emitCall();
        a->call((uint64_t)_F_ASM_NOTGUARANTEED_SETVALUE);
        R->destroyGR("rsi00");
        R->destroyGR("rdi00");
        R->destroyGR("rdx00");
        a->mov(SX(toGp), x86::rax);
        a->jmp(_end);
    }
    // =========================
    // FOUND
    // =========================
    a->bind(_f);
    if (!pointer) {
        a->add(S("kContainer__SIT"), offsetof(Node, val));
        a->mov(SX(toGp), x86::qword_ptr(S("?RSI__SIT"), S("kContainer__SIT")));
    } else {
        a->add(S("kContainer__SIT"), offsetof(Node, val));
        a->lea(SX(toGp), x86::qword_ptr(S("?RSI__SIT"), S("kContainer__SIT")));
    }
    //endzone:
    a->bind(_end);
    qlog0._log2("]\n");
    // Clear registers..
    R->destroyGR("?RSI__SIT");
    R->destroyGR("kContainer__SIT");
    R->destroyGR("key__SIT");
    if (IC__keyRawPointer != nullptr)
        R->destroyGR("kRaw__SIT");
}

uint64_t _CLEVAL_ASM_GCFNG = 0;

void _ASM__getContentsFromMGENERAL(TString *key, const std::string &toGp, bool modify = false) {
    std::string rName = "tablePointer"+std::to_string(_CLEVAL_ASM_GCFNG);
    GeneralRegister *tblPtr = R->createGR(rName);
    _CLEVAL_ASM_GCFNG++;
    a->movabs(S(rName), (uint64_t)m_General);
    _ASMH__rs_searchInTable(rName, std::pair<bool, std::pair<x86::Gp, TString*>>(false, std::pair<x86::Gp, TString*>(x86::noReg, key)), toGp, modify);
    R->destroyGR(rName);
}

/*
 * Modifying value reference.
 * 0 = No modifications required
 * 1 = Stands for own register modifications, no need to recall [Likely integers and numbers]
 * 2 = Needed direct pointer access
 */


// first=Register, second=High performance local register used
std::pair<x86::Gp, bool> _ASM__searchSymbolToUse(const std::string &toGp, TString *sym, lua_Scope *actScope, uint32_t toModify) {
    std::string symSTR = std::string(sym->data, sym->len);
    // First, search if it are saved on high speed vars (General variables)
    lua_localSymbol *s_ = searchSavedGeneralVars(symSTR);
    if (s_ != nullptr) {
        if (s_->cacheReg > 0 && !toModify) {
            return {_ASMH__parseVarCacheRef(s_->cacheReg), true};
        }
    }
    bool q0 = false;
    x86::Gp _R = S(toGp);
    if (actScope->symbols.find(symSTR) != actScope->symbols.end()) {
        // Get it and return to toGp
        lua_localSymbol *s = &actScope->symbols.at(symSTR);
        if (s->cacheReg > 0) {
            if (toModify) {
                q0 = true;
                _R = _ASMH__parseVarCacheRef(s->cacheReg);
                goto _nocacheregistry;
            }
            return {_ASMH__parseVarCacheRef(s->cacheReg), true};
        } else {
            _nocacheregistry:
            if (toModify < 2 || s->rawdata == 1) {
                if (s->availReg != "") {
                    return {S(s->availReg), true};
                }
            }
            if (!_0_0_0_CMPTIME_ASM_isScript) {
                int32_t sK = -0;
                sK -= (s->slot);
                if (!toModify)
                    a->mov(S(toGp), x86::qword_ptr(x86::rbp, sK));
                else
                    a->lea(S(toGp), x86::qword_ptr(x86::rbp, sK));
            } else {
                GeneralRegister *tmp0 = R->createGR("scriptMemoryF");
                a->movabs(T(tmp0), (uint64_t)_0_0_0_CMPTIME_ASM_scriptMem);
                if (!toModify)
                    a->mov(S(toGp), x86::qword_ptr(T(tmp0), s->slot));
                else
                    a->lea(S(toGp), x86::qword_ptr(T(tmp0), s->slot));
                R->destroyGR(tmp0);
            }
            if (!q0)
                return {S(toGp), false};
            else
                return {_R, q0};
        }
    } else {
        // Find in the rScopes
        // If not found in the rScopes, get it from the m_General.
        bool found = false;
        lua_Scope *rSCP = actScope->rSCOPE;
        while (true) {
            if (rSCP != nullptr) {
                if (rSCP->symbols.find(symSTR) == rSCP->symbols.end()) {
                    rSCP = rSCP->rSCOPE;
                    continue;
                } else {
                    // Get it and return to toGp
                    lua_localSymbol *s = &rSCP->symbols.at(symSTR);
                    if (s->cacheReg > 0) {
                        if (toModify) {
                            q0 = true;
                            _R = _ASMH__parseVarCacheRef(s->cacheReg);
                            goto _nocacheregistryP2;
                        }
                        return {_ASMH__parseVarCacheRef(s->cacheReg), true};
                    } else {
                        _nocacheregistryP2:
                        if (toModify < 2 || s->rawdata == 1) {
                            if (s->availReg != "") {
                                return {S(s->availReg), true};
                            }
                        }
                        if (!_0_0_0_CMPTIME_ASM_isScript) {
                            int32_t sK = -0;
                            sK -= (s->slot);
                            if (!toModify)
                                a->mov(S(toGp), x86::qword_ptr(x86::rbp, sK));
                            else
                                a->lea(S(toGp), x86::qword_ptr(x86::rbp, sK));
                        } else {
                            GeneralRegister *tmp0 = R->createGR("scriptMemoryF");
                            a->movabs(T(tmp0), (uint64_t)_0_0_0_CMPTIME_ASM_scriptMem);
                            if (!toModify)
                                a->mov(S(toGp), x86::qword_ptr(T(tmp0), s->slot));
                            else
                                a->lea(S(toGp), x86::qword_ptr(T(tmp0), s->slot));
                            R->destroyGR(tmp0);
                        }
                        if (!q0)
                            return {S(toGp), false};
                        else
                            return {_R, q0};
                    }
                }
            } else {
                break;
            }
        }
        if (!found) {
            // Notify if something is modified [or mod]
            _KFINISH:
            _ASM__getContentsFromMGENERAL(sym, toGp, toModify);
            return {_R, q0};
        }
    }
}

/*
 * case 9: {
 *   // Uhhuh.
 *   a->lea(x86::rsi, x86::qword_ptr(x86::rbp, -496));
 *   a->mov(x86::rdi, x86::qword_ptr(x86::rdi, var.slot));
 *   break;
 } *
 */

Values __ASM_F_STRINGMANIPULATOR_CONCAT2(TString *a, TString *b) {
    //TString *a, TString *b
    //TString *a = returnIndexOfStringPTR(_a);
    //TString *b = returnIndexOfStringPTR(_b);
    //TString *a = (TString*)lua_getPtr(*a0);
    //TString *b = (TString*)lua_getPtr(*b0);
    char *res = new char[a->len+b->len];
    uint32_t final_res_len;
    memcpy(res, a->data, a->len); // a->len == std::string(a).size()
    final_res_len = a->len;
    memcpy(&res[final_res_len], b->data, b->len);
    final_res_len += b->len;
    TString *OBJ = new TString();
    OBJ->IDX = stringTable.size()+1;
    OBJ->data = res;
    OBJ->len = final_res_len;
    stringTable[std::string(OBJ->data)] = *OBJ;
    return lua_makeVar(OBJ, LuaString); //More portable usage.
}

void _ASM__crashT() {
    a->call((uint64_t)abort);
}

void _CPP__setcntId(Reg id, _R_CONTENTS rQ) {
    if (id.is_vec128()) {
        lua_RegistersXMM.at(_CPP_getXMMfromASM(id)).cntId = rQ;
    } else {
        lua_Registers.at(_CPP_getRegisterFromASM(x86::Gp::make_r64(id.id()))).cntId = rQ;
    }
}
_R_CONTENTS _CPP__getcntId(Reg id) {
    if (id.is_vec128()) {
        return lua_RegistersXMM.at(_CPP_getXMMfromASM(id)).cntId;
    } else {
        return lua_Registers.at(_CPP_getRegisterFromASM(x86::Gp::make_r64(id.id()))).cntId;
    }
}

bool _CPP__existsMoreOnWay(std::vector<LuaLexFrame> *vct, uint32_t pos) {
    try {
        if (vct->at(pos+1).key == _L_OVERALLTYPECHECKER) {
            return false;
        } else {
            return true;
        }
    } catch (std::out_of_range &e) {
        return false;
    }
    return true;
}

x86::Vec _CPP__getVecRegister(_LUA_XMM_REGISTERS xmm) {
    switch (xmm) {
        case xmm0: {
            return x86::xmm0;
        }
        case xmm1: {
            return x86::xmm1;
        }
        case xmm2: {
            return x86::xmm2;
        }
        case xmm3: {
            return x86::xmm3;
        }
        case xmmU: {
            return x86::xmm10;
        }
    }
    return x86::xmm0;
}

x86::Gp _ASM__runOpCode__checkArithmeticRegister__tInteger(x86::Gp reg) {
    Label _B_isInteger = a->new_label();
    Label _B_crashZone = a->new_label();
    _ASM__cmpVarType(reg, LuaInteger);
    a->je(_B_isInteger);
    _ASM__cmpVarType(reg, LuaNumber);
    a->jne(_B_crashZone);
    a->movd(x86::xmm0, reg);
    a->cvttsd2si(reg, x86::xmm0);
    a->jmp(_B_isInteger);
    a->bind(_B_crashZone);
    a->mov(x86::rdi, _lua_es_InvalidType);
    a->call((uint64_t)_ASM__crash);
    a->bind(_B_isInteger);
    return reg;
}

Reg _ASM__runOpCode(uint64_t rawOpcode, Reg op0r, Reg op1r, bool check0 = true, bool check1 = true) {
    // Opcode should had info about which subtype should it be...
    uint64_t opcode = rawOpcode & 0x00000000000000FFULL;
    uint64_t mode = (rawOpcode >> 4) & 0x00000000000000FFULL;
    int oQ = static_cast<int>(opcode);
    // Math.
    if (oQ > 39 && oQ < 46) {
        if (op0r.is_vec128() || op1r.is_vec128()) {
            // Maybe vectors would work.
            x86::Vec x0;
            x86::Vec x1;
            if (op0r.is_gp64()) {
                // Transform.
                x0 = x86::xmm0;
                x86::Gp op0 = x86::Gp::make_r64(op0r.id());
                if (check1) {
                    Label _isNumber = a->new_label();
                    Label _iCrash = a->new_label();
                    _ASM__cmpVarType(op0, LuaNumber);
                    a->je(_isNumber);
                    // If not, transform it.
                    _ASM__cmpVarType(op0, LuaInteger);
                    a->jne(_iCrash);
                    GeneralRegister *tmp0 = R->createGR("_ASM__runOpCode::tmpValue");
                    _ASM__movToReg(S("_ASM__runOpCode::tmpValue"), op0);
                    a->mov(x86::r11, (uint64_t)0x0000FFFFFFFFFFFFULL);
                    a->and_(S("_ASM__runOpCode::tmpValue"), x86::r11); // Remember r11 is a scratchpad
                    a->cvtsi2sd(x0, S("_ASM__runOpCode::tmpValue"));
                    R->destroyGR(tmp0);
                    a->jmp(_isNumber);
                    a->bind(_iCrash);
                    a->mov(x86::rdi, _lua_es_InvalidType);
                    a->call((uint64_t)_ASM__crash);
                    a->bind(_isNumber);
                }
            } else {
                x0 = x86::Vec::make_v128(op0r.id());
            }
            if (op1r.is_gp64()) {
                x1 = x86::xmm1;
                x86::Gp op1 = x86::Gp::make_r64(op1r.id());
                if (check1) {
                    Label _isNumber = a->new_label();
                    Label _iCrash = a->new_label();
                    _ASM__cmpVarType(op1, LuaNumber);
                    a->je(_isNumber);
                    // If not, transform it.
                    _ASM__cmpVarType(op1, LuaInteger);
                    a->jne(_iCrash);
                    GeneralRegister *tmp0 = R->createGR("_ASM__runOpCode::tmpValue0");
                    _ASM__movToReg(S("_ASM__runOpCode::tmpValue0"), op1);
                    a->mov(x86::r11, (uint64_t)0x0000FFFFFFFFFFFFULL);
                    a->and_(S("_ASM__runOpCode::tmpValue0"), x86::r11);
                    a->cvtsi2sd(x1, S("_ASM__runOpCode::tmpValue0"));
                    R->destroyGR(tmp0);
                    a->jmp(_isNumber);
                    a->bind(_iCrash);
                    a->mov(x86::rdi, _lua_es_InvalidType);
                    a->call((uint64_t)_ASM__crash);
                    a->bind(_isNumber);
                }
            } else {
                x1 = x86::Vec::make_v128(op1r.id());
            }
            //x86::Vec x0 = x86::Vec::make_v128(op0r.id());
            //x86::Vec x1 = x86::Vec::make_v128(op1r.id());
            _ASM__vectorialProc(x0, x1, static_cast<_Lua_Lex_Keys>(opcode));
            return x0;
        } else {
            x86::Gp op0 = x86::Gp::make_r64(op0r.id());
            x86::Gp op1 = x86::Gp::make_r64(op1r.id());
            if (check0)
                _ASM__runOpCode__checkArithmeticRegister__tInteger(op0);
            if (check1)
                _ASM__runOpCode__checkArithmeticRegister__tInteger(op1);
            _ASM__GPR_Proc(op0, op1, static_cast<_Lua_Lex_Keys>(opcode));
        }
        return x86::Gp::make_r64(op0r.id());
    } else if (oQ == 15 || oQ == 21) {
        x86::Gp op0 = x86::Gp::make_r64(op0r.id());
        x86::Gp op1 = x86::Gp::make_r64(op1r.id());
        switch (opcode) {
            case _L_OR: {
                //a->or_(op0, op0, op1); // Returns corrupted data when both has data.
                Label _jmpIfZero = a->new_label();
                a->test(op0, op0);
                a->jz(_jmpIfZero);
                a->mov(op0, op1);
                a->bind(_jmpIfZero);
                return op0;
            }
            case _L_NOT: {
                /// !!! NOT USED!
                return op0;
            }
            default: {}
        }
    } else if (oQ > 28 && oQ < 34) { // STR/BOOL
        x86::Gp op0 = x86::Gp::make_r64(op0r.id());
        x86::Gp op1 = x86::Gp::make_r64(op1r.id());
        GeneralRegister *scratchPad0 = R->createGR("_ASM__runOpCode::scrathpad0");
        switch (opcode) {
            // Boolean cases.
            case _L_EQUALS: {
                a->cmp(op0, op1);
                a->setz(op0);
                R->destroyGR(scratchPad0);
                return op0;
            }
            case _L_EQUALS_OR_MORE: {
                a->cmp(op0, op1);
                a->setc(op0);
                R->destroyGR(scratchPad0);
                return op0;
            }
            case _L_EQUALS_OR_MINUS: {
                a->xor_(S("_ASM__runOpCode::scrathpad0"), S("_ASM__runOpCode::scrathpad0"));
                a->cmp(op0, op1);
                a->setc(S("_ASM__runOpCode::scrathpad0"));
                a->setz(op0);
                a->or_(op0, S("_ASM__runOpCode::scrathpad0"));
                R->destroyGR(scratchPad0);
                return op0;
            }
            case _L_DOESNT_EQUALS: {
                a->cmp(op0, op1);
                a->setz(op0);
                a->not_(op0);
                R->destroyGR(scratchPad0);
                return op0;
            }
            // String cases
            case _L_CONCAT: {
                R->destroyGR(scratchPad0); // Not needed, at least, for now.
                // Concat requires external calling..
                qlog0._log2("start::concat()\n", 16);
                Label noString;
                Label end = a->new_label();
                
                //GeneralRegister *op_0 = R->createGR("op0");
                //GeneralRegister *op_1 = R->createGR("op1");
                
                //_ASM__movToReg(SX("op1"), op1);
                //_ASM__movToReg(SX("op0"), op0);
                // Unmask
                a->mov(x86::r11, (uint64_t)0x0000FFFFFFFFFFFFULL);
                a->and_(op0, x86::r11);
                a->and_(op1, x86::r11);
                
                a->mov(T(R->createGR("rdi", false, x86::rdi)), op0);
                a->mov(T(R->createGR("rsi", false, x86::rsi)), op1);
                
                R->emitCall();
                a->call((uint64_t)__ASM_F_STRINGMANIPULATOR_CONCAT2);
                a->jmp(end);
                if (check0)
                    a->bind(noString);
                a->mov(x86::rax, 0);
                a->bind(end);
                qlog0._log2("end::concat()\n", 14);
                //R->destroyGR(op_0);
                //R->destroyGR(op_1);
                a->mov(op0, x86::rax);
                return x86::Gp::make_r64(op0r.id());
            }
            default: {
                m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, "Unknown resources.");
                _ASM__insertNull(op0);
                break;
            }
        }
    }
    return x86::Gp::make_r64(op0r.id());
}

std::pair<bool, uint8_t> _CPP__emittedAnyOpcode(_Lua_Lex_Keys c) {
    int oQ = static_cast<int>(c);
    if (oQ > 39 && oQ < 46) {
        return {true, 1}; // Math operators.
    }
    if (oQ > 28 && oQ < 34) {
        return {true, 2}; // String/Booleans operators.
    }
    if (oQ == 15 || oQ == 21) {
        return {true, 3};
    }
    return {false, 0};
}

void _ASMH__CRH(lua_ErrSignals signal, uint64_t symDlog) {
    m_LuaErrorHandler->reportError(signal, symDlog, "");
}

void _ASM_crashINSTR(lua_ErrSignals signal, uint64_t symDlog = 0) {
    a->mov(x86::rdi, signal);
    a->mov(x86::rsi, symDlog);
    a->call((uint64_t)_ASMH__CRH);
    a->leave();
    a->ret();
}

//x86::Gp CLUA_EvalExprNReturn(std::vector<LuaLexFrame> *k, lua_Scope *scope, std::pair<bool, x86::Gp> saveSpecificallyTo, bool getPointerInsteadofRawD = false);

bool _areThereNextValuesToGet(std::vector<LuaLexFrame> *vct, uint64_t pos) {
    try {
        LuaLexFrame &thing = vct->at(pos+1);
        return true;
    } catch (std::out_of_range &e) {
        return false;
    }
}

x86::Gp _ASM__getPathToSelGp(std::vector<LuaLexFrame> *vct, const std::string &RN, lua_Scope *aSCP, bool pointer, bool preservRegister, bool shutCheck) {
    bool gotFirst = false;
    bool continuity = false;
    LuaLexFrame *actual = nullptr;
    uint64_t pos = 0;
    
    uint64_t superMode = 0;
    
    bool highSpeedValue = false;
    x86::Gp HSV_register = x86::noReg;
    
    while (true) {
        try {
            actual = &vct->at(pos);
        } catch (std::out_of_range &e) {
            R->pushToKnownRegister(RN);
            return S(RN);
        }
        switch (actual->key) {
            case _L_VARNAME: {
                if (!gotFirst) {
                    std::pair<x86::Gp, bool> res = _ASM__searchSymbolToUse(RN, (TString*)actual->a, aSCP, pointer && !_areThereNextValuesToGet(vct, pos)); //for func args pointer should be false.
                    if (res.second) {
                        if (res.first.id() < 12) { // Do not overwrite
                            _ASM__movToReg(S(RN), res.first);
                        } else {
                            if (!pointer) {
                                if (!preservRegister) {
                                    _ASM__movToReg(S(RN), res.first);
                                } else {
                                    _ASM__movToReg(res.first, res.first); // Useless call but good.
                                    highSpeedValue = true;
                                    HSV_register = res.first;
                                }
                            }
                        }
                    }
                    gotFirst = true;
                } else {
                    if (!continuity) {
                        m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)actual->debugSymbolLine, "Expected '.' to continue table searching but found nothing.");
                        _ASM__insertNull(S(RN));
                        goto _RET;
                    } else {
                        // Search in table!
                        // Verify table
                        // get content in table
                        // continue.
                        // base = act.
                        if (!pointer) {
                            Label _f = a->new_label();
                            Label _f1 = a->new_label();
                            _ASM__cmpVarType(S(RN), LuaTable);
                            a->jne(_f);
                            if (!highSpeedValue) {
                                a->mov(x86::r11, 0x0000FFFFFFFFFFFFULL);
                                a->and_(S(RN), x86::r11);
                            }
                            _ASMH__rs_searchInTable(highSpeedValue ? HSV_register : S(RN), std::pair<bool, std::pair<x86::Gp, TString*>>(false, std::pair<x86::Gp, TString*>(x86::noReg, (TString*)actual->a)), S(RN), false);
                            a->jmp(_f1);
                            a->bind(_f);
                            _ASM_crashINSTR(_lua_es_UnknownDataIdx);
                            a->mov(S(RN), 0);
                            a->bind(_f1);
                        } else {
                            Label _f = a->new_label();
                            Label _f1 = a->new_label();
                            _ASM__cmpVarType(S(RN), LuaTable);
                            a->jne(_f);
                            //Search for this value.
                            if (!highSpeedValue) {
                                a->mov(x86::r11, 0x0000FFFFFFFFFFFFULL);
                                a->and_(S(RN), x86::r11);
                            }
                            _ASMH__rs_searchInTable(highSpeedValue ? HSV_register : S(RN), std::pair<bool, std::pair<x86::Gp, TString*>>(false, std::pair<x86::Gp, TString*>(x86::noReg, (TString*)actual->a)), S(RN), true);
                            a->jmp(_f1);
                            a->bind(_f);
                            _ASM_crashINSTR(_lua_es_UnknownDataIdx);
                            a->mov(S(RN), 0);
                            a->bind(_f1);
                        }
                    }
                } 
                break;
            }
            case _L_ON_TO_GO: {
                if (actual->ATTRIB == 0) { // non brkt
                    continuity = true;
                } else if (actual->ATTRIB == 0xFF) {
                    // Object calling.
                    continuity = true;
                } else {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, "Unexpected reading!");
                    _ASM__insertNull(S(RN));
                }
                break;
            }
            case _L_INT: {
                // A supermode, must respect their value for EXPR BRACKET.
                superMode = static_cast<uint64_t>(actual->key);
                break;
            }
            case _L_EXPRESSION_BRKT: {
                // Save ret just for later [Access!]
                uint64_t baseMem = (uint64_t)malloc(8);
                GeneralRegister *savingK = R->createGR("storage0");
                if (pointer) {
                    // Should restore their table poitner state rather than being a slot pointer.
                    a->mov(S(RN), x86::qword_ptr(S(RN)));
                }
                a->movabs(x86::r11, 0x0000FFFFFFFFFFFFULL);
                a->and_(S(RN), x86::r11);
                a->mov(S("storage0"), S(RN));
                GeneralRegister *result = R->createGR("ExpressionBracketResult");
                bool HSV_LOCAL = true;
                //x86::Gp ret0;
                std::tuple<bool, x86::Gp, const std::string> K = CLUA_EvalExprNReturn(&actual->EXPR_BRKT, aSCP, std::pair<bool, const std::string>(false, "ExpressionBracketResult"), false);
                auto [way, ret0, regName] = K;
                if (ret0.id() > 11) {
                    // Must delete that register if not used..
                    R->destroyGR(result);
                    HSV_LOCAL = true;
                }
                // Eval.
                Label _f;
                Label number__;
                Label _f2;
                Label _f3;
                if (HSV_LOCAL) {
                    // High speed variables. Only integers allowed for now.
                    GeneralRegister *tracked = R->createGR("storage1");
                    a->mov(S("storage1"), ret0);
                    if (superMode == _L_INT) {
                        _ASM__getFromTableIndex(1, RN, 0x0, "storage1", 0x0, RN, pointer);
                    } else if (superMode == _L_DOUBLE) {
                        // Transform to Integer..
                        _ASMH__doLuaNumberToInteger(S("storage1"));
                        _ASM__getFromTableIndex(1, RN, 0x0, "storage1", 0x0, RN, pointer);
                    } else if (superMode == _L_STRING2) {
                        _ASMH__rs_searchInTable("storage0", std::pair<bool, std::pair<x86::Gp, TString*>>(true, std::pair<x86::Gp, TString*>(ret0, nullptr)), RN);
                    }
                    R->destroyGR(tracked);
                    goto _IGNVARCHECK;
                }
                _VARCHECK:
                if (superMode > 0) {
                    // Maybe respect this...
                    a->mov(x86::r11, 0x0000FFFFFFFFFFFFULL);
                    a->and_(ret0, x86::r11);
                    if (superMode == _L_INT) {
                        _ASM__getFromTableIndex(1, RN, 0x0, R->getRegisterRAW(ret0)->name, 0x0, RN, pointer);
                    } else if (superMode == _L_STRING2) {
                        _ASMH__rs_searchInTable("storage0", std::pair<bool, std::pair<x86::Gp, TString*>>(true, std::pair<x86::Gp, TString*>(ret0, nullptr)), RN);
                    } else if (superMode == _L_DOUBLE) {
                        // Transform to Integer..
                        _ASMH__doLuaNumberToInteger(S("storage1"));
                        _ASM__getFromTableIndex(1, RN, 0x0, "storage1", 0x0, RN, pointer);
                    }
                } else {
                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, "Must define double, int or stringT for table searching");
                    m_LuaErrorHandler->setFatal(true);
                    a->ud2();
                }
                _IGNVARCHECK:
                break;
            }
            default: {
                //what
            }
        }
        pos++;
    }
    _RET:
    return S(RN);
}

std::pair<bool, uint8_t> _CPP__areThereOpInstruction(std::vector<LuaLexFrame> *l, uint32_t pos) {
    try {
        LuaLexFrame qID = l->at(pos);
        return {qID.key != _L_OVERALLTYPECHECKER, _CPP__emittedAnyOpcode(qID.key).second};
    } catch (std::out_of_range &e) {
        return {false, 0};
    }
}

static bool _savedRET_0 = false;
void _SaveRDX__000(x86::Assembler *b, _REGISTER_ *reg) {
    a->mov(x86::qword_ptr(x86::rbp, -64), x86::rdx);
}
void _SaveRET__000(x86::Assembler *b, _REGISTER_ *reg) {
    if (reg->cntId != _R_CLUATYPE_TAGGED)
        return;
    if (reg->rID == 0) {
        m_LuaErrorHandler->reportWarning(_lua_es_FutureCrashAtRuntime, 0, "reg->rID detected as null!");
        return;
    }
    a->mov(x86::qword_ptr(x86::rbp, -72), _CPP__getGeneralPurposeRegister(reg->rID));
    _savedRET_0 = true;
}
void _SaveRET__00X(x86::Assembler *b, _XREGISTER_ *reg) {
    if (reg->cntId != _R_CLUATYPE_TAGGED)
        return;
    a->movq(x86::qword_ptr(x86::rbp, -72), _CPP__getVecRegister(reg->rID));
    _savedRET_0 = true;
}

uint64_t cCounter0 = 0;

// CLUA's main function for eval. Returns true with a string when it are saved to normal GPs but in r12..15 registers, returns false with that
std::tuple<bool, x86::Gp, const std::string> CLUA_EvalExprNReturn(std::vector<LuaLexFrame> *k, lua_Scope *scope, std::pair<bool, const std::string> saveSpecificallyTo, bool getPointerInsteadofRawD, bool noTag, std::pair<uint32_t*, _Lua_Lex_Keys> middleCheck) {
    LuaLexFrame *pointer = nullptr;
    bool _usePointer = false;
    if (middleCheck.first != nullptr)
        _usePointer = true;
    _Lua_Lex_Keys stopAt = middleCheck.second;
    uint32_t pos = _usePointer ? *middleCheck.first : 0;
    GeneralRegister *toRet0 = nullptr;
    x86::Gp _toRet;
    Reg ret;
    std::string RET_NAME = "toReturn";
    RET_NAME.append(std::to_string(cCounter0));
    cCounter0++;
    if (!saveSpecificallyTo.first) {
        toRet0 = R->createGR(RET_NAME);
        _toRet = S(RET_NAME);
        ret = _toRet;
    } else {
        toRet0 = R->searchRegByName(saveSpecificallyTo.second);
        RET_NAME = saveSpecificallyTo.second;
        _toRet = S(saveSpecificallyTo.second);
        ret = _toRet;
    }
    x86::Gp DATA_CONTAINER = x86::noReg;
    bool _setReg = false;
    x86::Gp q0 = x86::noReg;
    _Lua_Lex_Keys _OPMODE = static_cast<_Lua_Lex_Keys>(0);
    _Lua_Lex_Keys _TYPE = static_cast<_Lua_Lex_Keys>(0);
    bool _notOpCode = false;
    uint8_t tEmittedCode = 0;
    bool _presearched_query = false;
    bool _use_ret_reg = false;
    bool _savedToXMMreg = false;
    bool _quotient_r = false;
    bool _hasTypePath = false;
    LuaLexFrame *doomPath0 = nullptr;
    LuaLexFrame _CODENAME(_L_NONE);
    uint8_t lType0 = 0;
    while (true) {
        try {
            pointer = &k->at(pos);
        } catch (std::out_of_range &e) {
            queue.clear();
            if (_usePointer)
                *middleCheck.first = pos;
            if (_toRet.id() != ret.id() || ret.is_vec128()) {
                // Unequal ids nor vector.
                if (_toRet.is_vec128()) {
                    a->movq(_toRet, x86::Vec::make_v128(ret.id()));
                } else {
                    if (ret.id() < 12 || getPointerInsteadofRawD) {
                        a->mov(_toRet, x86::Gp::make_r64(ret.id()));
                    } else {
                        R->destroyGR(RET_NAME); // Not used..
                        return {false, _toRet, ""};
                    }
                }
            }
            if (DATA_CONTAINER != x86::noReg) {
                a->mov(S(RET_NAME), DATA_CONTAINER);
            }
            return {true, _toRet, RET_NAME};
        }
        if (stopAt == pointer->key) {
            if (_usePointer)
                *middleCheck.first = pos;
            if (_toRet.id() != ret.id() || ret.is_vec128()) {
                // Unequal ids nor vector.
                if (_toRet.is_vec128()) {
                    a->movq(_toRet, x86::Vec::make_v128(ret.id()));
                } else {
                    if (ret.id() < 12 || getPointerInsteadofRawD) {
                        a->mov(_toRet, x86::Gp::make_r64(ret.id()));
                    } else {
                        R->destroyGR(RET_NAME); // Not used..
                        return {false, _toRet, ""};
                    }
                }
            }
            if (DATA_CONTAINER != x86::noReg) {
                a->mov(S(RET_NAME), DATA_CONTAINER);
            }
            return {true, _toRet, RET_NAME};
        }
        if (pointer->key == _L_OVERALLTYPECHECKER) {
            pos++;
            continue;
        }
        std::pair<bool, uint8_t> eao = _CPP__emittedAnyOpcode(pointer->key);
        if (eao.first) {
            _OPMODE = static_cast<_Lua_Lex_Keys>(_TYPE | pointer->key);
            tEmittedCode = eao.second;
            pos++;
            _quotient_r=true;
            _presearched_query = false;
            continue;
        }
        if (pos != 0) {
            if (ret.is_vec128()) {
                lua_RegistersXMM.at(_CPP_getXMMfromASM(ret)).cntId = _R_CLUATYPE_TAGGED; // Value is an value.
                lua_RegistersXMM.at(_CPP_getXMMfromASM(ret)).onModified = _SaveRET__00X;
            } else {
                lua_Registers.at(_CPP_getRegisterFromASM(x86::Gp::make_r64(ret.id()))).cntId = _R_CLUATYPE_TAGGED; // Value is an value.
                lua_Registers.at(_CPP_getRegisterFromASM(x86::Gp::make_r64(ret.id()))).onModified = _SaveRET__000;
            }
        }
        switch (pointer->key) {
            case _L_OBJECTCODENAME: {
                _CODENAME = *pointer;
                break;
            }
            // Expression case.
            case _L_EXPRESSION: {
                std::tuple<bool, x86::Gp, const std::string> K = CLUA_EvalExprNReturn(&pointer->EXPR.at(0), scope, std::pair<bool, const std::string>(_OPMODE == _L_NONE, _OPMODE == _L_NONE ? RET_NAME : ""), getPointerInsteadofRawD, noTag);
                auto [way, regRaw, regName] = K;
                if (_OPMODE != _L_NONE) {
                    if (way) {
                        Reg _ret =  _ASM__runOpCode(_OPMODE, DATA_CONTAINER == x86::noReg ? S(RET_NAME) : DATA_CONTAINER, S(regName), true, false);
                        if (DATA_CONTAINER != x86::noReg)
                            a->mov(S(RET_NAME), DATA_CONTAINER);
                        _OPMODE = _L_NONE;
                        R->destroyGR(regName);
                    } else {
                        Reg _ret =  _ASM__runOpCode(_OPMODE, DATA_CONTAINER == x86::noReg ? S(RET_NAME) : DATA_CONTAINER, regRaw, true, false);
                        if (DATA_CONTAINER != x86::noReg)
                            a->mov(S(RET_NAME), DATA_CONTAINER);
                        _OPMODE = _L_NONE;
                    }
                } else {
                    // High speed variable?
                    if (regRaw.id() > 11) {
                        Reg _ret =  _ASM__runOpCode(_OPMODE, DATA_CONTAINER == x86::noReg ? S(RET_NAME) : DATA_CONTAINER, regRaw, true, false);
                        if (DATA_CONTAINER != x86::noReg)
                            a->mov(S(RET_NAME), DATA_CONTAINER);
                        _OPMODE = _L_NONE;
                    }
                }
                break;
            }
            case _L_DOUBLE: {
                uint64_t K = LuaNumber;
                _TYPE = static_cast<_Lua_Lex_Keys>(K << 4); // Move 4bytes
                break;
            }
            case _L_INT: {
                uint64_t K = LuaInteger;
                _TYPE = static_cast<_Lua_Lex_Keys>(K << 4);
                break;
            }
            case _L_STRING2: {
                uint64_t K = LuaString;
                _TYPE = static_cast<_Lua_Lex_Keys>(K << 4);
                break;
            }
            case _L_NONE: {
                a->xor_(x86::Gp::make_r64(ret.id()), x86::Gp::make_r64(ret.id()));
                return {true, x86::Gp::make_r64(ret.id()), RET_NAME};
            }
            case _L_NIL: {
                a->xor_(x86::Gp::make_r64(ret.id()), x86::Gp::make_r64(ret.id()));
                break;
            }
            case _L_TABLE: {
                if (ret.is_vec128()) {
                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, "Vec128[double precision value] can't be converted into a General Purpose Reg. [table]");
                    m_LuaErrorHandler->setFatal(true);
                    _ASM__crashT();
                    return {false, x86::noReg, ""};
                }
                std::pair<bool, lua_Table*> _res = _LTABLE_HELPER__buildTable(&pointer->EXPR_BRKT, (uint64_t)scope);
                if (!_res.first) {
                    // Generate it via asm (online)
                    std::string regist = lua_genTable__Online(&pointer->EXPR_BRKT, scope, pointer->ATTRIB, nullptr);
                    _ASM__movToReg(x86::Gp::make_r64(ret.id()), S(regist));
                    R->destroyGR(regist);
                    if (!noTag) {
                        R->createGR("TMP0");
                        a->mov(S("TMP0"), (uint64_t)0x7FF5000000000000ULL);
                        a->or_(x86::Gp::make_r64(ret.id()), S("TMP0"));
                        R->destroyGR("TMP0");
                    }
                } else {
                    // Generated in compile time.
                    Values val = noTag ? (uint64_t)_res.second : lua_makeVar(_res.second, LuaTable);
                    a->mov(x86::Gp::make_r64(ret.id()), val);
                }
                break;
            }
            case _L_NOT: {
                _notOpCode = !_notOpCode;
                break;
            }
            case _L_CALL: {
                if (pointer->skipcheck) {
                    _CLHASM__buildArgs(pointer->EXPR, scope);
                    a->call(lua_getPtr(*(Values*)pointer->a));
                } else {
                    
                    if (_CODENAME.key != _L_NONE) {
                        // Get name of the table reference.
                        std::string str = std::string(_CODENAME._data.begin(),_CODENAME._data.end());
                        // Proceed.
                        if (ObjectFuncIds.find(str) == ObjectFuncIds.end()) {
                            m_LuaErrorHandler->reportError(_lua_es_NotCorrect, 0, "Invalid object name: "+str);
                            m_LuaErrorHandler->reportWarning(_lua_es_UnknownDataIdx, 0, "Skipping object execution.");
                        } else {
                            std::unordered_map<std::string, uint64_t> *_TABLE = &ObjectFuncIds.at(str);
                            // Get the first term.
                            if (pointer->ATTRIB && !pointer->addr->needToResolveAddr()) {
                                uint64_t ptr = 0;
                                std::string cfName = pointer->addr->getHeaderVarString();
                                try {
                                    ptr = _TABLE->at(cfName);
                                } catch (std::out_of_range &e) {
                                    m_LuaErrorHandler->reportWarning(_lua_es_NonFunction, 0, "Object's required function doesn't exist!");
                                }
                                // We got function pointer, set first those function arguments.
                                _HELPER__runHooksFor(x86::r9, _R_TRASHDATA);
                                LuaLexFrame _SELF(_L_PATH); 
                                LuaLexFrame fPtr = *pointer->addr->getHeader();
                                lua_AddrPath *p = new lua_AddrPath();
                                p->assignNewAddr(std::vector<LuaLexFrame>({fPtr}));
                                p->getBack()->_LK = true;
                                _SELF.addr = p;
                                _SELF.ATTRIB = 0;
                                lua_Expression E = _CPP__insertToFirstPosition(std::vector<LuaLexFrame>{_SELF}, &pointer->EXPR);
                                _CLHASM__buildArgs(E, scope);
                                a->call(ptr);
                                break;
                            }
                        }
                    } else {
                        // Normal block.
                        GeneralRegister *path = R->createGR("path0__"+RET_NAME);
                        x86::Gp p = _ASM__getPathToSelGp(pointer->addr->getData(), "path0__"+RET_NAME, scope, false, true);
                        a->mov(x86::rbx, p);
                        R->destroyGR("path0__"+RET_NAME);
                        //
                        _CLHASM__buildArgs(pointer->EXPR, scope);
                        R->createGR("mask"+RET_NAME);
                        a->mov(S("mask"+RET_NAME), PTR_MASK);
                        a->and_(x86::rbx, S("mask"+RET_NAME));
                        R->destroyGR("mask"+RET_NAME);
                        R->emitCall();
                        a->call(x86::rbx);
                    }
                    R->createGR("ReturnValue0", false, x86::rax);
                }
                _terminate:
                if (_OPMODE != 0) {
                    // Restored, now, let's proceed.
                    if (_use_ret_reg) {
                        // Time to move..
                        a->mov(S(RET_NAME), x86::Gp::make_r64(ret.id()));
                    }
                    _ASM__runOpCode(_OPMODE, S(RET_NAME), S("ReturnValue0"), false, false); // No overhead check, direct execution.
                    _OPMODE = static_cast<_Lua_Lex_Keys>(0);
                } else {
                    a->mov(S(RET_NAME), S("ReturnValue0"));
                }
                R->destroyGR("ReturnValue0");
                break;
            }
            case _L_VARNAME: {
                qlog0._log2("\n### _L_VARNAME START ###\n");
                // Only get the header. [Unique case.]
                abort(); // Not allowed.
                qlog0._log2("### _L_PATH END ###\n");
                break;
            }
            case _L_PATH: {
                GeneralRegister *path_ = _CPP__areThereOpInstruction(k, pos+1).first ? R->createGR("path"+RET_NAME) : nullptr;
                if (_OPMODE != 0) {
                    // Maybe save ret?..
                    if (_savedToXMMreg) {
                        a->movq(S(RET_NAME), x86::xmm0);
                    }
                }
                x86::Gp path = _ASM__getPathToSelGp(pointer->addr->getData(), _CPP__areThereOpInstruction(k, pos+1).first ? ("path"+RET_NAME) : RET_NAME, scope, (!_CPP__existsMoreOnWay(k, pos) && getPointerInsteadofRawD), true);
                
                // Convert the given value if cvt2* are present.
                bool _CVT = false;
                _Lua_Lex_Keys _K = static_cast<_Lua_Lex_Keys>(pointer->toTypeConvert);
                if (_K == _L_CVT_DOUBLE) {
                    if (path.id() < 12) {
                        // Untag
                        a->mov(x86::r11, PTR_MASK);
                        a->and_(path, PTR_MASK);
                    }
                    if (_OPMODE == 0) {
                        a->cvtsi2sd(x86::xmm0, path);
                        _savedToXMMreg = true;
                    } else
                        a->cvtsi2sd(x86::xmm1, path);
                } else if (_K == _L_CVT_INTEGER) { 
                    // path can already be interpreted as integer.
                }
                if (_OPMODE != 0) {
                    if (pointer->toTypeConvert == 0) 
                        _ASM__runOpCode(_OPMODE, S(RET_NAME), S("path"+RET_NAME), false, false);
                    else if (_K == _L_CVT_DOUBLE) {
                        // restore xmm0
                        a->movq(x86::xmm0, S(RET_NAME));
                        _ASM__runOpCode(_OPMODE, x86::xmm0, x86::xmm1, false, false);
                        //Convert if theres nothing left behind
                        if (!_CPP__existsMoreOnWay(k, pos+1)) {
                            a->movq(S(RET_NAME), x86::xmm0);
                        }
                    }
                } else {
                    // Register it.
                    if (path.id() > 11) {
                        ret = path;
                        _use_ret_reg = true;
                    } else {
                        a->mov(S(RET_NAME), S("path"+RET_NAME));
                    }
                }
                if (path_ != nullptr)
                    R->destroyGR("path"+RET_NAME);
                _OPMODE = static_cast<_Lua_Lex_Keys>(0);
                break;
            }
            // Direct medias.
            // string, number, booleans.
            case _L_STRING: {
                // Insert string.
                if (_OPMODE == 0) {
                    a->mov(S(RET_NAME), (uint64_t)lua_makeVar(pointer->a, LuaString));
                } else {
                    GeneralRegister *GP0 = R->createGR("STR00"+RET_NAME);
                    a->mov(S("STR00"+RET_NAME), (uint64_t)lua_makeVar(pointer->a, LuaString));
                    _ASM__runOpCode(_OPMODE, S(RET_NAME), S("STR00"+RET_NAME), false, false);
                    R->destroyGR("STR00"+RET_NAME);
                    _OPMODE = static_cast<_Lua_Lex_Keys>(0);
                }
                if (_notOpCode) {
                    a->not_(S(RET_NAME));
                }
                _CPP__setcntId(ret, _R_CLUATYPE_TAGGED);
                break;
            }
            case _L_TRUE: {
                if (_OPMODE == 0) {
                    a->mov(S(RET_NAME), 0x000000000000001ULL);
                } else {
                    GeneralRegister *GP0 = R->createGR("BOOL::true"+RET_NAME);
                    a->mov(S("BOOL::true"+RET_NAME), 0x000000000000001ULL);
                    if (_use_ret_reg) {
                        a->mov(S(RET_NAME), x86::Gp::make_r64(ret.id()));
                    }
                    _ASM__runOpCode(_OPMODE, S(RET_NAME), S("BOOL::true"+RET_NAME), false, false);
                    _OPMODE = static_cast<_Lua_Lex_Keys>(0);
                }
                break;
            }
            case _L_FALSE: {
                if (_OPMODE == 0) {
                    a->xor_(S(RET_NAME),S(RET_NAME));
                } else {
                    GeneralRegister *GP0 = R->createGR("BOOL::false"+RET_NAME);
                    a->xor_(S("BOOL::false"+RET_NAME), S("BOOL::false"+RET_NAME));
                    if (_use_ret_reg) {
                        a->mov(S(RET_NAME), x86::Gp::make_r64(ret.id()));
                    }
                    _ASM__runOpCode(_OPMODE, S(RET_NAME), S("BOOL::false"+RET_NAME), false, false);
                    _OPMODE = static_cast<_Lua_Lex_Keys>(0);
                }
                break;
            }
            
            // NUMBERS.
            
            case _L_NUMBER: {
                // Insert those typos.
                if (pointer->ATTRIB) { // It is a double precision value.
                    _HELPER__runHooksFor(x86::r9, _R_TRASHDATA);
                    
                    double _a = std::stod(std::string(pointer->_data.begin(), pointer->_data.end()));
                    uint64_t val = 0;
                    memcpy(&val, &_a, 8);
                    
                    GeneralRegister *reg = R->createGR("number::double"+RET_NAME);
                    std::pair<bool, uint8_t> z = _CPP__areThereOpInstruction(k, pos+1);
                    
                    if (_OPMODE != 0) {
                        a->mov(S(reg->name), val);
                        // proceed anyways, whenever _savedToXMMreg is false or true, everything should be direct
                        _ASM__runOpCode(_OPMODE, x86::xmm0, S("number::double"+RET_NAME), false, false); // If theres a number value neither way a double it always are saved in XMM register.
                        _OPMODE = static_cast<_Lua_Lex_Keys>(0);
                    } else {
                        // Just move the double value if there are not exponents.
                        if (z.first) { // Move to xmm0
                            a->mov(S(reg->name), val);
                            a->movq(x86::xmm0, S("number::double"+RET_NAME));
                            _savedToXMMreg = true;
                        } else {
                            a->mov(S(RET_NAME), val);
                        }
                        goto _L_NUMBER_END;
                    }
                    
                    
                    if (!z.first) {
                        // Transform to a direct double value.
                        a->movq(S(RET_NAME), x86::xmm0); // x0
                        _savedToXMMreg = false;
                    }
                    
                    _L_NUMBER_END:
                    R->destroyGR("number::double"+RET_NAME);
                } else {
                    uint64_t val = ((noTag ? 0x0ULL : (uint64_t)0x7FF8000000000000ULL) | (uint64_t)std::stoi(std::string(pointer->_data.begin(), pointer->_data.end())));
                    if (_CPP__getcntId(ret) == _R_TRASHDATA) {
                        a->movabs(S(RET_NAME), val);
                    } else {
                        if (!noTag) {
                            // Untag the requested item.
                            if (_use_ret_reg)
                                goto _CNTINTEGER;
                            a->mov(x86::r11, PTR_MASK);
                            a->and_(S(RET_NAME), PTR_MASK);
                        }
                        _CNTINTEGER:
                        a->mov(x86::r11, (uint64_t)std::stoi(std::string(pointer->_data.begin(), pointer->_data.end())));
                        if (_use_ret_reg) {
                            a->mov(S(RET_NAME), x86::Gp::make_r64(ret.id()));
                        }
                        ret = _ASM__runOpCode(_OPMODE, S(RET_NAME), x86::r11, false, false);
                    }
                    _CPP__setcntId(ret, _R_CLUATYPE_TAGGED);
                }
                break;
            }
        }
        pos++;
        _quotient_r=true;
    }
}
