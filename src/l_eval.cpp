#include "lua.hpp"
#include <csignal>
#include <cstdint>
#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>
#include <stdexcept>

using namespace asmjit;

static x86::Assembler *a = nullptr;
static _REGISTER_ *clogReg = nullptr;
int32_t stackRegCounter = 0; // Starts from -40.
RegistersDataGP lua_Registers = RegistersDataGP();
RegistersDataXMM lua_RegistersXMM = RegistersDataXMM();
static bool _xmmUsed = false;

// This calculates with "high precision" about contents in the registers. Eval.

void nothing(x86::Assembler *a, _REGISTER_ *reg) {}

void initializeRegistersData(void *asmPtr) {
    // Registers between r12 - r15 are banned to be used in a normal routime.
    // General purpose registers.
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RAX, _REGISTER_{REG_RAX,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RSI, _REGISTER_{REG_RSI,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RCX, _REGISTER_{REG_RCX,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RDI, _REGISTER_{REG_RDI,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RDX, _REGISTER_{REG_RDX,0x0,0,0,LuaUnknown,_R_TRASHDATA,nothing}));
    //lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R10, _REGISTER_{REG_R10,0x0,nullptr,LuaUnknown,_R_TRASHDATA}));
    //lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_R11, _REGISTER_{REG_R11,0x0,nullptr,LuaUnknown,_R_TRASHDATA}));
    //lua_Registers.insert(std::pair<greg_t, _REGISTER_>(REG_RBX, _REGISTER_{_R_TRASHDATA,REG_RBX,0x0,nullptr,LuaUnknown}));
    // XMM registers
    lua_RegistersXMM.insert(std::pair<_LUA_XMM_REGISTERS, _XREGISTER_>(xmm0, _XREGISTER_{0x0,xmm0,_R_TRASHDATA,0,nullptr,nothing}));
    lua_RegistersXMM.insert(std::pair<_LUA_XMM_REGISTERS, _XREGISTER_>(xmm1, _XREGISTER_{0x0,xmm1,_R_TRASHDATA,0,nullptr,nothing}));
    lua_RegistersXMM.insert(std::pair<_LUA_XMM_REGISTERS, _XREGISTER_>(xmm2, _XREGISTER_{0x0,xmm2,_R_TRASHDATA,0,nullptr,nothing}));
    lua_RegistersXMM.insert(std::pair<_LUA_XMM_REGISTERS, _XREGISTER_>(xmm3, _XREGISTER_{0x0,xmm3,_R_TRASHDATA,0,nullptr,nothing}));
    // Save assembler.
    a = (x86::Assembler*)asmPtr;
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
    }
    return 0;
}

void _CPPH_setRegisterStatus(x86::Gp reg, _R_CONTENTS cnt) {
    if (lua_Registers.find(_CPP_getRegisterFromASM(reg)) == lua_Registers.end()) 
        return;
    lua_Registers.at(_CPP_getRegisterFromASM(reg)).cntId == cnt;
}

void _CPP__turnRegistersAfterCall() {
    lua_Registers.at(REG_RDI).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RSI).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RCX).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RAX).cntId = _R_FUNC_RESULT;
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
    }
}

void _ASM__crash(uint8_t errCode) {
    switch (errCode) {
        case notFunction: {
            m_LuaErrorHandler->reportError(_lua_es_NonFunction, 0, "During execution: Not a function.");
            return;
        }
    }
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
    uint64_t base = 0x0000000000000000ULL | type << 48;
    // 3 integers + 1;
    // left
    return base;
}

x86::Gp _ASM__cmpVarType(x86::Gp base, LuaType toType) {
    x86::Gp cc = base;
    uint64_t tCMP = _ASMH__makeCoolTypeCMPROR(toType);
    a->movabs(x86::r10, tCMP);
    a->cmp(base, x86::r10);
}

