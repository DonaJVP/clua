// Table management.

/*
 * As i was thinking, how about we change the table form?
 * With arithmetic operations, we should get to the result.
 */

#include "ltable.hpp"
#include "lua.hpp"

//BEGIN MURMUR32

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <sys/mman.h>
#include <sys/ucontext.h>

static inline uint32_t rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

uint32_t murmur3_32(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const size_t nblocks = len / 4;
    
    uint32_t h1 = seed;
    
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    
    // body
    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
    for (size_t i = 0; i < nblocks; i++) {
        uint32_t k1 = blocks[i];
        
        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;
        
        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }
    
    // tail
    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;
    
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
    }
    
    // finalization
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    return h1;
}

//END MURMUR32
//Hasher must be used for strings.

Node *_F_ASM_MAKETABLENREHASH(lua_Table *_T, uint32_t s_) {
    Node *T;
    void *_nT = mmap(nullptr, s_*sizeof(Node), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    memset(_nT, 0, s_*sizeof(Node));
    T = new Node[s_]();
    Node *OLD = _T->nodes;
    _T->nodes = T;
    for (uint64_t i = 0; i <= _T->hsize; i++) {
        if (OLD[i].key && OLD[i].val) {
            _F_ASM_NOTGUARANTEED_SETVALUE(_T, OLD[i].key, OLD[i].val);
        }
    }
    munmap(OLD, _T->hsize);
    _T->hsize = s_;
    return T;
}

#include <iostream>

Values *_F_ASM_NOTGUARANTEED_SETVALUE(lua_Table *t, TString *key, Values v) {
    size_t idx = key->IDX & t->hmask;
    Node* n = &t->nodes[idx];
    //    __asm__ ( "ud2" );
    if (n->key == nullptr) {
        n->key = key;
        n->val = v;
        n->next = nullptr;
        t->used_on_smap++;
        return &n->val; //💜
    }
    if (t->used_on_smap *4 >= t->hmask * 3) { //More than 75%
        //Resize this map.
        t->nodes = _F_ASM_MAKETABLENREHASH(t, (t->hsize*2));
    }
    while (true) {
        if (std::strcmp(n->key->data, key->data) == 0) {
            n->val = v; // overwrite
            return &n->val;
        }
        if (!n->next)
            break;
        n = n->next;
    }
    n->next = new Node{key, v, nullptr};
    t->used_on_smap++;
    return &n->val;
}
Values _F_ASM_NOTGUARANTEED_GETVALUE(lua_Table *t, TString *k, Values nullPtr) {
    Values val = nullPtr;
    size_t idx = k->IDX & t->hmask;
    Node* n = &t->nodes[idx]; // sizeof(Node) * idx
    if (n != nullptr) {
        if (n->key) {
            return n->val;
        } else {
            Node *_n = n->next;
            while (_n) { // o(n)
                if (_n->key) {
                    if (std::strcmp(_n->key->data, k->data) == 0) {
                        return _n->val;
                    }
                    _n = _n->next;
                }
            }
        }
    }
    return val;
}
void *_F_ASM_NOTGUARANTEED_GETPTR(lua_Table *t, TString *k) {
    size_t idx = k->IDX & t->hmask;
    Node* n = &t->nodes[idx];
    if (!n->key) {
        // Not allocated.
        return _F_ASM_NOTGUARANTEED_SETVALUE(t, k, 0);
    } else {
        if (!n->next) {
            return &n->val; // o(5)
        } else {
            Node *_n = n->next;
            while (_n) { // o(n)
                if (_n->key) {
                    if (std::strcmp(_n->key->data, k->data) == 0) {
                        return &_n->val;
                    }
                    _n = _n->next;
                }
            }
        }
    }
    return _F_ASM_NOTGUARANTEED_SETVALUE(t, k, 0);
}
void *_F_ASM_NOTGUARANTEED_GETPTR_NOALLOC(lua_Table *t, TString *k, void *NPTR) {
    size_t idx = k->IDX & t->hmask;
    Node* n = &t->nodes[idx];
    if (!n->key) {
        // Not allocated.
        return NPTR;
    } else {
        if (!n->next) {
            return &n->val; // o(5)
        } else {
            Node *_n = n->next;
            while (_n) { // o(n)
                if (_n->key) {
                    if (std::strcmp(_n->key->data, k->data) == 0) {
                        return &_n->val;
                    }
                    _n = _n->next;
                }
            }
        }
    }
    return NPTR;
}
/*
Values *_F_ASM_NOTGUARANTEED_GETVALUENALLOCATE(lua_Table *t, TString *k, void *nullPtr) {
    Values *val = (Values*)nullPtr;
    size_t idx = k->IDX & t->hmask;
    Node* n = &t->nodes[idx];
    
    if (!n->key) {
        // Not allocated.
        // Then let's allocate this key.
        
        return val;
    } else {
        if (!n->next) {
            return n->val; // o(5)
        } else {
            Node *_n = n->next;
            while (_n) { // o(n)
                if (_n->key) {
                    if (_n->key->IDX == k->IDX) {
                        return _n->val;
                    }
                    _n = _n->next;
                }
            }
        }
    }
    
    return val;
}*/

#include <cmath>

// Helpers
void _clearNode(Node *node) {
    if (node == nullptr)
        return;
    if (node->next != nullptr)
        _clearNode(node->next);
    delete node;
}
void _disableTable(lua_Table *t) {
    munmap(t->array, std::pow(2, t->asize));
    // Clear.
    Node *node = nullptr;
    uint32_t counter = 0;
    while (true) {
        if (t->nodes[counter].val == 0) {
            goto _C;
        }
        if (t->nodes[counter].next != nullptr) {
            _clearNode(t->nodes[counter].next);
        }
        _C:
        counter++;
    }
    munmap(t->nodes, std::pow(2, t->hsize));
}

bool _canBuildTable(std::vector<LuaLexFrame> *vct) {
    // Must be secured. No variables should be allowed. If found any then return false.
    uint32_t pos = 0;
    LuaLexFrame *packet = nullptr;
    while (true) {
        try {
            packet = &vct->at(pos);
        } catch (std::out_of_range &e) {
            break;
        }
        if (packet->key == _L_PATH) {
            return false;
        } else if (packet->key == _L_TABLE) {
            bool status = _canBuildTable(&packet->EXPR_BRKT);
            if (!status)
                return false;
        }
        cont:
        pos++;
    }
    return true;
}

#include "llexiterator.hpp"
// Call when building a table structure. Must be called after the _L_TABLE_START
LuaLexFrame makeSingleTable(std::vector<LuaLexFrame> *vct, uint32_t *pos) {
    LuaLexFrame toRet = LuaLexFrame(_L_TABLE);
    std::vector<LuaLexFrame> *tableVec = &toRet.EXPR_BRKT;
    LuaLexFrame *it = nullptr;
    bool _0_Path = false;
    bool _0_selfContinue = false;
    bool _0_function = false;
    bool _0_declr = false;
    while (true) {
        try {
            it = &vct->at(*pos);
        } catch (std::out_of_range &e) {
            m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, "Script vector closed before table finished.");
            m_LuaErrorHandler->setFatal(true);
            return LuaLexFrame(_L_NONE);
        }
        switch (it->key) {
            // Flow control.
            case _L_CONSTTABLE: {
                toRet.ATTRIB = 1;
                break;
            }
            case _L_TABLE_END: {
                return toRet;
            }
            case _L_DECLR: {
                _0_declr = true;
                break;
            }
            case _L_VARNAME: {
                tableVec->push_back(makeSinglePath(vct, pos));
                *pos = *pos - 1;
                _0_Path = true;
                _0_selfContinue = true;
                break;
            }
            case _L_TABLE_START: {
                tableVec->push_back(makeSingleTable(vct, pos));
                *pos = *pos + 1; // Ignore _L_TABLE_END instruction.
                break;
            }
            case _L_F_ARGS_START: {
                *pos = *pos + 1;
                LuaLexFrame _expr = makeSingleExprP(vct, pos);
                LuaLexFrame _cache = LuaLexFrame(_L_NONE);
                if (tableVec->size() == 0) {
                    // Theres no path.
                    tableVec->push_back(_expr);
                } else {
                    // Theres path or something.
                    LuaLexFrame _tM = std::move(tableVec->back()); // Copy last character.
                    if (_tM.key == _L_PATH) {
                        if (_0_declr) {
                            _0_declr = false;
                            LuaLexFrame tG = LuaLexFrame(_L_DECLR_PLUS_DATA);
                            tableVec->pop_back();
                            tG.local = false;
                            tG.addr = _tM.addr;
                            tG.EXPR = _expr.EXPR;
                            tableVec->push_back(tG);
                            //*pos = *pos + 1;
                            break;
                        } else {
                            _cache.addr = _tM.addr;
                            _cache.key = _L_CALL;
                            _cache.EXPR = _expr.EXPR;
                            tableVec->at(tableVec->size()-1) = _cache;
                        }
                    } else {
                        tableVec->push_back(_expr);
                    }
                }
                *pos = *pos + 1; // Skip _L_F_ARGS_END
                break;
            }
            case _L_FUNCTION: { // Special.
                // Not permittied for now.
                m_LuaErrorHandler->reportError(_lua_es_Illegal, 0, "Function structure inside a table is not allowed!");
                m_LuaErrorHandler->setFatal(true);
                {return LuaLexFrame(_L_NONE);}
                _0_function = true;
                break; 
            }
            default: {
                tableVec->push_back(*it);
                break;
            }
        }
        *pos = *pos + 1;
    }
    return toRet;
}

