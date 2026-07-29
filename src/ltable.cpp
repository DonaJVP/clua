// Table management.

/*
 * As i was thinking, how about we change the table form?
 * With arithmetic operations, we should get to the result.
 */

#include "lua.hpp"

//BEGIN MURMUR32

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <sys/mman.h>

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
    T = new Node[s_]();
    Node *OLD = _T->nodes;
    _T->nodes = T;
    for (uint32_t i = 0; i <= _T->hsize; i++) {
        if (((uint64_t)&OLD[i]) != 0)
            _F_ASM_NOTGUARANTEED_SETVALUE(_T, OLD[i].key, OLD[i].val);
    }
    munmap(OLD, s_);
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

bool _canBuildTable(std::vector<LuaLexFrame> *vct, std::vector<std::string> ASTU) {
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
            for (std::string &selString: ASTU) {
                if (packet->addr->getHeaderVarString() == selString) {
                    goto cont;
                }
            }
            return false;
        } else if (packet->key == _L_TABLE) {
            bool status = _canBuildTable(packet->EXPR_BRKT);
            if (!status)
                return false;
        }
        cont:
        pos++;
    }
    return true;
}

// ASSEMBLER
x86::Assembler *a = nullptr;
void _lua_Table__initializeAssembler(x86::Assembler *ptr) { a = ptr; }

void _HELPER__runHooksFor(x86::Gp rId, _R_CONTENTS id) {
    lua_Registers.at(_CPP_getRegisterFromASM(rId)).onModified(a, lua_Registers.at(_CPP_getRegisterFromASM(rId)));
    lua_Registers.at(_CPP_getRegisterFromASM(rId)).onModified = __ASM_callback_nothing_;
    lua_Registers.at(_CPP_getRegisterFromASM(rId)).cntId = id;
}

// first arg = table pointer
void _ASM__checkArraySize(uint64_t tblPTR) {
    if (lua_Registers.at(_CPP_getRegisterFromASM(x86::rcx)).cntId != _R_TABLE_POINTER) {
        lua_Registers.at(_CPP_getRegisterFromASM(x86::rcx)).onModified(a, lua_Registers.at(_CPP_getRegisterFromASM(x86::rcx)));
        lua_Registers.at(_CPP_getRegisterFromASM(x86::rcx)).onModified = __ASM_callback_nothing_;
        a->movabs(x86::rcx, tblPTR);
        lua_Registers.at(_CPP_getRegisterFromASM(x86::rcx)).cntId = _R_TABLE_POINTER;
    }
    _HELPER__runHooksFor(x86::rdi, _R_TRASHDATA);
    a->mov(x86::rdi, x86::qword_ptr(x86::rcx, offsetof(lua_Table, asize)));
    _HELPER__runHooksFor(x86::rsi, _R_TRASHDATA);
    a->mov(x86::rsi, x86::qword_ptr(x86::rcx, offsetof(lua_Table, used_on_amap))); 
    // rdi = asize; rsi = used_on_amap
    Label _END = a->new_label();
    a->cmp(x86::rsi, x86::rdi);
    a->jb(_END);
    a->inc(x86::qword_ptr(x86::rcx, offsetof(lua_Table, asize)));
    //Create new mmap.
    
    
    //
    a->bind(_END);
}

// Table spec responsibles
// AllowedStringToUse = Allowed variables.
// ASM triggers here!
std::pair<bool, lua_Table*> _LTABLE_HELPER__buildTable(std::vector<LuaLexFrame> *vct, std::vector<std::string> AllowedStringToUse, uint64_t argPtr0) {
    if (!_canBuildTable(vct, AllowedStringToUse))
        return {false, nullptr};
    // HEADER
    lua_Scope *scope = (lua_Scope*)argPtr0;
    bool status = false;
    lua_Table *bTable = new lua_Table();
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
                // Get data.
                if (!_canBuildTable(&packet->EXPR_BRKT, AllowedStringToUse)) {
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
                
                break;
            }
            // Basic
            case _L_STRING: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                counter++;
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
                break;
            }
            case _L_NUMBER: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                counter++;
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
                break;
            }
            case _L_TRUE: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                counter++;
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
                break;
            }
            case _L_FALSE: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                counter++;
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
                break;
            }
            case _L_TABLE: {
                // Direct case.
                // // // VERIFY // // //
                uint64_t counter = bTable->used_on_amap;
                counter++;
                if (counter >= bTable->asize) {
                    // Allow new map.
                    bTable->asize++;
                    void *m = mmap(nullptr, std::pow(2, bTable->asize), PROT_WRITE | PROT_READ , MAP_PRIVATE, -1, 0);
                    memcpy(m, bTable->array, std::pow(2, bTable->asize-1));
                    munmap(bTable->array, std::pow(2, bTable->asize-1));
                    bTable->array = (Values*)m;
                }
                // // // PROCEED // // //
                if (_canBuildTable(packet->EXPR_BRKT, AllowedStringToUse)) {
                    std::pair<bool, lua_Table*> k = _LTABLE_HELPER__buildTable(packet->EXPR_BRKT, AllowedStringToUse);
                    bTable->array[counter] = lua_makeVar(k.second, LuaTable);
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





































