void _ASM__getFromTableIndex(uint8_t mode, x86::Gp baseTable0, uint64_t baseTable1, x86::Gp regCounter, uint32_t regCounter0, x86::Gp ret, bool getPtr = false) {
    if (mode == 1) { // Registers based
        Label _of = a->new_named_label("overflow");
        Label _end = a->new_named_label("_END__");
        a->mov(x86::r11, x86::qword_ptr(baseTable0, offsetof(lua_Table, asize)));
        a->cmp(x86::r11, regCounter);
        a->ja(_of);
        a->mov(x86::r11, x86::qword_ptr(baseTable0, offsetof(lua_Table, array)));
        a->lea(x86::r10, x86::qword_ptr(0,regCounter,8)); // I do not want to use MUL neither IMUL.
        if (!getPtr)
            a->mov(ret, x86::qword_ptr(x86::r11, x86::r10));
        else
            a->lea(ret, x86::qword_ptr(x86::r11, x86::r10));
        a->jmp(_end);
        a->bind(_of);
        a->mov(ret, 0);
    } else {
        // meth.
        Label _of = a->new_named_label("overflow");
        Label _end = a->new_named_label("_END__");
        a->movabs(x86::r10, baseTable1);
        a->mov(x86::r11, x86::qword_ptr(x86::r10, offsetof(lua_Table, asize)));
        a->cmp(x86::r11, regCounter0); // Inserted as an imm32
        a->ja(_of);
        a->mov(x86::r11, x86::qword_ptr(baseTable0, offsetof(lua_Table, array)));
        a->mov(x86::r10, regCounter0*8); // I do not want to use MUL neither IMUL.
        if (!getPtr)
            a->mov(ret, x86::qword_ptr(x86::r11, x86::r10));
        else
            a->lea(ret, x86::qword_ptr(x86::r11, x86::r10));
        a->jmp(_end);
        a->bind(_of);
        a->mov(ret, 0);
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
// Just for saving contents...
// INSERT METALLIC MADNESS HERE
void _ASMH__rs_searchInTable(
    x86::Gp tblPTR,
    std::pair<bool, std::pair<x86::Gp, TString*>> key,
    x86::Gp toGp,
    bool pointer = false
) {
    // Oof.
    if (lua_Registers.at(REG_RDI).cntId == _R_FUNC_ARGS) {
        // Should save it?
        lua_Registers.at(REG_RDI).stackPtrBase = -40+stackRegCounter;
        stackRegCounter -= 8;
        a->mov(x86::qword_ptr(x86::rbp, lua_Registers.at(REG_RDI).stackPtrBase), x86::rdi);
        lua_Registers.at(REG_RDI).cntId = _R_TRASHDATA;
    }
    if (lua_Registers.at(REG_RSI).cntId == _R_FUNC_ARGS) {
        // Should save it?
        lua_Registers.at(REG_RSI).stackPtrBase = -40+stackRegCounter;
        stackRegCounter -= 8;
        a->mov(x86::qword_ptr(x86::rbp, lua_Registers.at(REG_RDI).stackPtrBase), x86::rsi);
        lua_Registers.at(REG_RSI).cntId = _R_TRASHDATA;
    }
    lua_Registers.at(REG_RAX).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RDX).cntId = _R_TRASHDATA;
    qlog0._log2("start::searchInTable\n", 21);
    Label _nf = a->new_label();
    Label _f = a->new_label();
    Label _loop = a->new_label();
    //Label _insert = a->new_named_label("insert");
    Label _end = a->new_label();
    
    // r9 = table
    if (tblPTR != x86::noReg)
        if (tblPTR != x86::r9)
            a->mov(x86::r9, tblPTR);
    
    // r8 = hash
    if (key.first) {
        a->mov(x86::r8, key.second.first);
    } else {
        a->mov(x86::r8, key.second.second->IDX);
    }
    
    if (!pointer)
        a->mov(x86::rdi, x86::r8);
    else
        a->mov(x86::rdi, (uint64_t)key.second.second);
    
    // idx = hash & mask
    a->and_(x86::r8, x86::qword_ptr(x86::r9, offsetof(lua_Table, hmask)));
    // rsi = &nodes[idx]
    a->mov(x86::rsi, x86::qword_ptr(x86::r9, offsetof(lua_Table, nodes)));
    
    //a->xor_(x86::r10, x86::r10);
    a->shl(x86::r8, 5);
    //a->add(x86::r8, x86::rsi);
    a->lea(x86::r11, x86::qword_ptr(x86::rsi, x86::r8));
    if (pointer)
        a->mov(x86::r8, x86::qword_ptr(x86::rdi));
    
    // rdx = current node
    a->mov(x86::rdx, x86::r11);
    
    a->bind(_loop);
    
    a->test(x86::rdx, x86::rdx);
    a->jz(_nf);
    
    // rax = node->key
    a->mov(x86::rax, x86::qword_ptr(x86::rdx));
    a->test(x86::rax, x86::rax);
    a->jz(_nf);
    
    if (!pointer)
        a->cmp(x86::rdi, x86::qword_ptr(x86::rax));
    else
        a->cmp(x86::r8, x86::qword_ptr(x86::rax));
    a->je(_f);
    
    // next
    a->mov(x86::rdx, x86::qword_ptr(x86::rdx, offsetof(Node, next)));
    a->jmp(_loop);
    
    // =========================
    // NOT FOUND
    // =========================
    a->bind(_nf);
    
    if (!pointer) {
        a->mov(toGp, 0);
        a->jmp(_end);
    } else {
        // -------- INSERT --------
        /*
        // call alloc_node()
        a->mov(x86::rax, (uint64_t)alloc_node);
        a->call(x86::rax);
        
        
        // rax = new node
        // rdx = head (original bucket)
        
        // new->key = key
        a->mov(x86::qword_ptr(x86::rax, offsetof(Node, key)), x86::rcx);
        
        // new->next = old head
        a->mov(x86::qword_ptr(x86::rax, offsetof(Node, next)), x86::rsi);
        
        // write new head into table bucket
        a->mov(x86::r10, x86::qword_ptr(x86::r9, offsetof(lua_Table, nodes)));
        a->lea(x86::r10, x86::qword_ptr(x86::r10, x86::r8, sizeof(Node)));
        a->mov(x86::qword_ptr(x86::r10), x86::rax);
        
        // return &new->val
        a->lea(toGp, x86::qword_ptr(x86::rax, offsetof(Node, val)));*/
        a->mov(x86::rsi, x86::rdi);
        a->mov(x86::rdi, x86::r9);
        a->mov(x86::rdx, 0);
        a->mov(x86::rax, (uint64_t)_F_ASM_NOTGUARANTEED_SETVALUE);
        a->call(x86::rax);
        a->mov(toGp, x86::rax);
        a->jmp(_end);
        //a->bind(_f);
        //goto endzone;
    }
    // =========================
    // FOUND
    // =========================
    a->bind(_f);
    std::cout << "chr: " << toGp.id() << std::endl;
    if (!pointer) {
        a->mov(toGp, x86::qword_ptr(x86::rdx, offsetof(Node, val)));
    } else {
        a->lea(toGp, x86::qword_ptr(x86::rdx, offsetof(Node, val)));
    }
    //endzone:
    a->bind(_end);
    qlog0._log2("end::searchInTable\n", 19);
}
/*
void _ASMH__rs_searchInTable(x86::Gp tblPTR, std::pair<bool, std::pair<x86::Gp, TString*>> key, x86::Gp toGp, bool pointer = false) {
    Label _nf = a->new_named_label("notFound_GCFMG");
    Label _f = a->new_named_label("found_GCFMG");
    Label _end = a->new_named_label("__END");
    Label _slS = a->new_named_label("searchingLoopStart_GCFMG");
    if (tblPTR != x86::noReg)
        if (tblPTR != x86::r9)
            a->mov(x86::r9, tblPTR);
    if (key.first) { // Use GPR
        if (key.second.first != x86::r8)
            a->mov(x86::r8, key.second.first);
    } else {
        a->movabs(x86::r8, key.second.second->IDX);
    }
    if (pointer) {
        a->movabs(x86::rcx, (uint64_t)key.second.second);
        a->mov(x86::rdi, x86::r9);
    }    
    a->and_(x86::r8, x86::qword_ptr(x86::r9, offsetof(lua_Table, hmask)));
    a->mov(x86::rsi, x86::qword_ptr(x86::r9, offsetof(lua_Table, nodes)));
    a->lea(x86::rsi, x86::qword_ptr(x86::rsi, x86::r8, sizeof(Node)));
    a->test(x86::rsi, x86::rsi);
    a->jz(_nf);
    a->mov(x86::r9, x86::qword_ptr(x86::rsi, offsetof(Node, key)));
    a->test(x86::r9, x86::r9);
    a->jz(_nf);
    a->cmp(x86::r8, x86::r9);
    a->je(_f);
    // Chain loop
    a->mov(x86::rdx, x86::qword_ptr(x86::rsi, offsetof(Node, next)));
    a->bind(_slS);
    a->test(x86::rdx, x86::rdx);
    a->jz(_nf);
    a->mov(x86::r9, x86::qword_ptr(x86::rsi, offsetof(Node, key)));
    a->cmp(x86::r8, x86::r9);
    a->je(_f);
    a->mov(x86::rsi, x86::qword_ptr(x86::rsi, offsetof(Node, next)));
    a->jmp(_slS);
    // Proc
    a->bind(_nf);
    if (!pointer) {
        a->mov(toGp, 0);
        a->ud2();
    } else {
        a->mov(x86::rax, (uint64_t)_F_ASM_NOTGUARANTEED_SETVALUE);
        a->mov(x86::rsi, x86::rcx);
        a->call(x86::rax);
        a->mov(toGp, x86::rax);
    }
    a->jmp(_end);
    a->bind(_f);
    if (!pointer)
        a->mov(toGp, x86::qword_ptr(x86::rsi, offsetof(Node, val)));
    else
        a->lea(toGp, x86::qword_ptr(x86::rsi, offsetof(Node, val)));
    a->bind(_end);
}*/

void _ASM__getContentsFromMGENERAL(TString *key, x86::Gp toGp, bool modify = false) {
    // Get from m_General
    // r8, r9 and rsi
    // Store IDX and 'and' it with hmask
    // Simple.
    a->mov(x86::r9, (uint64_t)m_General);
    _ASMH__rs_searchInTable(x86::noReg, std::pair<bool, std::pair<x86::Gp, TString*>>(false, std::pair<x86::Gp, TString*>(x86::noReg, key)), toGp, modify);
}

// first=Register, second=High performance local register used
std::pair<x86::Gp, bool> _ASM__searchSymbolToUse(x86::Gp toGp, TString *sym, lua_Scope *actScope, bool toModify = false) {
    std::string symSTR = std::string(sym->data, sym->len);
    bool q0 = false;
    x86::Gp _R = toGp;
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
            if (!toModify) {
                if (s->register_ != x86::rax && lua_Registers.at(_CPP_getRegisterFromASM(s->register_)).cntId == _R_FUNC_ARGS_ENTRY) {
                    return {s->register_,true};
                }
            }
            if (!_0_0_0_CMPTIME_ASM_isScript) {
                int32_t sK = -512;
                sK -= ((int32_t)s->slot);
                if (!toModify)
                    a->mov(toGp, x86::qword_ptr(x86::rbp, sK));
                else
                    a->lea(toGp, x86::qword_ptr(x86::rbp, sK));
            } else {
                a->movabs(x86::r9, (uint64_t)_0_0_0_CMPTIME_ASM_scriptMem);
                if (!toModify)
                    a->mov(toGp, x86::qword_ptr(x86::r9, s->slot));
                else
                    a->lea(toGp, x86::qword_ptr(x86::r9, s->slot));
            }
            // Push lua_Registers handler for unexistent registers.
            _CPPH_setRegisterStatus(toGp, _R_CLUATYPE_TAGGED);
            if (!q0)
                return {toGp, false};
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
                        if (!toModify) {
                            if (!_0_0_0_CMPTIME_ASM_isScript && s->register_ != x86::rax && lua_Registers.at(_CPP_getRegisterFromASM(s->register_)).cntId == _R_FUNC_ARGS_ENTRY) {
                                return {s->register_,true};
                            }
                        }
                        if (!_0_0_0_CMPTIME_ASM_isScript) {
                            int32_t sK = -512;
                            sK -= ((int32_t)s->slot);
                            if (!toModify)
                                a->mov(toGp, x86::qword_ptr(x86::rbp, sK));
                            else
                                a->lea(toGp, x86::qword_ptr(x86::rbp, sK));
                        } else {
                            a->movabs(x86::r9, (uint64_t)_0_0_0_CMPTIME_ASM_scriptMem);
                            if (!toModify)
                                a->mov(toGp, x86::qword_ptr(x86::r9, s->slot));
                            else
                                a->lea(toGp, x86::qword_ptr(x86::r9, s->slot));
                        }
                        _CPPH_setRegisterStatus(toGp, _R_CLUATYPE_TAGGED);
                        if (!q0)
                            return {toGp, false};
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
            if (queue.size() > 0) {
                if (queue.back().m0 > 0) {
                    x86::Gp reg = x86::Gp::make_r64(static_cast<uint32_t>(queue.back().m0));
                    if (reg != x86::r9 && reg != toGp && reg != x86::rdi && reg != x86::rsi && reg != x86::rdx && reg != x86::rcx && reg != x86::rax && reg != x86::r8 && reg != x86::r11) {
                        goto _KFINISH;
                    }
                    a->mov(x86::qword_ptr(x86::rbp, -128), reg);
                    queue.back().m4 = 1;
                }
            }
            _KFINISH:
            _ASM__getContentsFromMGENERAL(sym, toGp, toModify);
            return {_R, q0};
        }
    }
}