//BEGIN ASSEMBLER
#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>
using namespace asmjit;
x86::Assembler *a = nullptr;
void _lua_Table__initializeAssembler(x86::Assembler *ptr) { a = ptr; }

_LUA_XMM_REGISTERS _CPP_getXMMfromASM(Reg rId) {
    if (!rId.is_vec128())
        return xmmU;
    return (_LUA_XMM_REGISTERS)rId.id();
}

void _HELPER__runHooksFor(Reg rId_, _R_CONTENTS id) {
    if (rId_.is_vec128()) {
        lua_RegistersXMM.at(_CPP_getXMMfromASM(rId_)).onModified(a, &lua_RegistersXMM.at(_CPP_getXMMfromASM(rId_)));
        lua_RegistersXMM.at(_CPP_getXMMfromASM(rId_)).onModified = __ASM_callback_nothingX_;
        lua_RegistersXMM.at(_CPP_getXMMfromASM(rId_)).cntId = id;
        return;
    }
    x86::Gp rId = x86::Gp::make_r64(rId_.id());
    lua_Registers.at(_CPP_getRegisterFromASM(rId)).onModified(a, &lua_Registers.at(_CPP_getRegisterFromASM(rId)));
    lua_Registers.at(_CPP_getRegisterFromASM(rId)).onModified = __ASM_callback_nothing_;
    lua_Registers.at(_CPP_getRegisterFromASM(rId)).cntId = id;
}