x86::Gp _ASM__keyInstRestoreVar(x86::Gp toVar) {
    if (lua_Registers.find(_CPP_getRegisterFromASM(toVar)) == lua_Registers.end()) {
        return toVar;
    }
    if (lua_Registers.at(_CPP_getRegisterFromASM(toVar)).cntId == _R_CLUATYPE_TAGGED) {
        // Stays in a register.
        //_ASM__movToReg(toVar, x86::rcx);
    } else if (lua_Registers.at(_CPP_getRegisterFromASM(toVar)).cntId == _R_TRASHDATA) {
        if (lua_Registers.at(REG_RCX).cntId == _R_CLUATYPE_TAGGED) {
            _ASM__movToReg(toVar, x86::rcx);
            return toVar;
        }
        // Are in the stack
        a->mov(toVar, x86::qword_ptr(x86::rbp, lua_Registers.at(_CPP_getRegisterFromASM(toVar)).stackPtrBase));
        stackRegCounter += 8;
        lua_Registers.at(_CPP_getRegisterFromASM(toVar)).cntId = _R_CLUATYPE_TAGGED;
    }
    return toVar;
}

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


x86::Gp _ASM__runOpCode(_Lua_Lex_Keys opcode, x86::Gp op0, x86::Gp op1, bool check0 = true) {
    int oQ = static_cast<int>(opcode);
    
    // Math.
    if (oQ > 39 && oQ < 46) {
        _ASM__GPR_Proc(op0, op1, opcode);
        return op0;
    } else if (oQ > 28 && oQ < 34) { // STR/BOOL
        
    }
    
    switch (opcode) {
        case _L_CONCAT: {
            // Reset variables <As we go.>
            if (lua_Registers.at(REG_RDI).cntId == _R_FUNC_ARGS) {
                // Should save it?
                lua_Registers.at(REG_RDI).stackPtrBase = -40+stackRegCounter;
                stackRegCounter -= 8;
                a->mov(x86::qword_ptr(x86::rbp, lua_Registers.at(REG_RDI).stackPtrBase), x86::rdi);
                lua_Registers.at(REG_RDI).cntId = _R_TRASHDATA;
            }
            if (lua_Registers.at(REG_RSI).cntId == _R_FUNC_ARGS) {
                // Should save it?
                lua_Registers.at(REG_RSI).stackPtrBase = -40+stackRegCounter;
                stackRegCounter -= 8;
                a->mov(x86::qword_ptr(x86::rbp, lua_Registers.at(REG_RSI).stackPtrBase), x86::rsi);
                lua_Registers.at(REG_RSI).cntId = _R_TRASHDATA;
            }
            // Concat requires external calling..
            qlog0._log2("start::concat()\n", 16);
            Label noString;
            Label end = a->new_label();
            _ASM__movToReg(x86::r10, op1);
            _ASM__movToReg(x86::r8, op0);
            //a->ud2();
            if (check0) {
                noString = a->new_label();
                a->mov(x86::r11, (uint64_t)0x000F000000000000ULL);
                a->and_(x86::r11, x86::r8);
                a->mov(x86::r9, (uint64_t)L_ASM_Stri);
                a->cmp(x86::r11, x86::r9);
                a->jne(noString);
            }
            //a->ud2();
            a->mov(x86::r11, (uint64_t)0x0000FFFFFFFFFFFFULL);
            a->mov(x86::rdi, x86::r8);
            a->mov(x86::rsi, x86::r10);
            
            qlog0._log2("\rrdi = Opcode0 ");
            a->and_(x86::rdi, x86::r11);
            qlog0._log2("\rrsi = Opcode1<Checked> ");
            a->and_(x86::rsi, x86::r11);
            qlog0._log2("\r__ASM_F_STRINGMANIPULATOR_CONCAT2(): ");
            a->mov(x86::rax, (uint64_t)__ASM_F_STRINGMANIPULATOR_CONCAT2);
            qlog0._log2("\r__ASM_F_STRINGMANIPULATOR_CONCAT2(): ");
            a->call(x86::rax);
            a->jmp(end);
            if (check0)
                a->bind(noString);
            // Crash.
            //a->ud2();
            a->mov(x86::rax, 0);
            a->bind(end);
            
            qlog0._log2("end::concat()\n", 14);
            return x86::rax;
        }
        default: {
            m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, "Unknown resources.");
            _ASM__insertNull(op0);
            break;
        }
    }
    return op0;
}