void _H_CPP__turnRegistersAfterCall() {
    _HELPER__runHooksFor(x86::rdi, _R_TRASHDATA);
    _HELPER__runHooksFor(x86::rsi, _R_TRASHDATA);
    _HELPER__runHooksFor(x86::rdx, _R_TRASHDATA);
    _HELPER__runHooksFor(x86::rcx, _R_TRASHDATA);
    _HELPER__runHooksFor(x86::rax, _R_FUNC_RESULT);
    _HELPER__runHooksFor(x86::r10, _R_TRASHDATA);
    _HELPER__runHooksFor(x86::r11, _R_TRASHDATA);
    //_HELPER__runHooksFor(x86::r12, _R_TRASHDATA);
    //_HELPER__runHooksFor(x86::r13, _R_TRASHDATA);
    //_HELPER__runHooksFor(x86::r14, _R_TRASHDATA);
    //_HELPER__runHooksFor(x86::r15, _R_TRASHDATA);
    _HELPER__runHooksFor(x86::r8, _R_TRASHDATA);
    _HELPER__runHooksFor(x86::r9, _R_TRASHDATA);
    // GUH
    lua_RegistersXMM.at(xmm0).cntId = _R_TRASHDATA;
    lua_RegistersXMM.at(xmm1).cntId = _R_TRASHDATA;
    lua_RegistersXMM.at(xmm2).cntId = _R_TRASHDATA;
    lua_RegistersXMM.at(xmm3).cntId = _R_TRASHDATA;
}