std::pair<bool, uint8_t> _CPP__emittedAnyOpcode(_Lua_Lex_Keys c) {
    int oQ = static_cast<int>(c);
    if (oQ > 39 && oQ < 46) {
        return {true, 1}; // Math operators.
    }
    if (oQ > 28 && oQ < 34) {
        return {true, 2}; // String/Booleans operators.
    }
    return {false, 0};
}

void _ASM_crashINSTR(lua_ErrSignals signal) {
    abort();
}

//x86::Gp CLUA_EvalExprNReturn(std::vector<LuaLexFrame> *k, lua_Scope *scope, std::pair<bool, x86::Gp> saveSpecificallyTo, bool getPointerInsteadofRawD = false);

x86::Gp _ASM__getPathToSelGp(std::vector<LuaLexFrame> *vct, x86::Gp ret, lua_Scope *aSCP, bool pointer, bool preservRegister) {
    bool gotFirst = false;
    bool continuity = false;
    LuaLexFrame *actual = nullptr;
    uint64_t pos = 0;
    x86::Gp act = ret;
    while (true) {
        try {
            actual = &vct->at(pos);
        } catch (std::out_of_range &e) {
            return act;
        }
        switch (actual->key) {
            case _L_VARNAME: {
                if (!gotFirst) {
                    std::pair<x86::Gp, bool> res = _ASM__searchSymbolToUse(ret, (TString*)actual->a, aSCP, pointer); //for func args pointer should be false.
                    if (res.second) {
                        if (res.first.id() < x86::Gp::Id::kIdR12) { // Do not overwrite
                            _ASM__movToReg(act, res.first);
                            act = res.first;
                        } else {
                            if (!pointer)
                                if (!preservRegister)
                                    _ASM__movToReg(ret, res.first);
                                else {
                                    _ASM__movToReg(res.first, res.first); // Useless call but good.
                                    //act = res.first;
                                }
                            act = res.first;
                        }
                    }
                    gotFirst = true;
                } else {
                    if (!continuity) {
                        m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, "Expected '.' to continue table searching but found nothing.");
                        _ASM__insertNull(ret);
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
                            _ASM__cmpVarType(act, LuaTable);
                            a->jne(_f);
                            //Search for this value.
                            _ASMH__rs_searchInTable(act, std::pair<bool, std::pair<x86::Gp, TString*>>(false, std::pair<x86::Gp, TString*>(x86::noReg, (TString*)actual->a)), act, pointer);
                            a->jmp(_f1);
                            a->bind(_f);
                            _ASM_crashINSTR(_lua_es_UnknownDataIdx);
                            a->mov(act, 0);
                            a->bind(_f1);
                        } else {
                            Label _f = a->new_label();
                            Label _f1 = a->new_label();
                            _ASM__cmpVarType(act, LuaTable);
                            a->jne(_f);
                            //Search for this value.
                            a->mov(act, x86::qword_ptr(act));
                            _ASMH__rs_searchInTable(act, std::pair<bool, std::pair<x86::Gp, TString*>>(false, std::pair<x86::Gp, TString*>(x86::noReg, (TString*)actual->a)), act);
                            a->jmp(_f1);
                            a->bind(_f);
                            _ASM_crashINSTR(_lua_es_UnknownDataIdx);
                            a->mov(act, 0);
                            a->bind(_f1);
                        }
                    }
                } 
                break;
            }
            case _L_ON_TO_GO: {
                if (actual->ATTRIB == 0) { // non brkt
                    continuity = true;
                } else {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, "Unexpected reading!");
                    _ASM__insertNull(ret);
                }
                break;
            }
            case _L_EXPRESSION_BRKT: {
                // Save ret just for later [Access!]
                uint64_t baseMem = (uint64_t)malloc(8);
                a->movabs(x86::r9, baseMem);
                a->mov(x86::qword_ptr(x86::r9), act);
                x86::Gp ret0 = CLUA_EvalExprNReturn(&actual->EXPR_BRKT, aSCP, std::pair<bool, x86::Gp>(false, x86::noReg), false);
                // Eval.
                // Should be: Int/Double, string or nil
                Label _f = a->new_named_label("notString_NumberOrDouble");
                Label number__ = a->new_named_label("NotNumber__");
                Label _f2 = a->new_named_label("Nothing__");
                Label _f3 = a->new_named_label("END__");
                _ASM__cmpVarType(ret0, LuaString);
                a->jne(_f);
                // Should either.. Search?
                // Bring it back.
                a->movabs(x86::r9, baseMem);
                a->mov(x86::r9, x86::qword_ptr(x86::r9));
                a->mov(x86::r8, ret0);
                a->movabs(x86::r11, 0x0000FFFFFFFFFFFFULL);
                a->and_(x86::r11, x86::r8);
                _ASMH__rs_searchInTable(x86::noReg, std::pair<bool, std::pair<x86::Gp, TString*>>(true, std::pair<x86::Gp, TString*>(x86::r8, nullptr)), act);
                a->jmp(_f3);
                a->bind(_f);
                a->movabs(x86::r9, baseMem);
                a->mov(x86::r9, x86::qword_ptr(x86::r9));
                // Should be a number.
                _ASM__cmpVarType(ret0, LuaNumber);
                a->jne(number__);
                // Hell.
                _ASMH__doLuaNumberToInteger(ret0);
                _ASM__getFromTableIndex(1, x86::r9, 0x0, ret0, 0x0, ret, false);
                a->jmp(_f3);
                a->bind(number__);
                _ASM__cmpVarType(ret0, LuaInteger);
                a->jne(_f2);
                _ASM__getFromTableIndex(1, x86::r9, 0x0, ret0, 0x0, ret, false);
                a->jmp(_f3);
                a->bind(_f2);
                //Crash [Nil]
                _ASM_crashINSTR(_lua_es_InvalidType);
                a->bind(_f3);
                break;
            }
        }
        pos++;
    }
    _RET:
    return act;
}

std::pair<bool, uint8_t> _CPP__areThereOpInstruction(std::vector<LuaLexFrame> *l, uint32_t pos) {
    try {
        LuaLexFrame qID = l->at(pos);
        return {true, _CPP__emittedAnyOpcode(qID.key).second};
    } catch (std::out_of_range &e) {
        return {false, 0};
    }
}



x86::Gp CLUA_EvalExprNReturn(std::vector<LuaLexFrame> *k, lua_Scope *scope, std::pair<bool, x86::Gp> saveSpecificallyTo, bool getPointerInsteadofRawD, bool noTag) {
    LuaLexFrame *pointer = nullptr;
    uint32_t pos = 0;
    x86::Gp ret = saveSpecificallyTo.first ? saveSpecificallyTo.second : x86::rax;
    bool _setReg = false;
    x86::Gp q0 = x86::noReg;
    _Lua_Lex_Keys _OPMODE = _L_NONE;
    bool _notOpCode = false;
    bool _emitted_xmmOCX = false;
    uint8_t tEmittedCode = 0;
    m_LuaErrorHandler->reportWarning(_lua_es_Illegal, 0, (noTag ? "noTag" : "Tagged"));
    bool _presearched_query = false;
    bool _use_ret_reg = false;
    bool _quotient_r = false;
    LuaLexFrame *doomPath0 = nullptr;
    uint8_t lType0 = 0;
    while (true) {
        try {
            pointer = &k->at(pos);
        } catch (std::out_of_range &e) {
            queue.clear();
            if (q0 != x86::noReg)
                return q0;
            return ret;
        }
        std::pair<bool, uint8_t> eao = _CPP__emittedAnyOpcode(pointer->key);
        if (eao.first) {
            _OPMODE = pointer->key;
            tEmittedCode = eao.second;
            // Save.
            if (!_emitted_xmmOCX)
                _ASM__keyInstSaveVarOnEnv(ret);
            pos++;
            _quotient_r=true;
            _presearched_query = false;
            continue;
        }
        switch (pointer->key) {
            case _L_NOT: {
                _notOpCode = !_notOpCode;
                break;
            }
            case _L_CALL: {
                // Call sistematics.
                // Reset vars.
                int32_t lvl = 0;
                int32_t _q0 = 0;
                if (lua_Registers.at(REG_RDI).cntId != _R_TRASHDATA) {
                    // Should save it?
                    lua_Registers.at(REG_RDI).stackPtrBase = -40+stackRegCounter;
                    a->mov(x86::qword_ptr(x86::rbp, lua_Registers.at(REG_RDI).stackPtrBase), x86::rdi);
                    if (ret == x86::rdi) {
                        lvl = -40+stackRegCounter;
                    }
                    stackRegCounter -= 8;
                }
                if (lua_Registers.at(REG_RSI).cntId != _R_TRASHDATA) {
                    // Should save it?
                    lua_Registers.at(REG_RSI).stackPtrBase = -40+stackRegCounter;
                    _q0+=8;
                    a->mov(x86::qword_ptr(x86::rbp, lua_Registers.at(REG_RSI).stackPtrBase), x86::rsi);
                    if (ret == x86::rsi) {
                        lvl = -40+stackRegCounter;
                    }
                    stackRegCounter -= 8;
                }
                if (_emitted_xmmOCX) {
                    // Save xmm0.
                    //a->movq(x86::r9, x86::xmm0);
                    stackRegCounter -= 8;
                    lvl = -40+stackRegCounter;
                    a->movq(x86::qword_ptr(x86::rbp, lvl), x86::xmm0);
                } else {
                    // Save 'saved' register.
                    if (_OPMODE != _L_NONE) {
                        if (_quotient_r) {
                            goto _r;
                        } else {
                            goto _F_line;
                        }
                        if (ret != x86::rdi && ret != x86::rsi) {
                            _r:
                            stackRegCounter -= 8;
                            lvl = -40+stackRegCounter;
                            _q0+=8;
                            a->mov(x86::qword_ptr(x86::rbp, lvl), ret);
                        }
                    }
                }
                _F_line:
                // Move args.
                //if (pointer->ATTRIB == 0) {
                    // Let's check if has a fixed address
                    if (pointer->skipcheck) {
                        // Yay!
                        // Move arguments.
                        qlog0._log2("CallFunc( fixedaddr )::Start   []\n");
                        qlog0._log2("CallFunc( fixedaddr )::_args   []\n");
                        _F_ASM_MAKEFUNCTIONARGUMENTS(&pointer->EXPR, a, scope, false, 0);
                        //qlog0._log2("CallFunc( fixedaddr )::_addr   []\n");
                        //a->ud2();
                        if (pointer->a == nullptr) {
                            m_LuaErrorHandler->reportError(_lua_es_Illegal, 0, "skipcheck=true but pointer are nullptr. Aborting.");
                            abort();
                        }
                        a->call(lua_getPtr(*(Values*)pointer->a));
                        if (_OPMODE == _L_NONE) {
                            _ASM__movToReg(ret, x86::rax);
                            qlog0._log2("CallFunc( fixedaddr )::End   []\n");
                            goto _jmp1;
                        } else {
                            qlog0._log2("CallFunc( fixedaddr )::End   []\n");
                            goto _jmp0;
                        }
                    } else {
                        // Let's find their address.
                        qlog0._log2("CallFunc( common )::Start   []\n");
                        uint64_t ptr0 = (uint64_t)(new uint64_t(0));
                        x86::Gp path = _ASM__getPathToSelGp(pointer->addr->getData(), ret, scope, getPointerInsteadofRawD, true);
                        a->movabs(x86::r9, ptr0);
                        a->mov(x86::qword_ptr(x86::r9), path);
                        _F_ASM_MAKEFUNCTIONARGUMENTS(&pointer->EXPR, a, scope, false, 0);
                        a->movabs(x86::r9, ptr0);
                        a->mov(x86::r8, x86::qword_ptr(x86::r9));
                        a->movabs(x86::r9, PTR_MASK);
                        a->and_(x86::r8, x86::r9);
                        a->call(x86::r8);
                        if (_OPMODE == _L_NONE) {
                            _ASM__movToReg(ret, x86::rax);
                            qlog0._log2("CallFunc( common )::End   [ret]\n");
                        } else {
                            qlog0._log2("CallFunc( common )::End   []\n");
                        }
                    }
                //}
                _jmp0:
                _CPP__turnRegistersAfterCall();
                // Proc it.
                // Restore old number <if it exists>
                if (_emitted_xmmOCX) {
                    // Number proc.
                    
                } else {
                    // To go!
                    if (_OPMODE != _L_NONE) {
                        if (_quotient_r) {
                            if (ret == x86::rax) {
                                if (q0 != x86::noReg)
                                    goto _buffer0;
                                // Occupy other reg.
                                a->mov(x86::rdx, x86::qword_ptr(x86::rbp, lvl));
                                ret = _ASM__runOpCode(_OPMODE, x86::rdx, x86::rax, true);
                                goto _jmp1;
                            }
                            a->mov(ret, x86::qword_ptr(x86::rbp, lvl));
                            ret = _ASM__runOpCode(_OPMODE, ret, x86::rax, true);
                            goto _jmp1;
                        }
                        _buffer0:
                        if (q0 != x86::noReg && ret != x86::rax)
                            a->mov(ret, x86::qword_ptr(x86::rbp, lvl));
                        ret = _ASM__runOpCode(_OPMODE, q0 == x86::noReg ? ret : q0, x86::rax, q0 == x86::noReg);
                        q0 = x86::noReg;
                    }
                }
                _jmp1:
                stackRegCounter += 8+_q0;
                break;
            }
            case _L_PATH: {
                qlog0._log2("\n### _L_PATH START ###\n");
                doomPath0 = pointer;
                // Get the path.. Must be direct.
                //qlog0._log2("[[[GET VARIABLE]]]\n");
                // Prevent overwritiing on data.
                x86::Gp toUse = ret;
                if (lua_Registers.at(_CPP_getRegisterFromASM(ret)).cntId == _R_CLUATYPE_TAGGED) {
                    if (ret != x86::rdx)
                        toUse = x86::rdx;
                    else
                        toUse = x86::rcx;
                }
                if (pos != 0)
                    queue.push_back(_Warning{ret.id(),128,0,0});
                x86::Gp path = _ASM__getPathToSelGp(pointer->addr->getData(), toUse, scope, getPointerInsteadofRawD, true);
                bool _MVKR = true;
                // Use deque if some registers have been modified.
                if (queue.size() > 0 && queue.back().m4 == 1) { // Main register modified.
                    // Restore.
                    a->mov(ret, x86::qword_ptr(x86::rbp, -128));
                    queue.pop_back();
                } // If else. that's good.'
                if (path.id() > 11)
                    q0 = path;
                lType0 = pointer->LuaTYPE;
                _presearched_query = true;
                //qlog0._log2("[[[GOOT VARIABLE]]]\n");
                std::cout << "UpdatedSeekedType: " << std::to_string(pointer->LuaTYPE) << "; Var=" <<pointer->addr->getHeaderVarString()<< std::endl;
                if (pointer->LuaTYPE == 9) {
                    qlog0._log2("pointer->LuaTYPE == LuaNumber [Arithmetic OPMODE]\n");
                    if (_OPMODE == _L_NONE) {
                        _emitted_xmmOCX = true;
                        if (path.id() > 11) {
                            q0 = path;
                            goto _CANCELTHR;
                        } else {
                            _ASM__movToReg(ret, path);
                        }
                    } else {
                        // Proc anything.
                        a->movd(x86::xmm1, path);
                        if (_emitted_xmmOCX) {
                            if (lua_RegistersXMM.at(xmm0).cntId == _R_TRASHDATA) {
                                a->mov(x86::r9, x86::qword_ptr(x86::rbp, lua_RegistersXMM.at(xmm0).stackPtrBase)); // It is weird.
                                a->movd(x86::xmm0, x86::r9);
                            }
                        } else {
                            // Move that number to our registers.
                            a->movd(x86::xmm0, _ASM__keyInstRestoreVar(ret));
                        }
                        _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                        if (_CPP__areThereOpInstruction(k, pos+1).first) {
                            a->movd(ret, x86::xmm0);
                        }
                    }
                    if (_notOpCode) {
                        a->not_(ret);
                    }
                    qlog0._log2("END [Arithmetic OPMODE]\n");
                    break;
                } else if (pointer->LuaTYPE == 8) {
                    
                    if (_OPMODE == _L_NONE) {
                        qlog0._log2("pointer->LuaTYPE == LuaInteger [SAVE]\n");
                        if (path.id() > 11) {
                            q0 = path;
                            goto _CANCELTHR;
                        }
                        _ASM__movToReg(ret, path);
                        qlog0._log2("END [SAVE]\n");
                    } else {
                        qlog0._log2("pointer->LuaTYPE == LuaInteger [Arithmetic OPMODE]\n");
                        if (_emitted_xmmOCX) {
                            // Transform to double in xmm1
                            a->cvtsi2sd(x86::xmm1, path);
                            _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                            a->movd(x86::xmm0, ret);
                        } else {
                            // Uh oh
                            _ASM__GPR_Proc(ret, path, _OPMODE);
                            _use_ret_reg = true;
                        }
                        qlog0._log2("END [Arithmetic OPMODE]\n");
                    }
                    if (_notOpCode) {
                        a->not_(ret);
                    }
                    
                    break;
                } else if (pointer->LuaTYPE == 2) {
                    if (_OPMODE != _L_NONE) {
                        // Holy checks..
                        // wanna had a good time? Go to @case _L_STRING: {
                        // base = path.
                        qlog0._log2("pointer->subkey == _L_NONE [Arithmetic OPMODE]\n");
                        qlog0._log2("start\n");
                        Label _isNumber = a->new_label();
                        Label _end = a->new_label();
                        a->mov(x86::r8, path);
                        a->movabs(x86::r9, (uint64_t)0x7FF0000000000000ULL);
                        a->and_(x86::r8, x86::r9);
                        a->mov(x86::r10, ret);
                        a->and_(x86::r10, x86::r9);
                        a->cmp(x86::r8, x86::r9);
                        a->jne(_isNumber);
                        // Integer.
                        if (_OPMODE != _L_NONE) { // something emitted.
                            if (_emitted_xmmOCX) {
                                a->cvtsi2sd(x86::xmm1, path);
                                _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                                a->movd(ret, x86::xmm0);
                            } else {
                                a->movabs(x86::r9, (uint64_t)0x0000FFFFFFFFFFFFULL);
                                a->and_(path, x86::r9);
                                _ASM__GPR_Proc(x86::r10, path, _OPMODE);
                                a->mov(ret, x86::r10);
                                if (!_CPP__areThereOpInstruction(k, pos+1).first) { //Future op
                                    a->mov(x86::r9, (uint64_t)0x7FF8000000000000ULL);
                                    a->or_(ret, x86::r9);
                                }
                            }
                        }
                        a->jmp(_end);
                        a->bind(_isNumber);
                        // Number.
                        if (_OPMODE != _L_NONE) { // something emitted.
                            if (_emitted_xmmOCX) {
                                a->movd(x86::xmm1, path);
                                _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                                a->movd(ret, x86::xmm0);
                            } else {
                                _ASM__keyInstRestoreVar(ret);
                                a->movabs(x86::r9, (uint64_t)0x0000FFFFFFFFFFFFULL);
                                a->and_(x86::rcx, x86::r9);
                                a->cvtsi2sd(x86::xmm0, x86::rcx);
                                _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                                a->movd(ret, x86::xmm0);
                            }
                        }
                        a->bind(_end);
                    } else if (pointer->LuaTYPE == 4) {
                        if (_OPMODE != _L_NONE) {
                            // Variable already saved IF any opcode instruction was emitted.
                            ret = _ASM__runOpCode(_OPMODE, _ASM__keyInstRestoreVar(ret), path, false);
                            _OPMODE = _L_NONE;
                        }
                    }
                    if (_notOpCode) {
                        a->not_(ret);
                    }
                    _CANCELTHR:
                    qlog0._log2("END [Arithmetic OPMODE]\n");
                    // Update councils.
                    lua_Registers.at(_CPP_getRegisterFromASM(ret)).cntId = _R_CLUATYPE_TAGGED;
                    break;
                }
                if (_OPMODE != _L_NONE) {
                    // Variable already saved IF any opcode instruction was emitted.
                    ret = _ASM__runOpCode(_OPMODE, _ASM__keyInstRestoreVar(ret), path);
                    _OPMODE = _L_NONE;
                } else {
                    if (_MVKR)
                        _ASM__movToReg(ret, path);
                }
                if (_notOpCode) {
                    a->not_(ret);
                }
                // Update councils.
                lua_Registers.at(_CPP_getRegisterFromASM(ret)).cntId = _R_CLUATYPE_TAGGED;
                qlog0._log2("### _L_PATH END ###\n");
                break;
            }
            // Direct medias.
            // string, number, booleans.
            case _L_STRING: {
                // Insert string.
                if (_OPMODE == _L_NONE) {
                    a->movabs(ret, (uint64_t)lua_makeVar(pointer->a, LuaString));
                } else {
                    // Darker yet darker.
                    a->movabs(x86::r9, (uint64_t)lua_makeVar(pointer->a, LuaString));
                    //a->ud2();
                    x86::Gp to_use = q0 == x86::noReg ? ret : q0;
                    x86::Gp a = _ASM__runOpCode(_OPMODE, _ASM__keyInstRestoreVar(to_use), x86::r9, q0 == x86::noReg);
                    _ASM__movToReg(ret, x86::rax);
                    //if (to_use == q0) {
                        q0 = x86::noReg;
                    //}
                    _OPMODE = _L_NONE;
                }
                if (_notOpCode) {
                    a->not_(ret);
                }
                lua_Registers.at(_CPP_getRegisterFromASM(ret)).cntId = _R_CLUATYPE_TAGGED;
                break;
            }
            
            // NUMBERS.
            
            case _L_NUMBER: {
                // Insert those typos.
                if (_OPMODE == _L_NONE) {
                    if (!_CPP__areThereOpInstruction(k, pos+1).first) {
                        if (pointer->ATTRIB) {
                            a->movabs(ret, std::stod(std::string(pointer->_data.begin(), pointer->_data.end())));
                        } else {
                            if (!noTag)
                                a->movabs(ret, ((uint64_t)0x7FF8000000000000ULL | (uint64_t)std::stoi(std::string(pointer->_data.begin(), pointer->_data.end()))));
                            else
                                a->mov(ret, (uint64_t)std::stoi(std::string(pointer->_data.begin(), pointer->_data.end())));
                        }
                    } else {
                        if (pointer->ATTRIB) {
                            a->movabs(x86::r9, std::stod(std::string(pointer->_data.begin(), pointer->_data.end())));
                            a->movd(x86::xmm0, x86::r9);
                            lua_RegistersXMM.at(xmm0).cntId = _R_CLUATYPE_TAGGED;
                            _emitted_xmmOCX = true;
                        } else {
                            if (!noTag)
                                a->movabs(ret, ((uint64_t)0x7FF8000000000000ULL | (uint64_t)std::stoi(std::string(pointer->_data.begin(), pointer->_data.end()))));
                            else
                                a->mov(ret, ((uint64_t)std::stoi(std::string(pointer->_data.begin(), pointer->_data.end()))));
                        }
                    } 
                } else {
                    if (_emitted_xmmOCX) {
                        // Restore the var if modified.
                        if (lua_RegistersXMM.at(xmm0).cntId == _R_TRASHDATA) {
                            a->mov(x86::r9, x86::qword_ptr(x86::rbp, lua_RegistersXMM.at(xmm0).stackPtrBase)); // It is weird.
                            a->movd(x86::xmm0, x86::r9);
                        }
                        a->mov(x86::r9, std::stod(std::string(pointer->_data.begin(), pointer->_data.end())));
                        a->movd(x86::xmm1, x86::r9);
                        // Dont care if it are Vectorial registers... Just data.
                        // Proc those. [Class0 = Vectorial Proc]
                        _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                        a->movd(ret, x86::xmm0);
                        break;
                    }
                    if (q0 != x86::noReg) {
                        if (lType0 == 8) { // Integer
                            if (pointer->ATTRIB) { // Transform q0 to xmm
                                a->cvtsi2sd(x86::xmm0, q0);
                                a->movabs(x86::r9, (uint64_t)std::stod(std::string(pointer->_data.begin(), pointer->_data.end())));
                                a->movd(x86::xmm1, x86::r9);
                                _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                                a->movd(q0, x86::xmm0);
                                //q0 = x86::noReg;
                            } else { // Direct
                                a->mov(x86::r9, std::stoi(std::string(pointer->_data.begin(), pointer->_data.end())));
                                _ASM__GPR_Proc(_ASM__keyInstRestoreVar(q0), x86::r9, _OPMODE);
                                //a->movd(ret, x86::xmm0);
                            }
                        } else if (lType0 == 9) { // Number
                            if (pointer->ATTRIB) { // Direct.
                                a->movabs(x86::r9, (uint64_t)std::stod(std::string(pointer->_data.begin(), pointer->_data.end())));
                                a->movd(x86::xmm1, x86::r9);
                                a->movd(x86::xmm0, q0);
                                _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                                a->movd(q0, x86::xmm0);
                            } else { // Transform current to xmm & move.
                                a->movabs(x86::r9, (uint64_t)std::stod(std::string(pointer->_data.begin(), pointer->_data.end())));
                                a->movd(x86::xmm1, x86::r9);
                                a->movd(x86::xmm0, q0);
                                _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                                a->movd(q0, x86::xmm0);
                            }
                        }
                        break;
                    }
                    
                    if (pointer->ATTRIB) {
                        // Double.. make it be as it should be.
                        a->movd(x86::xmm0, _ASM__keyInstRestoreVar(ret));
                        a->mov(x86::r9, std::stod(std::string(pointer->_data.begin(), pointer->_data.end())));
                        a->movd(x86::xmm1, x86::r9);
                        // [Class0 = Vectorial Proc]
                        _ASM__vectorialProc(x86::xmm0, x86::xmm1, _OPMODE);
                        a->movd(ret, x86::xmm0);
                    } else {
                        // [Class1 = GPR Proc]
                        a->mov(x86::r10, (uint64_t)std::stoi(std::string(pointer->_data.begin(), pointer->_data.end())));
                        _ASM__movToReg(ret, _ASM__keyInstRestoreVar(ret));
                        //a->mov(x86::r8, (uint64_t)0x0000FFFFFFFFFFFFULL);
                        //a->and_(ret, x86::r8);
                        _ASM__GPR_Proc(ret, x86::r10, _OPMODE);
                        if (!noTag) {
                            if (!_CPP__areThereOpInstruction(k, pos+1).first) {
                                // Push some classic LUA
                                a->mov(x86::r9, (uint64_t)0x7FF8000000000000ULL);
                                a->or_(ret, x86::r9);
                            }
                        }
                    }
                }
                break;
            }
        }
        pos++;
        _quotient_r=true;
    }
}