#define getRegisterStatus(x) (lua_Registers.at(x).cntId)

void _ASM__copyData(void *newArray, uint64_t bytes, void *oldArray) {
    
}

// first arg = table pointer
void _ASM__checkArraySize(uint64_t tblPTR) {
    if (lua_Registers.at(_CPP_getRegisterFromASM(x86::r9)).cntId != _R_TABLE_POINTER) {
        lua_Registers.at(_CPP_getRegisterFromASM(x86::r9)).onModified(a, &lua_Registers.at(_CPP_getRegisterFromASM(x86::r9)));
        lua_Registers.at(_CPP_getRegisterFromASM(x86::r9)).onModified = __ASM_callback_nothing_;
        a->movabs(x86::r9, tblPTR);
        lua_Registers.at(_CPP_getRegisterFromASM(x86::r9)).cntId = _R_TABLE_POINTER;
    }
    _HELPER__runHooksFor(x86::rdi, _R_TRASHDATA);
    a->mov(x86::rdi, x86::qword_ptr(x86::rcx, offsetof(lua_Table, asize)));
    _HELPER__runHooksFor(x86::rsi, _R_TRASHDATA);
    a->mov(x86::rsi, x86::qword_ptr(x86::rcx, offsetof(lua_Table, used_on_amap))); 
    _HELPER__runHooksFor(x86::r9, _R_TRASHDATA);
    // rdi = asize; rsi = used_on_amap
    Label _END = a->new_label();
    Label _targetOnFailure = a->new_label();
    a->cmp(x86::rsi, x86::rdi);
    a->jb(_END);
    a->inc(x86::qword_ptr(x86::r9, offsetof(lua_Table, asize)));
    //Create new mmap.
    uint64_t *_slot0mem0 = new uint64_t(0);
    uint64_t *_slot1mem0 = new uint64_t(0);
    a->mov(x86::rax, _slot0mem0);
    a->mov(x86::rsi, x86::qword_ptr(x86::r9, offsetof(lua_Table, used_on_amap)));
    a->mov(x86::qword_ptr(x86::rax), x86::rsi);
    // Calc a little bit.
    a->mov(x86::rax, x86::qword_ptr(x86::r9, offsetof(lua_Table, asize)));
    a->mov(x86::rsi, 2);
    a->mul(x86::rsi);
    a->mov(x86::r10, x86::rax);
    // end calc
    a->mov(x86::rax, _slot1mem0);
    a->lea(x86::r9, x86::qword_ptr(x86::r9, offsetof(lua_Table, array)));
    a->mov(x86::qword_ptr(x86::rax), x86::r9);
    _H_CPP__turnRegistersAfterCall();
    a->mov(x86::rax, 0x9); //mmap
    a->xor_(x86::rdi, x86::rdi);
    a->mov(x86::rsi, x86::r10);
    a->mov(x86::rdx, 0x3);
    a->mov(x86::r10, 0x22);
    a->mov(x86::r8, -1);
    a->xor_(x86::r9, x86::r9);
    // syscall for mmap()
    a->syscall();
    a->cmp(x86::rax, 0);
    a->je(_targetOnFailure);
    // Move and copy.
    a->mov(x86::rdi, x86::rax);
    a->movabs(x86::r11, _slot0mem0);
    a->mov(x86::rdx, x86::qword_ptr(x86::r11));
    a->movabs(x86::r11, _slot1mem0);
    a->mov(x86::rsi, x86::qword_ptr(x86::r11));
    a->call((uint64_t)_ASM__copyData);
    // Save that nnnnnnnnnn
    a->movabs(x86::rcx, tblPTR);
    a->lea(x86::rsi, x86::qword_ptr(x86::rcx, offsetof(lua_Table, array)));
    a->mov(x86::qword_ptr(x86::rsi), x86::rax);
    a->jmp(_END);
    a->bind(_targetOnFailure);
    // Crash handler.
    a->bind(_END);
}
void _ASM_r9_save(x86::Assembler *a, _REGISTER_ *reg) {
    a->mov(x86::qword_ptr(x86::rbp, -480), x86::r9);
}
// A little helper
x86::Gp _HELPER_ret_RCONTENT_GP(_REGISTER_ *reg) {
    switch (reg->rID) {
        case REG_RAX: return x86::rax;
        case REG_RSI: return x86::rsi;
        case REG_RCX: return x86::rcx;
        case REG_RDI: return x86::rdi;
        case REG_RDX: return x86::rdx;
        case REG_R10: return x86::r10;
        case REG_R11: return x86::r11;
        case REG_R12: return x86::r12;
        case REG_R13: return x86::r13;
        case REG_R14: return x86::r14;
        case REG_R15: return x86::r15;
        case REG_RBX: return x86::rbx;
    }
    return x86::noReg;
}
greg_t _HELPER_RCONTENT__GP(x86::Gp REG) {
    switch (REG.id()) {
        case x86::Gp::kIdAx: return REG_RAX;
        case x86::Gp::kIdCx: return REG_RCX;
        case x86::Gp::kIdDx: return REG_RDX;
        case x86::Gp::kIdBx: return REG_RBX;
        case x86::Gp::kIdSp: return REG_RSP;
        case x86::Gp::kIdSi: return REG_RSI;
        case x86::Gp::kIdDi: return REG_RDI;
        case x86::Gp::kIdR8: return REG_R8;
        case x86::Gp::kIdR9: return REG_R9;
        case x86::Gp::kIdR10: return REG_R10;
        case x86::Gp::kIdR11: return REG_R11;
        case x86::Gp::kIdR12: return REG_R12;
        case x86::Gp::kIdR13: return REG_R13;
        case x86::Gp::kIdR14: return REG_R14;
        case x86::Gp::kIdR15: return REG_R15;
    }
    return REG_ERR;
}
void _ASM_reg_save(x86::Assembler *a, _REGISTER_ *reg) {
    a->mov(x86::qword_ptr(x86::rbp, -464), _HELPER_ret_RCONTENT_GP(reg));
}
// Core of table builder ASM.
// rbx should not be touched in this context.
// _constTable is a feature which symbols when to modify members data.
/*
 * local _table = {
 *  _CLUA@ConstTable
 *  member0 = "data0",
 *  member1 = "data1",
 * }
 * 
 * -- Then:
 * _table.member0 = "modifiedData0"; -- No crash
 * _table.member2 = "modifiedData2"; -- Crashes (Non mutable table)
 */
x86::Gp lua_genTable__Online(std::vector<LuaLexFrame> *vct, lua_Scope *scope, bool _constTable, lua_Table **tbl) {
    lua_Table *Table = new lua_Table(); // Save in 480.
    if (tbl != nullptr)
        *tbl = Table;
    
    Table->asize = 0xFF; // 255bytes. 32slots of 8bytes
    Table->array = new Values[32];
    Table->hsize = 0xFFFF; // 65536bytes. 8192slots each Node*
    Table->nodes = new Node[2048];
    
    a->movabs(x86::r9, (uint64_t)Table);
    _HELPER__runHooksFor(x86::r9, _R_TABLE_POINTER);
    lua_Registers.at(REG_R9).onModified = reinterpret_cast<_regCallback>(_ASM_r9_save);
    x86::Gp toRet = x86::r9;
    x86::Gp cacheRes = x86::noReg;
    uint32_t pos = 0; 
    LuaLexFrame *frm = nullptr;
    bool _insertedString = false;
    _Lua_Lex_Keys operator_ = _L_NONE;
    _Lua_Lex_Keys _lastData = _L_NONE;
    bool _arrayMode = false;
    TString *keyword = nullptr;
    while (true) {
        try {
            frm = &vct->at(pos);
        } catch (std::out_of_range &e) {
            Table->_BOOL_constTable = _constTable;
            return toRet;
        }
        if (frm->key == _L_DECLR_PLUS_DATA) { // Get their keyword and then proccess their data.
            if (frm->addr->needToResolveAddr()) {
                m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, "Variables inside a table should not be resolveable but direct! -> ");
                m_LuaErrorHandler->setFatal(true);
                a->leave();
                a->ret();
                return x86::noReg;
            }
            keyword = returnCompiledString(frm->addr->getHeaderVarString());
            // Proceed to eval their data.
            _arrayMode = false;
            cacheRes = CLUA_EvalExprNReturn(&frm->EXPR_BRKT, scope, {false, x86::noReg}, false, false, {0, _L_NONE});
            _HELPER__runHooksFor(cacheRes, _R_CLUATYPE_UNTAGGED);
            // Now, we wait.
        } else if (frm->key == _L_SEPARATOR) {
            if (_arrayMode) {
                // [0] = <data>
                // Let's dig into it.
                _ASM__checkArraySize((uint64_t)Table);
                _HELPER__runHooksFor(x86::r9, _R_TABLE_POINTER);
                a->mov(x86::r9, x86::qword_ptr(x86::rbp, -480));
                // Continue;
                // Save cacheRes to the next table pos.
                if (getRegisterStatus(_HELPER_RCONTENT__GP(cacheRes)) != _R_CLUATYPE_UNTAGGED)
                    a->mov(cacheRes, x86::qword_ptr(x86::rbp, -464));
                // Continue;
                _HELPER__runHooksFor(x86::rcx, _R_TRASHDATA);
                a->lea(x86::rcx, x86::qword_ptr(x86::r9, offsetof(lua_Table, array)));
                a->mov(x86::rsi, x86::qword_ptr(x86::r9, offsetof(lua_Table, used_on_amap)));
                // Save
                a->mov(x86::qword_ptr(x86::rcx, x86::rsi), cacheRes);
                a->inc(x86::qword_ptr(x86::r9, offsetof(lua_Table, used_on_amap)));
            } else {
                // <keyword> = <data>
                // Let's use _F_ASM_NOTGUARANTEED_SETVALUE instead of coding pure asm (I could but nope.)
                if (getRegisterStatus(REG_R9) != _R_TABLE_POINTER)
                    a->mov(x86::r9, x86::qword_ptr(x86::rbp, -480));
                if (getRegisterStatus(_HELPER_RCONTENT__GP(cacheRes)) != _R_CLUATYPE_UNTAGGED)
                    a->mov(cacheRes, x86::qword_ptr(x86::rbp, -464));
                if (keyword == nullptr) {
                    m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, "Internal error. TString* == nullptr when <keyword::TString*> = data statement was true.");
                    m_LuaErrorHandler->setFatal(true);
                    a->leave();
                    a->ret();
                    return x86::noReg;
                }
                a->mov(x86::rdi, x86::r9);
                a->movabs(x86::rsi, (uint64_t)keyword);
                if (x86::rdx == cacheRes)
                    a->mov(x86::rdx, cacheRes);
                a->call((uint64_t)_F_ASM_NOTGUARANTEED_SETVALUE);
                _H_CPP__turnRegistersAfterCall();
            }
        } else {
            // Get callings.
            cacheRes = CLUA_EvalExprNReturn(vct, scope, std::pair<bool, x86::Gp>(false, x86::noReg), false, false, std::pair<uint32_t*, _Lua_Lex_Keys>(&pos, _L_SEPARATOR));
            _HELPER__runHooksFor(cacheRes, _R_CLUATYPE_UNTAGGED); // If running on local slots [r12 to r15] it should be untagged.
            
            pos--; // Jesus.
        }
        _lastData = frm->key;
        pos++;
    }
}
//END ASSEMBLER

// Table spec responsibles
// AllowedStringToUse = Allowed variables.
std::pair<bool, lua_Table*> _LTABLE_HELPER__buildTable(std::vector<LuaLexFrame> *vct, uint64_t argPtr0) {
    if (!_canBuildTable(vct))
        return {false, nullptr};
    // HEADER
    lua_Scope *scope = (lua_Scope*)argPtr0;
    bool status = true;
    lua_Table *bTable = new lua_Table();
    bTable->asize = 0xFF; // 255bytes. 32slots of 8bytes
    bTable->array = new Values[32];
    bTable->hsize = 0xFFF;
    bTable->hmask = bTable->hsize;
    bTable->nodes = new Node[0xFFF];//(Node*)malloc(0xFFFF*sizeof(Node));
    memset(bTable->nodes, 0, sizeof(Node)*0xFFF);
    // HELPERS
    uint32_t pos = 0;
    LuaLexFrame *packet = nullptr;
    while (true) {
        try {
            packet = &vct->at(pos);
        } catch (std::out_of_range &e) {
            break;
        }
        switch (packet->key) {
            // Hard style. [x = value]
            case _L_DECLR_PLUS_DATA: {
                if (packet->addr->needToResolveAddr()) {
                    _disableTable(bTable);
                    m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, "Using table addresses inside table.");
                    m_LuaErrorHandler->setFatal(true);
                    return {false, nullptr};
                }
                // Extract varname
                std::string vName = packet->addr->getHeaderVarString();
                TString *vSlot = returnCompiledString(vName);
                // Get data.
                if (!_canBuildTable(&packet->EXPR.at(0))) {
                    return {false, nullptr};
                } else {
                    // Seek at info.
                    uint64_t counter = bTable->used_on_smap;
                    counter++;
                    if (counter >= bTable->asize) {
                        // Rehash anything.
                        bTable->nodes = _F_ASM_MAKETABLENREHASH(bTable, bTable->hsize*2);
                    }
                }
                // Well continue.
                // Must be a indexer at all... lVarControl.cpp
                uint32_t _pos = 0;
                LuaLexFrame value = getExprValue(&packet->EXPR.at(0), &_pos, nullptr, true);
                Values data = 0x0000000000000000ULL; // Make default: NIL
                if (value.key != _L_NONE) {
                    switch (value.key) {
                        case _L_STRING: {
                            // Compile the string object.
                            std::string str = std::string(value._data.begin(), value._data.end());
                            TString *strCmp = returnCompiledString(str);
                            // Save
                            data = lua_makeVar(strCmp, LuaString);
                            break;
                        }
                        case _L_NUMBER: {
                            if (value.ATTRIB) {
                                double val = std::stod(std::string(value._data.begin(), value._data.end()));
                                data = lua_aMakeVarDouble(val);
                            } else {
                                int value_ = std::stoi(std::string(value._data.begin(), value._data.end()));
                                data = lua_makeVar(reinterpret_cast<void*>((uintptr_t)((uint64_t(0x0000000000000000ULL)) & value_)), LuaInteger);
                            }
                            break;
                        }
                        case _L_TRUE: {
                            data = 0x7FF3000000000001ULL;
                            break;
                        }
                        case _L_FALSE: {
                            data = 0x0000000000000000ULL;
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                } else if (value.key == _L_NOP) {
                    return {};
                }
                // Save statement.
                _F_ASM_NOTGUARANTEED_SETVALUE(bTable, vSlot, data);
                break;
            }
            // Basic
            case _L_STRING: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                if (counter >= bTable->asize) {
                    // Allow new map.
                    bTable->asize++;
                    void *m = mmap(nullptr, std::pow(2, bTable->asize), PROT_WRITE | PROT_READ , MAP_PRIVATE, -1, 0);
                    memcpy(m, bTable->array, std::pow(2, bTable->asize-1));
                    munmap(bTable->array, std::pow(2, bTable->asize-1));
                    bTable->array = (Values*)m;
                }
                // // // PROCEED // // //
                bTable->array[counter] = lua_makeVar(packet->a, LuaString); // Too simple.
                bTable->used_on_amap++;
                break;
            }
            case _L_NUMBER: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                if (counter >= bTable->asize) {
                    // Allow new map.
                    bTable->asize++;
                    void *m = mmap(nullptr, std::pow(2, bTable->asize), PROT_WRITE | PROT_READ , MAP_PRIVATE, -1, 0);
                    memcpy(m, bTable->array, std::pow(2, bTable->asize-1));
                    munmap(bTable->array, std::pow(2, bTable->asize-1));
                    bTable->array = (Values*)m;
                }
                // // // PROCEED // // //
                uint64_t pValue;
                LuaType t = LuaNumber;
                if (packet->ATTRIB)
                    pValue = std::stod(std::string(packet->_data.begin(), packet->_data.end()));
                else {
                    // Rewrite and write data.
                    int32_t TC = std::stoi(std::string(packet->_data.begin(), packet->_data.end()));
                    memcpy(&pValue, &TC, 4);
                    t = LuaInteger;
                }
                bTable->array[counter] = t == LuaNumber ? pValue : lua_makeVar((void*)pValue, t);
                bTable->used_on_amap++;
                break;
            }
            case _L_TRUE: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                if (counter >= bTable->asize) {
                    // Allow new map.
                    bTable->asize++;
                    void *m = mmap(nullptr, std::pow(2, bTable->asize), PROT_WRITE | PROT_READ , MAP_PRIVATE, -1, 0);
                    memcpy(m, bTable->array, std::pow(2, bTable->asize-1));
                    munmap(bTable->array, std::pow(2, bTable->asize-1));
                    bTable->array = (Values*)m;
                }
                // // // PROCEED // // //
                bTable->array[counter] = lua_makeVar((void*)0x0000000000000001ULL, LuaBoolean);
                bTable->used_on_amap++;
                break;
            }
            case _L_FALSE: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                if (counter >= bTable->asize) {
                    // Allow new map.
                    bTable->asize++;
                    void *m = mmap(nullptr, std::pow(2, bTable->asize), PROT_WRITE | PROT_READ , MAP_PRIVATE, -1, 0);
                    memcpy(m, bTable->array, std::pow(2, bTable->asize-1));
                    munmap(bTable->array, std::pow(2, bTable->asize-1));
                    bTable->array = (Values*)m;
                }
                // // // PROCEED // // //
                bTable->array[counter] = lua_makeVar((void*)0x0000000000000000ULL, LuaBoolean);
                bTable->used_on_amap++;
                break;
            }
            case _L_TABLE: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                if (counter >= bTable->asize) {
                    // Allow new map.
                    bTable->asize++;
                    void *m = mmap(nullptr, std::pow(2, bTable->asize), PROT_WRITE | PROT_READ , MAP_PRIVATE, -1, 0);
                    memcpy(m, bTable->array, std::pow(2, bTable->asize-1));
                    munmap(bTable->array, std::pow(2, bTable->asize-1));
                    bTable->array = (Values*)m;
                }
                // // // PROCEED // // //
                if (_canBuildTable(&packet->EXPR_BRKT)) {
                    std::pair<bool, lua_Table*> k = _LTABLE_HELPER__buildTable(&packet->EXPR_BRKT, argPtr0);
                    bTable->array[counter] = lua_makeVar(k.second, LuaTable);
                    bTable->used_on_amap++;
                } else {
                    _disableTable(bTable);
                    return {false, nullptr};
                }
                break;
            }
            default: {
                // Nope.
            }
        }
        pos++;
    }
    return {status, bTable};
}

void _LUA_LIBRARY__insert(Values RDI, Values RSI) {
    
}





































































