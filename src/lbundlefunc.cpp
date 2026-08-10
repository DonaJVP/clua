#include "lua.hpp"
#include <cstddef>
#include <luaconf.h>
#include <sys/mman.h>
#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <ostream>
#include <sys/types.h>
#include <algorithm>

// This runs on duct tapes, if you remove one this wont work..

using namespace asmjit;
using namespace asmjit::x86;

JitRuntime rt;

void _F_ASM_SEARCHVALUENTHENRETURNRAX(std::vector<LuaLexFrame> *b, uint32_t *pos, x86::Assembler *a, lua_Scope *SCP, std::unordered_map<std::string, uint16_t> *_stack_mem, uint16_t *persize, bool _AllocateIfNotFound);

Values *__ASM_F_ALLOCATEMORESPACEFORARRAYINTABLE_PTR(lua_Table *T, uint64_t S, Values X) {
    uint32_t num = T->asize;
    for (;;) {
        if (num < S) {
            num = num * num;
        } else {
            break;
        }
    }
    //Got num. Now alloc that count
    void *MEM = mmap(nullptr, num, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    memcpy(MEM, T->array, T->asize);
    free((void*)T->array);
    T->array = (Values*)MEM;
    return &T->array[S];
}

Values __ASM_F_ALLOCATEMORESPACEFORARRAYINTABLE(lua_Table *T, uint64_t S, Values X) {
    uint32_t num = T->asize;
    for (;;) {
        if (num < S) {
            num = num * num;
        } else {
            break;
        }
    }
    //Got num. Now alloc that count
    void *MEM = mmap(nullptr, num, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    memcpy(MEM, T->array, T->asize);
    free((void*)T->array);
    T->array = (Values*)MEM;
    return T->array[S];
}

Values __ASM_F_MAKEVAR(void *ptr, LuaType T) {
    return NAN_BASE | (T << 48) | (uintptr_t)ptr;
}

LuaType __ASM_F_GETVARTYPE(Values o) {
    return LuaType((o >> 48) & 0xF);
}

void *__ASM_F_GETPTR(Values k) {
    return (void*)(k & PTR_MASK);
}

void __ASM_F_INDEXOR_NOT_VALID_TSTRING(TString *val) {
    m_LuaErrorHandler->reportError(_lua_es_UnknownDataIdx, 0, std::string("Unknown variable"));
    m_LuaErrorHandler->setFatal(true);
}

void __ASM_F_INDEXOR_NOT_VALID_NUM(uint32_t val) {
    m_LuaErrorHandler->reportError(_lua_es_UnknownDataIdx, 0, std::string("Unknown variable"));
    m_LuaErrorHandler->setFatal(true);
}

void __ASM_F_INDEXOR_NOT_VALID_NULL(uint32_t i) {
    m_LuaErrorHandler->reportError(_lua_es_UnknownDataIdx, 0, std::string("Unknown variable"));
    m_LuaErrorHandler->setFatal(true);
}

void __ASM_F_TABLE_NOT_VALID_OTHERTYPE(uint32_t i) {
    m_LuaErrorHandler->reportError(_lua_es_UnknownDataIdx, 0, std::string("Unknown variable"));
    m_LuaErrorHandler->setFatal(true);
}

TString *__ASM_F_STRINGMANIPULATOR_CONCAT(Values *a0, TString *b) {
    //TString *a, TString *b
    //TString *a = returnIndexOfStringPTR(_a);
    //TString *b = returnIndexOfStringPTR(_b);
    TString *a = (TString*)lua_getPtr(*a0);
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
    return OBJ; //More portable usage.
}

// Some helpers to research variables in a function from arguments
uint8_t searchForValuesOutSideNestedFunc(std::string stringID, lua_Scope *T) {
    //Returns a int8 which should decide which memmap we must use
    //The int64 is the offset
    /*
     * 0: Script Local Map
     * 1: 'this' Function map
     * 2: Upper Function map
     */
    uint8_t mapid = 0x01;
    bool _on_final_scope = false;
    lua_Scope *actual = T;
    bool _reached_limit = false;
    while (true) {
        if (actual->_001) { // Found header.
            //mapid = 0x02; //It has reached the limit, but not to research the variable.
            _reached_limit = true;
        } else if (actual->_002) {
            // Can not go any further. Select this map, and if cant find, search on m_General
            mapid = 0x00;
            _on_final_scope = true;
        }
        if (actual->symbols.find(stringID) != actual->symbols.end()) {
            //Found
            return mapid;
        } else {
            if (!_on_final_scope) {
                if (actual->rSCOPE != nullptr) {
                    actual = actual->rSCOPE;
                    if (_reached_limit) {
                        mapid = 0x02;
                    }
                } else {
                    _on_final_scope = true;
                    //goto _S_GEN;
                }
            } else { // _on_final_scope == true
                //_S_GEN:
                return mapid;
            }
        }
    }
}


//(std::vector<LuaLexFrame> *Keys, LuaTableBase *ENV, uint32_t _LINES, uint32_t &pos) {
void lua_CheckIfOnList(const std::string it, std::vector<std::string> *TR) {
    for (std::string &q: *TR) {
        if (q == it)
            return;
    }
    TR->push_back(it);
}

void lua_checkPathNupdateHV(lua_AddrPath *P, std::unordered_map<std::string, uint32_t> *hv, std::vector<std::string> *TR, lua_Scope *S) {
    ///
    LuaLexFrame HEADER = *P->getHeader();
    lua_CheckIfOnList(P->getHeaderVarString(), TR);
    ///
    std::string a = P->getHeaderVarString();
    if (hv->find(a) != hv->end()) {
        hv->at(a) = hv->at(a) + 1;
    } else {
        //Maybe on another.... Scope?
        lua_Scope *CACHE = S->rSCOPE;
        while (true) {
            if (CACHE != nullptr) {
                if (CACHE->HottestVariables.find(a) != CACHE->HottestVariables.end()) {
                    CACHE->HottestVariables.at(a) = CACHE->HottestVariables.at(a) + 1;
                    hv->insert(std::pair<std::string, uint32_t>(a, CACHE->HottestVariables.at(a))); // Include this so we can put a 'equal' to the actual scope
                    break;
                }
            } else {
                hv->insert(std::pair<std::string, uint32_t>(a, 1)); //Huh?
                break;
            }
        }
    }
}

void lua_checkExprData(lua_Expression *expr, lua_Scope *S, std::vector<std::string> *TR, std::unordered_map<std::string, uint32_t> *hv) {
{return;}
    std::vector<LuaLexFrame> *_ps0;
    LuaLexFrame *_ps2;
    uint16_t _ps1 = 0;
    uint32_t _ps1l = 0;
    while (true) {
        try {
            _ps0 = &expr->at(_ps1);
        } catch (std::out_of_range &e) {
            break;
        }
        while (true) {
            try {
                _ps2 = &_ps0->at(_ps1l);
            } catch (std::out_of_range &e) {
                break;
            }
            if (_ps2->key == _L_PATH) {
                lua_checkPathNupdateHV(_ps2->addr, hv, TR, S);
            }
            _ps1l++;
        }
        _ps1++;
    }
}

void updateVariablesUsageLevelToScope(lua_Scope *MAIN) {
    //Mark the first 4.
    std::unordered_map<std::string, bool> G;
    for (auto &e: MAIN->HottestVariables) {
        G[e.first] = true;
    }
    uint32_t c = MAIN->HottestVariables.size();
    while (c) {
        uint32_t maxpt = 0;
        std::string cache = 0;
        for (auto &i: MAIN->HottestVariables) {
            if (G.at(i.first)) {
                if (i.second >= maxpt) {
                    maxpt = i.second;
                    cache = i.first;
                }
            }
        }
        G.at(cache) = false;
        MAIN->HVtoCompiler.push_back(std::pair<std::string, uint32_t>(cache, maxpt));
        c--;
    }
}

void updateVariablesTypes(lua_Scope *SCP) {
    
}

std::string preCmpPath(LuaLexFrame path) {
    std::string toRet;
    for (LuaLexFrame &i: *path.addr->getData()) {
        if (i.key == _L_VARNAME)
            toRet.append(std::string(i._data.begin(), i._data.end()));
        if (i.key == _L_ON_TO_GO) {
            toRet.append(".");
        }
        if (i.key == _L_EXPRESSION_BRKT) {
            toRet.append("<ExpressionBracket>");
        }
    }
    return toRet;
}

std::string dumpInfoo(std::vector<LuaLexFrame> S, bool putEndNOT = false) {
    std::string _s;
    if (!putEndNOT)
        _s.append("<> ");
    for (LuaLexFrame &i: S) {
        _s.append("$[");
        _s.append(std::to_string(static_cast<int>(i.key)));
        _s.append("]");
        if (i.key == _L_PATH) {
            _s.append("::");
            _s.append(preCmpPath(i));
            _s.append(":: ");
            goto _cnt;
        }
        if (!i.keystring.empty()) {
            _s.append("<");
            _s.append(i.keystring);
            _s.append(">");
            if (i.EXPR.size() > 0)
                _s.append("&");
            else
                _s.append(" ");
        } else {
            if (i.EXPR.size() > 0)
                _s.append("&");
            else
                _s.append(" ");
        }
        if (i.EXPR.size() > 0) {
            _s.append("{");
            _s.append(dumpInfoo(i.EXPR.at(0), true));
            _s.append("} ");
        }
        _cnt:
        asm volatile ( "nop" );
    }
    if (!putEndNOT)
        _s.append(" <>END");
    return _s;
}

x86::Gp _HELPER_PARSEREGISTER_FROMOFFSET(uint32_t crt) {
    switch (crt) {
        case 1:
            return x86::rdi;
        case 2:
            return x86::rsi;
        case 3:
            return x86::rcx;
    }
    return x86::noReg;
}

// Translates from LuaLexFrame keys to compatible scope mode. Which can be used for optimizations and for making ASM code easier.
// When this reaches a function, this will ONLY ignore that part and call it self to build.
std::vector<lua_biOpCode> lua_B_F_OP(std::vector<LuaLexFrame> *Keys, uint32_t *pos, lua_Scope *bulldozer, bool _ONLYFUNC, bool _INSIDEAFUNC) {
    // If _ONLYFUNC then it should start on _L_F_ARGS_END
    std::cout << "KEYS UNOPTIMIZED: " << Keys->size() << " ;;;";
    std::string _c2 = dumpInfoo(*Keys);
    std::cout << _c2 << std::endl;
    std::vector<LuaLexFrame> updated = analizeNupdateConstantsNvars(Keys);
    std::cout << "KEYS OPTIMIZED: " << updated.size() << " ;;;";
    std::string _c = dumpInfoo(updated);
    std::cout << _c << std::endl;
    Keys = &updated;
    std::vector<lua_biOpCode> opcodes;
    opcodes.reserve(64); // Prevent reallocation-triggered copies of partially-init opcodes
    uint16_t scope = 0;
    uint16_t _to_close_args = 0;
    SymbolTable *t_ = &bulldozer->symbols;
    lua_biOpCode cache2;
    lua_Scope *block0 = bulldozer; //Must be created one.
    block0->rSCOPE = nullptr;
    //lock0->lSCOPE = new lua_Scope();
    lua_Scope *_LastBLOCK = block0;
    std::vector<LuaLexFrame> cache;
    std::vector<LuaLexFrame> cache3;
    std::vector<std::string> toRecover; //If not found on actual function search into builder's func map
    bool _decL_S = false;
    bool _declr_L = false;
    bool _declr_sto = false;
    uint64_t *_dir = nullptr;
    bool _for_declr_got = false;
    int16_t _to_close_table = 0;
    uint64_t _m_offset = 0;
    bool _table_Start = false;
    std::vector<LuaLexFrame> _subTable;
    bool _B_D_F = false;
    bool _D_F_I = false;
    bool _F_F_0 = false;
    bool _D_I_T = false;
    bool _D_S_0 = false;
    bool _IF = false;
    bool _FOR = false;
    bool _ELSE = false;
    bool _ELSEIF = false;
    bool _LOCALDEFINED = false;
    uint32_t base_scope = 0;
    uint32_t cnt_scope = 0;
    uint32_t to_reserv = 0;
    LuaLexFrame _LAST_FRAME;
    LuaLexFrame _FRM;
    //pos--;
    uint32_t _pos_lastscope = 0;
    if (_ONLYFUNC) {
        // Allocate space for args.
        for (auto &e: bulldozer->symbols) {
            base_scope++;
        }
    }
    bool _F_F_U = false;
    //a->mov(x86::rdi, (uint64_t)_CACHE0);
    std::vector<LuaLexFrame> _C_C_;
    uint16_t t = 1;
    for (LuaLexFrame &K: *Keys) {
        t++;
    }
    while (true) {
        try {
            _FRM = Keys->at(*pos);
        } catch (std::out_of_range &e) {
            goto _TERM_;
        }
        // Thertiary
        if (_table_Start) {
            // Contents must go to the _subTable array, so we can build it with a optional pointer in online assembly (For function)
            _subTable.push_back(_FRM);
            if (_FRM.key == _L_TABLE_START) {
                _to_close_table++;
            }
            if (_FRM.key == _L_TABLE_END) {
                if (_to_close_table == 0) {
                    //CRASH
                }
                _to_close_table--;
                if (_to_close_table <= 0) {
                    _table_Start = false;
                    continue;
                }
            }
            continue;
        }
        // Secondary
        switch (_FRM.key) { //Determination.
            case _L_BlockEnd: {
                if (_decL_S) {
                    // Crash
                }
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_SCE;
                opcodes.push_back(c);
                
                // Function attributes should be given to compiler, so returning opcodes without memory check is obsolete.
                /*if (_ONLYFUNC) {
                    return opcodes;
                }*/
                // Reverse to blockstart.
                t_ = &_LastBLOCK->rSCOPE->symbols;
                updateVariablesUsageLevelToScope(_LastBLOCK);
                _LastBLOCK = _LastBLOCK->rSCOPE;
                break;
            }
            case _L_BlockStart: {
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_SCP;
                //Create new scope and allocate.
                lua_Scope *nsc = new lua_Scope();
                nsc->rSCOPE = _LastBLOCK;
                nsc->symbols = std::unordered_map<std::string, lua_localSymbol>();
                nsc->HottestVariables = std::unordered_map<std::string, uint32_t>();
                _LastBLOCK->lSCOPE.push_back(nsc);
                t_ = &nsc->symbols;
                nsc->base_slot = _LastBLOCK->base_slot+_LastBLOCK->count;
                nsc->count = 0;
                _LastBLOCK = nsc;
                c.SCOPE = nsc;
                opcodes.push_back(c);
                break;
            }
            case _L_EXPRESSION: {
                // Unique key, maybe it are for IF or FOR methods.
                lua_biOpCode rle;
                if (_FOR) {
                    rle.OPCODE = l_b_o_c_FOR;
                    _FOR = false;
                } else if (_ELSEIF) {
                    rle.OPCODE = l_b_o_c_ELS;
                    _ELSEIF = false;
                } else {
                    rle.OPCODE = l_b_o_c_RLE;
                }
                rle.p = _FRM.EXPR;
                opcodes.push_back(rle);
                break;
            }
            case _L_DECLR_PLUS_DATA: {
                if (_FRM.local) {
                    _LOCALDEFINED = true;
                    if (!_FRM.multipleway) {
                        LuaLexFrame HEADER = *_FRM.addr->getHeader();
                        size_t slot = (_LastBLOCK->base_slot+_LastBLOCK->count+1)*8;
                        _LastBLOCK->count++;
                        lua_localSymbol o;
                        o.slot = slot;
                        o.qID = 2;
                        o.id = std::string(HEADER._data.begin(), HEADER._data.end());
                        _LastBLOCK->symbols.insert(std::pair<std::string, lua_localSymbol>(std::string(HEADER._data.begin(), HEADER._data.end()), o));
                        _m_offset = slot;
                        if (slot >= to_reserv)
                            to_reserv = slot;
                        // Include raw data
                        lua_biOpCode c;
                        c.OPCODE = l_b_o_c_STO;
                        c.p = _FRM.EXPR;
                        c.toMemOffset = _m_offset;
                        opcodes.push_back(c);
                    } else {
                        uint64_t _startpointmem = 0;
                        uint64_t _endpointmem = 0;
                        for (LuaLexFrame &HEADER: _FRM.EXPR_BRKT) { // Theres only labels.
                            size_t slot = (_LastBLOCK->base_slot+_LastBLOCK->count+2)*8;
                            if (_startpointmem == 0)
                                _startpointmem = slot;
                            _LastBLOCK->count = _LastBLOCK->count + 1;
                            lua_localSymbol o;
                            o.slot = slot-8;
                            o.qID = 2;
                            o.id = std::string(HEADER._data.begin(), HEADER._data.end());
                            _LastBLOCK->symbols.insert(std::pair<std::string, lua_localSymbol>(std::string(HEADER._data.begin(), HEADER._data.end()), o));
                            if (slot >= to_reserv)
                                to_reserv = slot;
                            _endpointmem = slot+8;
                        }
                        lua_biOpCode c;
                        c.OPCODE = l_b_o_c_STM;
                        c.p = _FRM.EXPR;
                        c.size = _endpointmem;
                        c.toMemOffset = _startpointmem;
                        c.fixedaddr = (_endpointmem - _startpointmem)*8;
                        c.ATR = 1;
                        opcodes.push_back(c);
                    }
                } else {
                    lua_biOpCode c;
                    c.OPCODE = l_b_o_c_DEC;
                    c.LLF = *_FRM.addr->getData();
                    c.p = _FRM.EXPR;
                    opcodes.push_back(c);
                }
                lua_checkExprData(&_FRM.EXPR, _LastBLOCK, &toRecover, &_LastBLOCK->HottestVariables);
                break;
            }
            case _L_NEWLINE: {
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_SSE;
                opcodes.push_back(c);
                break;
            }
            case _L_SEPARATOR: {
                _L_SEPARATOR_CODE:
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_SSE;
                opcodes.push_back(c);
                break;
            }
            case _L_TABLE_START: {
                if (!_table_Start) {
                    _table_Start = true;
                    _to_close_table++;
                }
                _subTable.push_back(_FRM);
                break;
            }
            case _L_TABLE_END: {
                // Push _subTable to be an dynamic object. Also, with the scope for dynamic variables placement (Online)
                lua_biOpCode t;
                t.OPCODE = l_b_o_c_TBL;
                t.SCOPE = _LastBLOCK;
                t.LLF = _subTable;
                _subTable.clear();
                opcodes.push_back(t);
                break;
            }
            case _L_FUNCTION: {
                if (_FRM.ATTRIB > 0) {
                    lua_Expression args = _FRM.EXPR;
                    lua_Scope *startPoint = new lua_Scope();
                    startPoint->lSCOPE = std::vector<lua_Scope*>();
                    startPoint->rSCOPE = _LastBLOCK;
                    uint32_t _current = 0;
                    uint32_t _counter2 = 0;
                    int32_t rdiPos = 0;
                    int32_t rsiPos = 0;
                    for (std::vector<LuaLexFrame> &arg: args) {
                        if (arg.size() == 0)
                            continue;
                        LuaLexFrame *_LABEL = &arg.at(0);
                        if (_LABEL->key == _L_PATH) { // It can't be _L_VARNAME as we compiled the entire opcodes vector to erradicate manual=>0 keys
                            if (_LABEL->addr->needToResolveAddr()) {
                                m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("Only one variable is allowed per comma, not a full statement."));
                                m_LuaErrorHandler->setFatal(true);
                                return std::vector<lua_biOpCode>();
                            }
                            LuaLexFrame *LABEL = _LABEL->addr->getHeader();
                            ///
                            lua_localSymbol sym;
                            if (_current < 4) {
                                sym.qID = 2;
                                sym.slot = (_current+1)*8;
                                if ((_current*8)<17)
                                    sym.register_ = _HELPER_PARSEREGISTER_FROMOFFSET(_current+1);
                            } else {
                                sym.qID = 9;
                                sym.slot = _counter2*8;
                            }
                            sym.id = _LABEL->addr->getHeaderVarString();
                            m_LuaErrorHandler->reportError(_lua_es_Illegal, 0, std::string("FFF_C: ")+std::to_string(sym.slot)+", "+sym.id);
                            startPoint->symbols.insert(std::pair<std::string, lua_localSymbol>(sym.id, sym));
                            _current++;
                        }
                    }
                    startPoint->lvl = _current;
                    startPoint->toEXbytes = _current*8;
                    startPoint->base_slot = 0;
                    startPoint->count = _current;
                    startPoint->argPos = _helperLua_ArgsPos{0,8};
                    if (!_ONLYFUNC)
                        startPoint->rSCOPE->_001 = true;
                    //pos++;
                    uint32_t _cPos = 0;
                    std::vector<lua_biOpCode> *OPC = new std::vector<lua_biOpCode>(lua_B_F_OP(&_FRM.EXPR_BRKT, &_cPos, startPoint, true, _INSIDEAFUNC));
                    lua_biOpCode op;
                    op.OPCODE = l_b_o_c_FUN;
                    op.FuncPTR2 = OPC;
                    op.SCOPE = startPoint;
                    op.path = _FRM.addr;
                    op.ATR = _FRM.ATTRIB;
                    op._F_LOCAL = _FRM.local;
                    opcodes.push_back(op);
                } else {
                    // How tf did this occur?
                    
                }
                break;
            }
            case _L_CALL: {
                lua_biOpCode c;
                if (_FRM.ATTRIB == 0) {
                    std::vector<LuaLexFrame> _vct;
                    LuaLexFrame Path;
                    Path.addr = _FRM.addr;
                    Path.key = _L_PATH;
                    _vct.push_back(Path);
                    c.LLF = std::move(_vct);
                    c.ATR = 0;
                    c.fixedaddr = _FRM.skipcheck ? ((uint64_t)_FRM.a) : 0;
                } else {
                    c.ATR = 1;
                    c.LLF = _FRM.EXPR_BRKT; // Raw form.
                }
                c.OPCODE = l_b_o_c_CFN;
                c.p = _FRM.EXPR;
                lua_checkExprData(&_FRM.EXPR, _LastBLOCK, &toRecover, &_LastBLOCK->HottestVariables);
                opcodes.push_back(c);
                break;
            }
            case _L_F_ARGS_END: {
                break;
            }
            case _L_FOR: {
                // Using FOR keyword is an direct usage of Locals.
                _LOCALDEFINED = true;
                lua_biOpCode c;
                // INITIALIZE NEW SCOPE.
                //Create new scope and allocate.
                lua_Scope *nsc = new lua_Scope();
                nsc->rSCOPE = _LastBLOCK;
                nsc->symbols = std::unordered_map<std::string, lua_localSymbol>();
                nsc->HottestVariables = std::unordered_map<std::string, uint32_t>();
                _LastBLOCK->lSCOPE.push_back(nsc);
                t_ = &nsc->symbols;
                nsc->base_slot = _LastBLOCK->base_slot+_LastBLOCK->count;
                nsc->count = 0;
                _LastBLOCK = nsc;
                c.SCOPE = nsc;
                // The first two variables (Might be one...) can be saved as stack mem slot!
                LuaLexFrame *_tS0 = nullptr;
                LuaLexFrame *_tS1 = nullptr;
                if (_FRM.EXPR.size() > 0) {
                    LuaLexFrame *_s0 = nullptr;
                    try {
                        _s0 = &_FRM.EXPR.at(0).at(0);
                    } catch (std::out_of_range &e) {
                        m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("Internal error. Bad code for _L_FOR. Not known variable."));
                        m_LuaErrorHandler->setFatal(true);
                        return std::vector<lua_biOpCode>();
                    }
                    if (_s0->key == _L_PATH) {
                        //Save this.
                        _tS0 = _s0;
                        LuaLexFrame HEADER = *_s0->addr->getHeader();
                        size_t slot = (_LastBLOCK->base_slot+_LastBLOCK->count+1)*8;
                        _LastBLOCK->count++;
                        lua_localSymbol o;
                        o.slot = slot;
                        o.qID = 2;
                        o.id = std::string(HEADER._data.begin(), HEADER._data.end());
                        t_->insert(std::pair<std::string, lua_localSymbol>(std::string(HEADER._data.begin(), HEADER._data.end()), o));
                        _m_offset = slot;
                        if (slot >= to_reserv)
                            to_reserv = slot;
                        c.ptr = (void*)returnCompiledString(std::string(HEADER._data.begin(), HEADER._data.end()));
                    }
                    std::vector<LuaLexFrame> *_s1 = nullptr;
                    try {
                        _s1 = &_FRM.EXPR.at(1);
                    } catch (std::out_of_range &e) {
                        goto _ENDZONE;
                    }
                    if (_s1->size() > 0) {
                        LuaLexFrame *_s2 = nullptr;
                        LuaLexFrame *_s3 = nullptr;
                        try {
                            _s2 = &_s1->at(0);
                        } catch (std::out_of_range &e) {
                            goto _ENDZONE;
                        }
                        try {
                            _s3 = &_s1->at(1);
                        } catch (std::out_of_range &e) {
                            goto _ENDZONE;
                        }
                        if (_s2->key == _L_PATH && _s3->key == _L_IN) {
                            _tS1 = _s2;
                            LuaLexFrame HEADER = *_s2->addr->getHeader();
                            size_t slot = (_LastBLOCK->base_slot+_LastBLOCK->count+1)*8;
                            _LastBLOCK->count++;
                            lua_localSymbol o;
                            o.slot = slot;
                            o.qID = 2;
                            o.id = std::string(HEADER._data.begin(), HEADER._data.end());
                            t_->insert(std::pair<std::string, lua_localSymbol>(std::string(HEADER._data.begin(), HEADER._data.end()), o));
                            _m_offset = slot;
                            if (slot >= to_reserv)
                                to_reserv = slot;
                        }
                    } else {
                       // m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("Internal error."));
                       // m_LuaErrorHandler->setFatal(true);
                       // return std::vector<lua_biOpCode>();
                    }
                } else {
                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("Typing spec error. 'for' keyword arguments are nil!"));
                    m_LuaErrorHandler->setFatal(true);
                    return std::vector<lua_biOpCode>();
                }
                _ENDZONE:
                c.OPCODE = l_b_o_c_FOR; 
                c.p = _FRM.EXPR;
                std::vector<LuaLexFrame> toSPT;
                if (_tS0 != nullptr) {
                    toSPT.push_back(*_tS0);
                }
                if (_tS1 != nullptr) {
                    toSPT.push_back(*_tS1);
                }
                //c.LLF = toSPT; //NOTE: Obsolete toSPT
                opcodes.push_back(c);
                break; 
            }
            case _L_AND: {
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_AND;
                opcodes.push_back(c);
                break;
            }
            case _L_NOT: {
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_NOT;
                opcodes.push_back(c);
                break;
            }
            case _L_IF: {
                lua_biOpCode c;
                //Create new scope and allocate.
                lua_Scope *nsc = new lua_Scope();
                nsc->rSCOPE = _LastBLOCK;
                nsc->symbols = std::unordered_map<std::string, lua_localSymbol>();
                nsc->HottestVariables = std::unordered_map<std::string, uint32_t>();
                _LastBLOCK->lSCOPE.push_back(nsc);
                t_ = &nsc->symbols;
                nsc->base_slot = _LastBLOCK->base_slot+_LastBLOCK->count;
                nsc->count = 0;
                _LastBLOCK = nsc;
                c.SCOPE = nsc;
                c.OPCODE = l_b_o_c_IFS; 
                c.p = _FRM.EXPR;
                opcodes.push_back(c);
                break;
            }
            case _L_ELSE: {
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_ELS;
                opcodes.push_back(c);
                break;
            }
            case _L_ELSEIF: {
                _ELSEIF = true;
                break;
            }
            /*case _L_FUNCTION: { //hate this shit
                //function.
                // <function>(a, b, c)   OR   <function> hithere(a, b, c)
                // arguments should stand as a symbol.
                _D_F_I = true;
                break;
            }*/
            default: {
                defaulty:
                //l_b_o_c_LXC : Lexical complex thing
                if (!cache.empty()) {
                    lua_biOpCode k;
                    k.OPCODE = l_b_o_c_VTN;
                    k.LLF = cache;
                    cache.clear();
                }
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_LXC;
                c.KEY = _FRM.key;
                opcodes.push_back(c);
                break;
            }
        }
        *pos = *pos + 1;
        _LAST_FRAME = _FRM;
    }
    _TERM_:
    if (_INSIDEAFUNC)
        abort();
    // SPECIAL NODES
    lua_biOpCode lCf;
    lCf.ATR = _LOCALDEFINED;
    lCf.OPCODE = l_b_o_c_UPV;
    opcodes.push_back(lCf);
    // DEPENDENCIES
    lua_biOpCode dep;
    dep.OPCODE = l_b_o_c_DEP;
    if (_INSIDEAFUNC) {
        //Let's evaluate those variables which need revision
        for (const std::string &s: toRecover) {
            uint8_t pair = searchForValuesOutSideNestedFunc(s, bulldozer);
            if (pair == 0x02) {
                dep.nestedtoUpValues.push_back(s);
            }
        }
    }
    opcodes.push_back(dep);
    // MEMORY
    lua_biOpCode mem;
    mem.OPCODE = l_b_o_c_MEM;
    mem.size = to_reserv;
    opcodes.push_back(mem);
    // Update variables usage.
    opcodes = *lua_Scope::updateHottestVariablesForKeys(block0, &opcodes);
    return opcodes;
}




lua_localSymbol *acquireVariableFromExtensionsPtr(std::string stringID, lua_Scope *T) {
    //Returns a int8 which should decide which memmap we must use
    //The int64 is the offset
    /*
     * 0: Script Local Map
     * 1: 'this' Function map
     * 2: Upper Function map
     */
    uint8_t mapid = 0x01;
    bool _on_final_scope = false;
    lua_Scope *actual = T;
    bool _reached_limit = false;
    
    while (true) {
        if (actual->symbols.find(stringID) != actual->symbols.end()) {
            return &actual->symbols.find(stringID)->second;
        } else {
            if (actual->rSCOPE != nullptr) {
                actual = actual->rSCOPE;
            } else {
                // Limit reached
                return new lua_localSymbol{0, 3, 0, x86::rax, stringID};
            }
        }
    }
}

lua_localSymbol acquireVariableFromExtensions(std::string stringID, lua_Scope *T) {
    //Returns a int8 which should decide which memmap we must use
    //The int64 is the offset
    /*
     * 0: Script Local Map
     * 1: 'this' Function map
     * 2: Upper Function map
     */
    uint8_t mapid = 0x01;
    bool _on_final_scope = false;
    lua_Scope *actual = T;
    bool _reached_limit = false;
    
    while (true) {
        if (actual->symbols.find(stringID) != actual->symbols.end()) {
            return actual->symbols.find(stringID)->second;
        } else {
            if (actual->rSCOPE != nullptr) {
                actual = actual->rSCOPE;
            } else {
                // Limit reached
                lua_localSymbol k;
                k.qID = 3;
                //k.slot = (uint64_t)_F_ASM_NOTGUARANTEED_GETVALUE(m_General, returnCompiledString(stringID), nullptr);
                k.slot = (uint64_t)returnCompiledString(stringID);
                return k;
            }
        }
    }
}

lua_localSymbol acquireVariableFromExtensions(TString *stringID, lua_Scope *T) {
    //Returns a int8 which should decide which memmap we must use
    //The int64 is the offset
    /*
     * 0: Script Local Map
     * 1: 'this' Function map
     * 2: Upper Function map
     */
    uint8_t mapid = 0x01;
    bool _on_final_scope = false;
    lua_Scope *actual = T;
    bool _reached_limit = false;
    
    while (true) {
        if (actual->symbols.find(std::string(stringID->data, stringID->len)) != actual->symbols.end()) {
            return actual->symbols.find(std::string(stringID->data, stringID->len))->second;
        } else {
            if (actual->rSCOPE != nullptr) {
                actual = actual->rSCOPE;
            } else {
                // Limit reached
                lua_localSymbol k;
                k.qID = 3;
                k.slot = (uint64_t)stringID;//(uint64_t)_F_ASM_NOTGUARANTEED_GETVALUE(m_General, stringID, nullptr);
                return k;
            }
        }
    }
}

// ASM

std::pair<x86::Gp, x86::Gp> _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(TString *ID, lua_Scope *scp, x86::Assembler *a, bool tb, bool f_mem, bool _Both = false);
//void _F_ASM_MultiUse_EvalUntil(std::vector<LuaLexFrame> *Keys, x86::Assembler *a, lua_Scope *AS, _Lua_Lex_Keys stop, LuaType *FINALTYPE, bool stackptrReq = false, uint64_t stacksize = 0);
//void _F_ASM_SEARCHVALUE(std::vector<LuaLexFrame> *Keys, uint32_t *pos, x86::Assembler *a, lua_Scope *AS, bool tb);

void _F_ASM_CRASH(const lua_ErrSignals ERR, TString *str) {
    m_LuaErrorHandler->reportError(ERR, 0, std::string(str->data, str->len));
    m_LuaErrorHandler->setFatal(true);
}

LuaType __LEX_KEY_TO_LuaType(_Lua_Lex_Keys a, uint8_t ATTR) {
    switch (a) {
        case _L_STRING: {
            return LuaString;
        }
        case _L_INT: {
            return LuaInteger;
        }
        case _L_DOUBLE: {
            return LuaNumber;
        }
        case _L_NUMBER: {
            if (ATTR) {
                return LuaNumber;
            } else {
                return LuaInteger;
            }
        }
        case _L_BOOL: {
            return LuaBoolean;
        }
        case _L_FALSE: {
            return LuaBoolean;
        }
        case _L_TRUE: {
            return LuaBoolean;
        }
        case _L_NONE: {
            return LuaUnknown;
        }
        default: {
            m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, std::string("Something happened but CLua can't explain."));
        }
    }
    return LuaUnknown;
}

void sss(uint64_t a) {
    std::cout << std::hex << a << std::endl;
}

void _F_ASM_TOOLSET_IsNumber(x86::Assembler *a, x86::Gp a0) {
    a->mov(x86::rcx, a0);
    a->and_(x86::rcx, NAN_MASK);
    a->cmp(x86::rcx, NAN_MASK);
}

void _F_ASM_TOOLSET_IsVarType(x86::Assembler *a, x86::Gp a0, LuaType TYP) {
    if (x86::rcx != a0)
        a->mov(x86::rcx, a0);
    else
        a->mov(x86::rax, a0);
    a->shr(a0, 48);
    a->and_(a0, 0xF);
    a->cmp(a0, TYP);
}

void _F_ASM_MAKEFUNCTIONARGUMENTS(lua_Expression *Args, x86::Assembler *a, lua_Scope *AS, bool give_stackptr, uint32_t stackptrsize) {
    size_t s = Args->size();
    if (s == 0) {
        /*a->mov(x86::rdi, 8);
        a->call((uint64_t)malloc);
        a->xor_(x86::rdx, x86::rdx);
        a->mov(x86::qword_ptr(x86::rax), x86::rdx);*/
        //Give no obj.
        return;
    }
    
    std::vector<x86::Gp> organs{x86::rdi, x86::rsi};
    
    uint64_t *_memSlot0 = new uint64_t(0);
    uint64_t *_memSlot1 = new uint64_t(0);
    
    uint8_t counter = 0;
    bool use_stack = false;
    int16_t stackcounter = 0;
    int16_t stackbase = -128;
    for (std::vector<LuaLexFrame> &A: *Args) {
        if (!use_stack) {
            x86::Gp to_use;
            try {
                to_use = organs.at(counter);
            } catch (std::out_of_range &e) {
                // use stack
                // Save rdi and rsi registers
                a->mov(x86::rdx, (uint64_t)_memSlot0);
                a->mov(x86::qword_ptr(x86::rdx), x86::rdi);
                a->mov(x86::rdx, (uint64_t)_memSlot1);
                a->mov(x86::qword_ptr(x86::rdx), x86::rsi);
                use_stack = true;
                goto _stackusage;
            }
            counter++;
            // Use this register.
            //_F_ASM_MultiUse_EvalUntil(&A, a, AS, _L_NONE, &_a_); // Has RDI
            x86::Gp rUse = CLUA_EvalExprNReturn(&A, AS, std::pair<bool, x86::Gp>(true, to_use), false);
            _ASM__movToReg(to_use, rUse);
            
            if (to_use == x86::rdi) {
                lua_Registers.at(REG_RDI).cntId = _R_FUNC_ARGS;
            } else if (to_use == x86::rsi) {
                lua_Registers.at(REG_RSI).cntId = _R_FUNC_ARGS;
            }
            
            continue;
        }
        _stackusage:
        // rdi and rsi should not be occupied. So we will save them
        LuaType _a_ = LuaUnknown;
        //_F_ASM_MultiUse_EvalUntil(&A, a, AS, _L_SEPARATOR, &_a_);
        x86::Gp rUse = CLUA_EvalExprNReturn(&A, AS, std::pair<bool, x86::Gp>(true, x86::rax), false);
        a->mov(x86::qword_ptr(x86::rbp, stackbase-stackcounter), rUse);
        stackcounter -= 8;
    }
    if (use_stack) {
        // rdx is the third argument, so lets use it
        /*a->lea(x86::rdx, x86::qword_ptr(x86::rbp, stackbase));
        a->mov(x86::rcx, (uint64_t)_memSlot0);
        a->mov(x86::rdi, x86::qword_ptr(x86::rcx));
        a->mov(x86::rcx, (uint64_t)_memSlot1);
        a->mov(x86::rsi, x86::qword_ptr(x86::rcx));*/
        // Restore some registers if theyre pushed away.
    }
    // Uhhuh.
    if (lua_Registers.at(REG_RDI).cntId != _R_FUNC_ARGS) {
        // Restore key.
        _ASM__keyInstRestoreVar(x86::rdi);
        
    }
    if (lua_Registers.at(REG_RSI).cntId != _R_FUNC_ARGS && (Args->size() > 1)) {
        _ASM__keyInstRestoreVar(x86::rsi);
    }
}

// Let's abuse a own compiler bug for optimized entries!
std::unordered_map<std::string, lua_localSymbol*> _GENERAL_SAVEDVARS;
lua_localSymbol *searchSavedGeneralVars(const std::string id) {
    if (_GENERAL_SAVEDVARS.find(id) ==  _GENERAL_SAVEDVARS.end())
        return nullptr;
    return _GENERAL_SAVEDVARS.at(id);
}

static uint64_t PTRMASK = 0x0000FFFFFFFFFFFFULL;
static uint64_t CNTMASK = 0xFFFF000000000000ULL;
bool _0_0_0_CMPTIME_ASM_isScript = false;
void *_0_0_0_CMPTIME_ASM_scriptMem = nullptr;
int32_t _0_0_0_CMPTIME_ASM_localStackFrameBytes = 0;

// f_mem = force memory get/save
// tb = save/load
// a = Assembler
// ID = TString
// scp = Operating Scope
std::pair<x86::Gp, x86::Gp> _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(TString *ID, lua_Scope *scp, x86::Assembler *a, bool tb, bool f_mem, bool _Both) {
    _HELPER__runHooksFor(x86::rdi, _R_TRASHDATA);
    _HELPER__runHooksFor(x86::rsi, _R_TRASHDATA);
    lua_localSymbol var;
    lua_localSymbol *_HV_var = searchSavedGeneralVars(std::string(ID->data, ID->len));
    if (_HV_var != nullptr) {
        if (tb)
            goto _getNormalPointer;
        var = *_HV_var;
    } else {
        _getNormalPointer:
        var = acquireVariableFromExtensions(ID, scp);
    }
    if (var.qID == 3 && var.slot == 0) {
        var.slot = (uint64_t)ID;
    }
    //DEBUG
    x86::Gp sReg = x86::noReg;
    if (!f_mem) { // Search from memory if false
        if (var.cacheReg > 0) {
            x86::Gp toReg;
            switch (var.cacheReg) {
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
            //a->mov(x86::rdi, toReg);
            return {toReg, x86::noReg};
        }
    }
    switch (var.qID) {
        case 0: { // Local map
            //Get variable from 0
            // Local script map
            a->lea(x86::rsi, x86::qword_ptr(x86::rbp, -24));
            if (tb)
                a->lea(x86::rdi, x86::qword_ptr(x86::rsi, var.slot));
            else
                a->mov(x86::rdi, x86::qword_ptr(x86::rsi, var.slot));
            break;
        }
        case 1: { // FuncArgs*
            //Get variable from 1
            // Our map
            a->lea(x86::rsi, x86::qword_ptr(x86::rbp, -16));
            //a->mov(x86::rax, x86::qword_ptr(x86::rsi, var.second.slot));
            if (tb)
                a->lea(x86::rdi, x86::qword_ptr(x86::rsi, var.slot));
            else
                a->mov(x86::rdi, x86::qword_ptr(x86::rsi, var.slot));
            break;
        }
        case 2: { // rbp-520+x = Locals
            if (!_0_0_0_CMPTIME_ASM_isScript) {
                if (var.register_ != x86::rax) {
                    if (tb)
                        goto _continueSadly;
                    if (lua_Registers.at(_CPP_getRegisterFromASM(var.register_)).cntId == _R_FUNC_ARGS_ENTRY)
                        return {var.register_, x86::noReg};
                }
                _continueSadly:
                a->lea(x86::rsi, x86::qword_ptr(x86::rbp, -520));
                if (tb)
                    a->lea(x86::rdi, x86::qword_ptr(x86::rsi, -var.slot));
                else
                    a->mov(x86::rdi, x86::qword_ptr(x86::rsi, -var.slot));
                if (_Both)
                    a->lea(x86::rsi, x86::qword_ptr(x86::rsi, -var.slot));
                sReg = x86::rsi;
            } else {
                a->movabs(x86::rsi, (uint64_t)_0_0_0_CMPTIME_ASM_scriptMem);
                if (tb)
                    a->lea(x86::rdi, x86::qword_ptr(x86::rsi, var.slot));
                else
                    a->mov(x86::rdi, x86::qword_ptr(x86::rsi, var.slot));
                if (_Both)
                    a->lea(x86::rsi, x86::qword_ptr(x86::rsi, var.slot));
                sReg = x86::rsi;
            }
            break;
        }
        case 4: { // rsp+16 = UpFuncScope
            //Get variable from upper func scope
            a->lea(x86::rsi, x86::qword_ptr(x86::rbp, -24));
            a->mov(x86::rdi, x86::qword_ptr(x86::rsi, var.slot));
            break;
        }
        case 3: { // m_General
            //Search online
            a->mov(x86::rsi, var.slot);
            a->mov(x86::rdi, (uint64_t)m_General);
            a->xor_(x86::rdx, x86::rdx);
            if (tb) 
                a->call((uint64_t)_F_ASM_NOTGUARANTEED_GETPTR);    
            else
                a->call((uint64_t)_F_ASM_NOTGUARANTEED_GETVALUE);
            a->mov(x86::rdi, x86::rax);
            break;
        }
        case 9: {
            // Uhhuh.
            a->lea(x86::rsi, x86::qword_ptr(x86::rbp, -496));
            a->mov(x86::rdi, x86::qword_ptr(x86::rdi, var.slot));
            break;
        }
    }
    lua_Registers.at(REG_RDI).cntId = _R_TRASHDATA;
    lua_Registers.at(REG_RSI).cntId = _R_TRASHDATA;
    return {x86::rdi, sReg};
}


//3000~


//If found some upvalues and O == true, then make a third map and put it
void _F_ASM_Copy64bitValue(x86::Assembler *a, uint32_t pos, void *mem) {
    a->mov(x86::r8, (uint64_t)mem);
    a->mov(x86::r9, x86::qword_ptr(x86::r8, pos));
    a->mov(x86::rax, x86::r9);
}
//asm = ud2

void lua_initializeRuntime() {
    //rt = JitRuntime();
}

static std::string dumpInfo(std::vector<lua_biOpCode> S) {
    std::string _s;
    _s.append(" ");
    for (lua_biOpCode &i: S) {
        _s.append("$");
        _s.append(std::to_string(static_cast<int>(i.OPCODE)));
        //_s.append("[" + i.KEY + ";" + i.toMemOffset + "]");
        _s.append(" ");
    }
    return _s;
}

uint8_t _getForTypeExpression(lua_Expression *K) {
    /*
     * 0 = Unknown                  !!!
     * 1 = Number count             i = 0,2[,3]
     * 2 = First-way iteration      a in <>
     * 3 = Double-way iteration     a,b in <>
     */
    if (K->size() > 0) {
        std::vector<LuaLexFrame> *_v0 = &K->at(0);
        if (_v0->size() == 1) { // You cannot had a expression for 'for' like this: for a do [Be in hell.]
            // Double way iteration.
            return 3;
        }
        // Number count or First way iteration.
        LuaLexFrame *_k0 = nullptr;
        LuaLexFrame *_k1 = &_v0->at(1); // Must be 'in' or 'equal'
        switch (_k1->key) {
            case _L_IN: {
                // First way.
                return 2;
            }
            case _L_DECLR: {
                // Number count.
                return 1;
            }
            default: {
                m_LuaErrorHandler->reportError(_lua_es_UnknownDataIdx, 0, std::string("Not known: ")+std::to_string(_k1->key));
            }
        }
    }
    return 0;
}

static bool _upperVarsNotRequiredHighRegistersSlot = true;

void frontNlowerPushes(x86::Assembler *a, std::vector<lua_biOpCode> *quote, bool way) {
    // Uh oh.
    lua_biOpCode *code = &quote->at(quote->size()-3);
    if (code->OPCODE != l_b_o_c_UPV) {
        m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, std::string("Expected OpCode{l_b_o_c_UPV=19} doesn't exist at posTotal-3"));
        m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, std::string("Got: ") + std::to_string(code->OPCODE));
        m_LuaErrorHandler->setFatal(true);
    }
    if (code->ATR == 0) {
        if (!way) {
            a->pop(x86::rbx);
        } else {
            a->push(x86::rbx);
        }
        _upperVarsNotRequiredHighRegistersSlot = true;
        return;
    }
    _upperVarsNotRequiredHighRegistersSlot = false;
    if (way) {
        a->push(x86::r12);
        a->push(x86::r13);
        a->push(x86::r14);
        a->push(x86::r15);
        a->push(x86::rbx);
    } else {
        a->pop(x86::rbx);
        a->pop(x86::r15);
        a->pop(x86::r14);
        a->pop(x86::r13);
        a->pop(x86::r12);
    }
}

// a = Compiler
// Scope = 'Right Now' scope
// Symbols = Actual symbols [Used variables on registers]
void updateCacheRegisters(x86::Assembler *a, lua_Scope *Scope, std::unordered_map<uint8_t, lua_localSymbol*> *Symbols) {
    if (_upperVarsNotRequiredHighRegistersSlot) {
        return;
    }
    // Update registers.
    // Iterate with the first 4 items available on the scope side.
    uint8_t _register_0 = 0;
    std::vector<uint8_t> freeRegisters; // Search for free registers as we go. But do not delete those registers which we need data.
    //std::unordered_map<uint8_t, bool> _notAllowedHV;
    bool canOccupyReg = true;
    uint8_t _register = 0;
    lua_localSymbol *sym = nullptr;
    std::unordered_map<uint8_t, bool> _optRegisterAlloc;
    // if (Scope->HVtoCompiler.size() == 0) {
        for (uint8_t i = 0; i < 4; i++) {
            lua_localSymbol *sym = Symbols->at(i);
            if (sym) {
                _optRegisterAlloc[i] = true;
            } else
                _optRegisterAlloc[i] = false; // Do not take or save a register which has unknown data
            /*qlog0._log2(">>> Saving ");
            qlog0._log2(sym->id.c_str());
            qlog0._log2(" :: ");
            qlog0._log2(std::to_string(sym->cacheReg).c_str());
            qlog0._log2("\n");
            _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(returnCompiledString(sym->id), Scope, a, true, true);
            // Must put their type back.
            //a->mov(x86::rsi, );
            a->mov(x86::qword_ptr(x86::rdi), _ASMH__parseVarCacheRef(sym->cacheReg));
            qlog0._log2(">>> END SAVE ");
            qlog0._log2(sym->id.c_str());
            qlog0._log2(" :: ");
            qlog0._log2(std::to_string(sym->cacheReg).c_str());
            qlog0._log2("\n");
            sym->cacheReg = 0;*/
        }
        // return;
    // }
    while (_register_0 < 4) {
        // Get name
        std::string name;
        try {
            auto _item = Scope->HVtoCompiler.at(_register_0);
            name = _item.first;
        } catch (std::out_of_range &e) {break;}
        // Proceed.
        canOccupyReg = true;
        _register = 0;
        sym = nullptr;
        bool _jmp0 = false;
        while (_register < 4) { // See which we do not need to delete.
            try {
                sym = Symbols->at(_register);
            } catch (std::out_of_range &e) {break;}
            if (sym != nullptr && sym->id == name) {
                qlog0._log2(("TO_BE_HANDLED: {"+name+", ptr="+std::to_string((uint64_t)sym)+"}\n").c_str());
                qlog0._log2(">>> Can't occupy regSlot: ");
                qlog0._log2(std::to_string(_register+1).c_str());
                qlog0._log2(" :: ");
                qlog0._log2(sym->id.c_str());
                qlog0._log2(" :: cacheReg=");
                qlog0._log2(std::to_string(sym->cacheReg).c_str());
                qlog0._log2("\n");
                canOccupyReg = false;
                _optRegisterAlloc[_register] = false;
                goto _justSum;
            } else if (sym && sym->id != name) {
                // Maybe need to save.
                /// OPTIMIZATION: If register of this obj not used, then make this remaining.
                _optRegisterAlloc[_register] = true;
                /*if (sym->cacheReg > 0) {
                    
                }*/
                //std::cout << "AJDSKASJDKASDJKSDJAKDJLWKNDNALSKDASJDQWJIJAOIDCAJOIWJROIAJWDI" << std::endl;
            }
            _register++;
        }
        qlog0._log2("Occupying for: ");
        qlog0._log2(name.c_str());
        qlog0._log2("\n");
        if (canOccupyReg) {
            freeRegisters.push_back(_register_0);
            //Symbols->at(_register_0) = nullptr;
        }
        _justSum:
        _register_0++;
    }
    // Use freed registers.
    uint8_t _c = 0;
    bool _putMaskIfUsed = false;
    bool _ignoreSubRegisterOptimizations = false;
    if (freeRegisters.size() == 0)
        goto _reCheckVars__SAVE;
    while (_c < 4) {
        std::string name;
        qlog0._log2("Seeking for var loading..\n");
        try {
            auto _item = Scope->HVtoCompiler.at(_c);
            name = _item.first;
        } catch (std::out_of_range &e) {break;}
        lua_localSymbol *slot = acquireVariableFromExtensionsPtr(name, Scope);
        qlog0._log2(("TO_BE_USED: {"+name+", ptr="+std::to_string((uint64_t)slot)+"}\n").c_str());
        uint8_t track = freeRegisters.size();
        uint8_t _st0 = 1;
        if (slot->cacheReg == 0) {
            uint8_t regId;
            if (track > 0) {
                regId = freeRegisters.back();
            } else {
                break;
            }
            
            if (_ignoreSubRegisterOptimizations)
                goto _ignoreOptimizationMethods;
            // Track optional registers. 
            if (track == 1 && slot->qID != 3) // Ignore the m_General ones.
                goto _ignoreOptimizationMethods;
            if (_optRegisterAlloc.at(regId)) {
                // if (track > 1) {
                    // We're using the back register.
                    _repeat:
                    try {
                        regId = freeRegisters.at(freeRegisters.size()-_st0);
                    } catch (std::out_of_range &e) {
                        // No available registers, so continue with the first.
                        regId = freeRegisters.back();
                        _ignoreSubRegisterOptimizations = true;
                        goto _ignoreOptimizationMethods;
                    }
                    _st0++;
                    if (_optRegisterAlloc.at(regId)) {
                        goto _repeat;
                    }
                // }
            }
            _ignoreOptimizationMethods:
            freeRegisters.pop_back();
            if (_optRegisterAlloc.at(regId)) {
                if (slot->qID == 3) {// Do not overwrite a used value if General Variable
                    _c++;
                    continue;
                }
                _optRegisterAlloc[regId] = false;
                // Couldn't resolve, so erase it gracefully.
                lua_localSymbol *sym = Symbols->at(regId);
                qlog0._log2(">>> Saving ");
                qlog0._log2(sym->id.c_str());
                qlog0._log2(" :: ");
                qlog0._log2(std::to_string(sym->cacheReg).c_str());
                qlog0._log2("\n");
                _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(returnCompiledString(sym->id), Scope, a, true, true);
                // Must put their type back.
                //a->mov(x86::rsi, );
                a->mov(x86::qword_ptr(x86::rdi), _ASMH__parseVarCacheRef(sym->cacheReg));
                qlog0._log2(">>> END SAVE ");
                qlog0._log2(sym->id.c_str());
                qlog0._log2(" :: ");
                qlog0._log2(std::to_string(sym->cacheReg).c_str());
                qlog0._log2("\n");
                sym->cacheReg = 0;
            }
            _st0 = 0;
            if (slot->qID == 3) { // Let's use a nonwritable
                qlog0._log2("{non writable rXX reg for m_General variable}\n");
                lua_localSymbol *q = new lua_localSymbol;
                //q->slot = slot->slot;
                memcpy(&q->slot, &slot->slot, 8);
                q->cacheReg = regId+1;
                q->qID = 3;
                q->id = name;
                _GENERAL_SAVEDVARS[slot->id] = slot;
            }
            if (!_putMaskIfUsed) {
                a->mov(x86::r9, (uint64_t)PTR_MASK);
                _putMaskIfUsed = true;
            }
            qlog0._log2(">>> Loading ");
            qlog0._log2(slot->id.c_str());
            qlog0._log2("\n");
            _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(returnCompiledString(slot->id), Scope, a, false, true);
            a->and_(x86::rdi, x86::r9);
            a->mov(_ASMH__parseVarCacheRef(regId+1), x86::rdi);
            slot->cacheReg = regId+1;
            qlog0._log2(">>> Ended load ");
            qlog0._log2(slot->id.c_str());
            qlog0._log2("\n");
            if (slot->qID != 3)
                Symbols->at(regId) = slot;
        }
        _c++;
    }
    /// Maybe save those vars which aren't saved at all.
    _reCheckVars__SAVE:
    uint8_t c__ = 0;
    while (c__ < 4) {
        if (_optRegisterAlloc.at(c__)) {
            lua_localSymbol *sym = Symbols->at(c__);
            qlog0._log2(">>> Saving ");
            qlog0._log2(sym->id.c_str());
            qlog0._log2(" :: ");
            qlog0._log2(std::to_string(sym->cacheReg).c_str());
            qlog0._log2("\n");
            _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(returnCompiledString(sym->id), Scope, a, true, true);
            // Must put their type back.
            //a->mov(x86::rsi, );
            a->mov(x86::qword_ptr(x86::rdi), _ASMH__parseVarCacheRef(sym->cacheReg));
            qlog0._log2(">>> END SAVE ");
            qlog0._log2(sym->id.c_str());
            qlog0._log2(" :: ");
            qlog0._log2(std::to_string(sym->cacheReg).c_str());
            qlog0._log2("\n");
            sym->cacheReg = 0;
        } else {
            std::cout << "AKSDLASDKASDNANDKNWDNQI:OANODKAHFKJAS" << " = " << std::to_string(c__) << std::endl; 
        }
        c__++;
    }
}

x86::Gp id_to_reg(uint8_t x) {
    x86::Gp toReg = x86::noReg;
    switch (x) {
        case 0: {
            toReg = x86::r12;
            break;
        }
        case 1: {
            toReg = x86::r13;
            break;
        }
        case 2: {
            toReg = x86::r14;
            break;
        }
        case 3: {
            toReg = x86::r15;
            break;
        }
    }
    return toReg;
}

uint8_t rIdTo_symbol(uint32_t i) {
    return static_cast<uint8_t>(i) - 12;
}

std::pair<bool, x86::Gp> areThisVarInHotVars(std::string vname, std::unordered_map<uint8_t, lua_localSymbol*> *Symbols) {
    uint8_t n = 0xFF;
    for (uint8_t i = 0; i < 4; i++) {
        lua_localSymbol *smb = Symbols->at(i);
        // Proceed.
        if (smb == nullptr)
            continue;
        if (smb->id == vname) {
            n = i;
            break;
        }
    }
    if (n != 0xFF) {
        return {true, id_to_reg(n)};
    }
    return {false, x86::noReg};
}

static void dumpinf(std::vector<lua_biOpCode> *c) {
    std::cout << "BIOPCODES: ";
    for (lua_biOpCode &a: *c) {
        std::cout << "$(" << std::to_string(a.OPCODE);
        if (a.OPCODE == l_b_o_c_LXC) {
            std::cout << ";LXC=$[" << std::to_string(a.KEY) << "]) ";
        } else if (a.OPCODE == l_b_o_c_UPV) {
            std::cout << ";SlotsAsRegisters=" << (a.ATR ? "true" : "false") << ") ";
        } else {
            std::cout << ") ";
        }
    }
    std::cout << '\n';
    
}

/*
 * Closures type:
 * 1: IF
 * 2: FOR
 * 3: WHILE
 * 4: REPEAT
 */

struct _closure_helper {
    x86::Gp _uReg = x86::noReg;
    x86::Gp _toCmp = x86::noReg;
    x86::Gp _stepReg = x86::noReg;
    uint8_t closureType = 0;
    int64_t toCmp_ = 0;
    int64_t step_ = 0;
    TString *_vName;
};

static std::string dumpinf1(std::vector<LuaLexFrame> *c) {
    std::string q0 = "";
    std::cout << "EvaluateExpression: ";
    for (LuaLexFrame &a: *c) {
        q0.append("$[");
        q0.append(std::to_string(a.key));
        q0.append("]; ");
    }
    return q0;
}

uint32_t _stackframe_getMaxValue(uint64_t args) {
    uint64_t _aRes = args*8;
    uint64_t res = 16;
    while (true) {
        
    }
}

// Search if the var are in symbols.

#include <deque>
StringLogger qlog0;

// Precompiler [Uses _Lua_Keywords_Asm for optimization and labeling]
void *luaBundleFunction(std::vector<lua_biOpCode> *_CODE, lua_Scope *THREADRIPPER, bool _online_gen, void *F_MEM_UF, void *F_MEM_SCR, bool Script) {
    // Clear last buffer.
    qlog0.clear();
    if (_online_gen) {
        lua_Registers.at(REG_RDI).cntId = _R_FUNC_ARGS_ENTRY;
        lua_Registers.at(REG_RSI).cntId = _R_FUNC_ARGS_ENTRY;
        lua_Registers.at(REG_RCX).cntId = _R_FUNC_ARGS_ENTRY;
    }
    dumpinf(_CODE);
    FunctionPointer FUNC;
    FuncArgs *ARGS;
    std::vector<lua_biOpCode> CODE = std::vector<lua_biOpCode>(*_CODE);
    FuncArgs *f_t_ = nullptr;
    //std::vector<_lua_Keywords_Asm> ASM_INSTR;
    std::unordered_map<uint8_t, lua_localSymbol*> *symbols = new std::unordered_map<uint8_t, lua_localSymbol*>({
        {0, nullptr},
        {1, nullptr},
        {2, nullptr},
        {3, nullptr},
    });
    std::deque<std::pair<Label, Label>> scopeBlocks;
    lua_Scope *ActualScope;
    bool _FOR = false;
    uint32_t IF_statements;
    // Assembly [ASMJIT]
    CodeHolder code;
    //rtS.resize(rtS.size());
    code.init(rt.environment());
    x86::Assembler a(&code);
    code.set_logger(&qlog0);
    initializeRegistersData((void*)&a);
    _lua_Table__initializeAssembler(&a);
    /*FuncDetail fn_;
    fn_.init(FuncSignature::build<FuncArgs*, FuncArgs*>(), rt.environment());
    FuncFrame frame;
    frame.init(fn_);
    
    //Put arguments where they need to go.
    FuncArgsAssignment args(&fn_);
    args.assign_all(x86::rdi);
    args.update_func_frame(frame);
    frame.finalize();
    
    //Emit
    a.emit_prolog(frame);*/
    
    // magic
    lua_biOpCode cache;
    lua_1_biOpCode _LK = l_b_o_c_NUL;
    uint32_t pos = 0;
    Label k1;
    Label k2;
    Label k3;
    Label k4;
    //FuncSignature sig = FuncSignature::build<FuncArgs*, FuncArgs*>(CallConv::kIdHost);
    //a.emit_prolog(sig);
    a.push(x86::rbp);
    a.mov(x86::rbp, x86::rsp);
    frontNlowerPushes(&a, _CODE, true);
    uint16_t persize = 0;
    std::unordered_map<std::string, uint16_t> _stack_mem;
    //Args = (FuncArgs*)rdi
    //ScrM = (void*)rsi // Script Memory Map
    //FunM = (void*)rdx // Upper Function Memory Map
    //Upper function
    void *fMem;// = 
    uint64_t *m64bit = (uint64_t*)F_MEM_UF;
    //Must create or place an existing memory map for locals management.
    //If this is an live function creation, it should ever exist on rsi as uint64_t address
    //Make a own mmap for this.
    lua_biOpCode tMem = _CODE->at(_CODE->size()-1);
    uint32_t s = tMem.size;
    uint32_t finalAllocMem = 0;
    if (!Script) {
        //May sum some other bytes for those variables that are outside this @nested function
        bool _allocatedMemorySave = false;
        if (!_CODE->at(_CODE->size()-2).nestedtoUpValues.empty()) {
            _allocatedMemorySave = true;
            // Alloc
            size_t tAlloc = _CODE->at(_CODE->size()-2).nestedtoUpValues.size();
            tAlloc--;
            fMem = mmap(nullptr, tAlloc*8, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            // Sum to the offset of our main variables.
            uint32_t base = 0;
            std::vector<std::pair<std::string, lua_localSymbol>> Alloc;
            a.movabs(x86::r10, (uint64_t)fMem);
            for (std::string &var: _CODE->at(_CODE->size()-2).nestedtoUpValues) {
                //Time to recover those vars
                lua_localSymbol k;
                k.qID = 4;
                k.slot = base;
                Alloc.push_back(std::pair<std::string, lua_localSymbol>(var, k)); //Register the local
                a.mov(x86::rax, (uint64_t)m64bit[acquireVariableFromExtensions(returnCompiledString(var), THREADRIPPER).slot]); // Copy raw bytes
                a.mov(x86::qword_ptr(x86::r10, k.slot), x86::rax);
                base = base + 8;
            }
            for (std::pair<std::string, lua_localSymbol> &j: Alloc) {
                //THREADRIPPER->symbols.insert(j);
            }
        }
        uint64_t *p = nullptr;
        uint32_t _offset = s;
        finalAllocMem = 520 + s + (THREADRIPPER->lvl*8);
        // Check alignment of the stack.
        if ((finalAllocMem & 0xF) == 0) {
            finalAllocMem += 8;
        }
        //finalAllocMem = 0xFFFF;
        a.sub(x86::rsp, finalAllocMem);
        
        _0_0_0_CMPTIME_ASM_localStackFrameBytes -= 520;
        _0_0_0_CMPTIME_ASM_localStackFrameBytes -= s;
        a.mov(x86::rcx, (uint64_t)F_MEM_SCR);
        if (THREADRIPPER->lvl >= 1)
            a.mov(x86::qword_ptr(x86::rbp, -528), x86::rdi);
        else
            goto _saveMem;
        if (THREADRIPPER->lvl == 2)
            a.mov(x86::qword_ptr(x86::rbp, -536), x86::rsi);
        _saveMem:
        if (_allocatedMemorySave)
            a.mov(x86::qword_ptr(x86::rbp, -32), x86::r10);
        a.mov(x86::qword_ptr(x86::rbp, -496), x86::rcx); // Save pointer in case it are used.
        _0_0_0_CMPTIME_ASM_isScript = false;
    } else { //All locals from script SHOULD be saved in a map.
        a.sub(x86::rsp, 520); // Starting from byte 128 it should be arguments pass, and the starting from 256 should be return place 
        if (s > 0) {
            fMem = mmap(nullptr, s, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            _0_0_0_CMPTIME_ASM_scriptMem = fMem;
            _0_0_0_CMPTIME_ASM_isScript = true;
            a.movabs(x86::rcx, (uint64_t)fMem);
            a.mov(x86::qword_ptr(x86::rbp, -16), x86::rcx);
        }
    }
    /*if (s > 0) {
        a.xor_(x86::rdi, x86::rdi);
        a.mov(x86::rsi, s);
        a.mov(x86::rdx, 0x03);
        a.mov(x86::r10, 0x22);
        a.mov(x86::r8, -1);
        a.xor_(x86::r9, x86::r9);
        a.mov(x86::rax, 9);
        a.syscall();
        a.mov(x86::qword_ptr(x86::rsp, 24), x86::rax);
        // Now as we got the memory page, propagate it.
        // Mem on rax
        if (THREADRIPPER->toEXbytes > 0) {
            Label _loop = a.new_label();
            Label _exit = a.new_label();
            a.mov(x86::r9, x86::rax);
            a.mov(x86::rdi, x86::qword_ptr(x86::rsp, s));
            uint32_t pos = 0;
            //while (THREADRIPPER->toEXbytes != 0) {
            //rdx = cache obj
            //rcx = counter+1
            //rax = per counter
            //rdi = arguments obj
            //rsi = arguments count
            //r9 = mmap obj
            a.mov(x86::rsi, x86::qword_ptr(x86::rdi));
            a.mov(x86::rcx, x86::rsi);
            a.mov(x86::r8, 0x10);
            a.bind(_loop);
            a.mov(x86::rax, x86::rsi);
            a.mul(x86::r8);
            a.add(x86::rax, 0x08);
            a.mov(x86::rdx, x86::qword_ptr(x86::rdi, x86::rax));
            a.sub(x86::rax, 0x08);
            a.mov(x86::qword_ptr(x86::r9, x86::rax), x86::rdx);
            a.loop(_loop);
            a.bind(_exit);
        }
    }*/
    std::vector<_closure_helper> closures;
    uint_fast16_t for_cnt_;
    ActualScope = THREADRIPPER;
    updateCacheRegisters(&a, ActualScope, symbols);
    while (true) {
        //Do/then and end parts should be blocks that when 'break' keyword used it should jump to end.
        try {
            cache = CODE.at(pos);
        } catch (std::out_of_range &e) {
            // Uh oh!
            goto _END_;
        }
        switch (cache.OPCODE) {
            /*
             * AND                              .
             * NOT                              .
             * OR                               .
             * VTN (Variable address)           .
             * IF                               .
             * ELSE                             .
             * ELSEIF                           .
             * SCOPE                            .
             * SCOPE_END                        .
             * FOR                              .
             * FOR EXPRESSION                   .
             * CALL <arguments>                 .
             * DECLARATION <to store addr>      .
             * FIRST LEXICAL OPCODES            .
             * SYNTAX SEPARATOR                 .
             * FUNCTION                         .
             * RAW LUA TO EVAL                  .
             * TABLE                            .
             * */
            case l_b_o_c_SCP: {
                // Scope start, has variables in it
                ActualScope = cache.SCOPE;
                _LK = cache.OPCODE;
                // Update the variables cache
                updateCacheRegisters(&a, ActualScope, symbols);
                break;
            }
            case l_b_o_c_DEC: {
                // Declaration.
                bool _predef_one = false;
                LuaLexFrame k;
                uint32_t pos = 0;
                //_F_ASM_SEARCHVALUENTHENRETURNRAX(&cache.LLF, &pos, &a, ActualScope, &_stack_mem, &persize, true);
                //_F_ASM_SEARCHVALUE(&cache.LLF, &pos, &a, ActualScope, true);
                std::pair<bool, x86::Gp> chk = areThisVarInHotVars(std::string(cache.LLF.at(0)._data.begin(), cache.LLF.at(0)._data.end()), symbols);
                if (chk.first) {
                    qlog0._log2("prepare Pointer for data alloc: <ONLY REGISTER>\n");
                    x86::Gp reg = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, x86::Gp>(false, x86::noReg), false, true);
                    _ASM__movToReg(chk.second, reg);
                    qlog0._log2("prepare done <ONLY REGISTER>\n");
                    qlog0._log2("\n");
                    _LK = cache.OPCODE;
                    break;
                }
                qlog0._log2("prepare Pointer for data alloc:\n");
                bool _toHiSpeedReg = false;
                x86::Gp uGp = _ASM__getPathToSelGp(&cache.LLF, x86::rbx, ActualScope, true);
                if (uGp.id() > x86::Gp::Id::kIdR11)
                    _toHiSpeedReg = true;
                qlog0._log2("Pointer saved to REGISTER:RBX\n");
                x86::Gp reg = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, x86::Gp>(true, x86::rdi), false, _toHiSpeedReg);
                //qlog0._log2("\r[ret]CLUA_EvalExprNReturn()=x86::noReg : ");
                // RDI used here!
                qlog0._log2("save [ret] to REGISTER:RBX>>\n");
                _ASM_DEBUGGER_STOP();
                a.mov(x86::qword_ptr(x86::rbx), reg);
                if (uGp.id() > x86::Gp::Id::kIdR11) {
                    // Should update hot variables register if available..
                    a.mov(uGp, reg);
                }
                qlog0._log2("[[UPPER DONE]]\n");
                qlog0._log2("\n");
                _LK = cache.OPCODE;
                break;
            }
            case l_b_o_c_STM: {
                // Multiple declaration at once.
                // The multideclaration method ONLY works for local variables.
                // Now, let's eval the data to parse.
                LuaType _UNK;
                //_F_ASM_MultiUse_EvalUntil(&cache.p.at(0), &a, ActualScope, _L_NONE, &_UNK, true, cache.fixedaddr);
                // The data returned in rax should be a boolean and the data in the array... Already saved.
                // The function executed will had in the third argument an array, which points directly into the variables so it should save it.
                // Example: Address starting for local variables [3], slot 256 to 256+(8*3)
                // -> lea [addr of slot 256 (start point)]
                // But let's remember theres a limit.
                // The specified variables to name might be tight to fit with the data returned of the function, so it maybe modify other variables.
                // Anyways, there will be shadow space for those bugs. Well.
                // UNSTABLE!
                break;
            }
            case l_b_o_c_IFS: {
                // Only values with NIL flag or false flag will don't allow the execution of this.
                LuaType _UNK = LuaUnknown;
                //_F_ASM_MultiUse_EvalUntil(&cache.p.at(0), &a, ActualScope, _L_NONE, &_UNK, false, 0);
                qlog0._log2("if() start\n");
                qlog0._log2("if() start::data\n");
                qlog0._log2("if() start::data::size() = ");
                qlog0._log2(std::to_string(cache.p.at(0).size()).c_str());
                qlog0._log2("\n");
                qlog0._log2("if() start::data::data[$]() = ");
                for (LuaLexFrame &i: cache.p.at(0)) {
                    qlog0._log2("$[");
                    qlog0._log2(std::to_string(i.key).c_str());
                    qlog0._log2("] ");
                }
                qlog0._log2("\n");    
                x86::Gp reg = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, x86::Gp>(false, x86::noReg), false, true);
                qlog0._log2("if() start::endData\n");
                // Let's see...
                // on rdi.
                Label _STARTPOINT = a.new_label();
                Label _ENDPOINT = a.new_label();
                a.test(reg, reg);
                a.jz(_ENDPOINT);
                a.bind(_STARTPOINT);
                scopeBlocks.push_back(std::pair<Label, Label>(_STARTPOINT, _ENDPOINT)); // startpoint and endpoint
                closures.push_back(_closure_helper{reg, x86::noReg, x86::noReg, 1, 0, 0, nullptr});
                IF_statements++;
                // Scope start, has variables in it
                ActualScope = cache.SCOPE;
                _LK = cache.OPCODE;
                // Update the variables cache
                updateCacheRegisters(&a, ActualScope, symbols);
                qlog0._log2("if() end\n");
                break;
            }
            case l_b_o_c_FOR: {
                
                // Scope start, has variables in it
                qlog0._log2("start::For__\n", 0xD);
                ActualScope = cache.SCOPE;
                _LK = cache.OPCODE;
                // Update the variables cache
                qlog0._log2("mid::For__<UpdateScopeVariables::START>\n");
                updateCacheRegisters(&a, ActualScope, symbols);
                qlog0._log2("mid::For__<UpdateScopeVariables::END>\n");
                // Calculate which type of expr this has to offer.
                
                uint8_t _type = _getForTypeExpression(&cache.p);
                TString *varname = (TString*)cache.ptr;
                switch (_type) {
                    case 0: {
                        m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("Internal error. Type for 'for' expression is 0"));
                        m_LuaErrorHandler->setFatal(true);
                        return nullptr;
                    }
                    case 1: {
                        // Number count
                        // Let's use our variables.
                        // First, search it.
                        std::pair<x86::Gp, x86::Gp> register_1 = _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(varname, ActualScope, &a, true, false);
                        qlog0._log2("\033[31m");
                        x86::Gp register_ = register_1.first; // Count ptr
                        x86::Gp startPointReg = x86::noReg; // Number which we need to reach.
                        x86::Gp toCmpReg = x86::noReg; // Reached point ptr
                        x86::Gp stepReg = x86::noReg; // How many steps should take every iteration.
                        // Extract values from the expression.
                        int64_t _hyperValue__startpoint = 0;
                        int64_t _hyperValue__comparepoint = 0;
                        int64_t _hyperValue__stepPoint = 0;
                        bool _fI = false;
                        for (std::vector<LuaLexFrame> &q: cache.p) {
                            for (LuaLexFrame &b: q) {
                                if (b.key == _L_PATH) {
                                    // Skip the first iteration, as is.. This is the first variable to iterate, likely: <i> = 1, 2
                                    if (!_fI) {
                                        _fI = !_fI;
                                        continue;
                                    }
                                }
                                    
                                if (b.key == _L_NUMBER) {
                                    //nms.push_back();
                                    int64_t value = std::stoll(std::string(b._data.begin(), b._data.end()));
                                    if (startPointReg == x86::noReg) {
                                        startPointReg = x86::rdi;
                                        _hyperValue__startpoint = value;
                                    } else if (toCmpReg == x86::noReg){
                                        toCmpReg = x86::rdi;
                                        _hyperValue__comparepoint = value;
                                    } else if (stepReg == x86::noReg) {
                                        stepReg = x86::rdi;
                                        _hyperValue__stepPoint = value;
                                    }
                                } else {
                                    if (b.key == _L_EXPRESSION) {
                                        // SHOOOSH.
                                        uint64_t *toSave = nullptr;
                                        // Compare.
                                        x86::Gp reg = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, x86::Gp>(true, x86::rax), false, true); // rax will be ignored if using a high speed local var
                                        if (reg.id() > 11) { // High registers
                                            if (startPointReg == x86::noReg) {
                                                startPointReg = reg;
                                            } else if (toCmpReg == x86::noReg)
                                                toCmpReg = reg;
                                            else { // Set
                                                if (stepReg == x86::noReg) {
                                                    stepReg = reg;
                                                } else {
                                                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("More than expected -> 'for' keyword"));
                                                    m_LuaErrorHandler->setFatal(true);
                                                    return nullptr;
                                                }
                                            }
                                        } else {
                                            toSave = new uint64_t(0);
                                            a.movabs(x86::r8, (uint64_t)toSave);
                                            a.mov(x86::qword_ptr(x86::r8), reg);
                                            //memcpy(&_h)
                                            if (startPointReg == x86::noReg) {
                                                toCmpReg = x86::rax;
                                                memcpy(&_hyperValue__startpoint, &(toSave), 8); // Fast at first glance
                                            } else if (toCmpReg == x86::noReg) {
                                                toCmpReg = x86::rax;
                                                memcpy(&_hyperValue__comparepoint, &(toSave), 8); // Slow
                                            } else if (stepReg == x86::noReg) {
                                                stepReg = x86::rax;
                                                memcpy(&_hyperValue__stepPoint, &(toSave), 8); // Slow
                                            } else {
                                                m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("More than expected -> 'for' keyword"));
                                                m_LuaErrorHandler->setFatal(true);
                                                return nullptr;
                                            }
                                        }
                                    } else {
                                        if (b.key == _L_PATH) {
                                            //std::pair<x86::Gp, x86::Gp> regs = _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(varname, ActualScope, &a, true, false);
                                            // Maybe it is not the correct time to guess where it is, but their root.
                                            lua_localSymbol var = acquireVariableFromExtensions(b.addr->getHeaderVarString(), ActualScope);
                                            x86::Gp reg0;
                                            if (var.cacheReg > 0) {
                                                reg0 = id_to_reg(var.cacheReg-1);
                                                //std::cout << std::to_string(var.cacheReg) << "; " << b.addr->getHeaderVarString() << std::endl;
                                            } else {
                                                reg0 = x86::rax; // A way to assimilate.
                                            }
                                            // if (reg0.id() > 11) {
                                                if (startPointReg == x86::noReg)
                                                    startPointReg = reg0;
                                                else if (toCmpReg == x86::noReg)
                                                    toCmpReg = reg0;
                                                else if (stepReg == x86::noReg)
                                                    stepReg = reg0;
                                                else {
                                                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("More than expected -> 'for' keyword"));
                                                    m_LuaErrorHandler->setFatal(true);
                                                    return nullptr;
                                                }
                                            // }
                                        }
                                    }
                                }
                            }
                        }
                        //counters.push_back(std::pair<std::vector<int64_t>, x86::Gp>(nms, register_));
                        // Transform nms.at(2) to be a compatible value.
                        closures.push_back(_closure_helper{register_, toCmpReg, stepReg, 2, _hyperValue__comparepoint, _hyperValue__stepPoint, varname});
                        if (register_ != x86::rdi) { // If not rdi then it is a high speed variable
                            if (_hyperValue__startpoint != 0 && startPointReg == x86::rdi) {
                                a.mov(register_, (uint64_t)_hyperValue__startpoint & 0x0000FFFFFFFFFFFFULL);
                            } else {
                                if (startPointReg != x86::noReg) {
                                    if (startPointReg != x86::rdi) {
                                        if (startPointReg.id() > 11) {
                                            a.mov(x86::r11, 0x0000FFFFFFFFFFFFULL);
                                            a.and_(startPointReg, x86::r11);
                                        } else {
                                            // Stored in memory.
                                            a.mov(x86::rdi, static_cast<uint64_t>(_hyperValue__startpoint));
                                            a.mov(startPointReg, x86::qword_ptr(x86::rdi));
                                        }
                                        a.mov(register_, startPointReg);
                                    } else {
                                        a.mov(register_, (uint64_t)_hyperValue__startpoint);
                                    }
                                } else {
                                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("Required special keyword"));
                                    m_LuaErrorHandler->setFatal(true);
                                    return nullptr;
                                }
                                uint8_t idx = rIdTo_symbol(register_.id());
                                symbols->at(idx)->type = LuaInteger;
                            }  
                        } else {
                            if (startPointReg != x86::noReg) {
                                if (startPointReg != x86::rdi) {
                                    if (startPointReg.id() > 11) {
                                        a.mov(x86::r11, 0x0000FFFFFFFFFFFFULL);
                                        a.and_(startPointReg, x86::r11);
                                    } else {
                                        // Stored in memory.
                                        a.mov(x86::rdi, static_cast<uint64_t>(_hyperValue__startpoint));
                                        a.mov(startPointReg, x86::qword_ptr(x86::rdi));
                                    }
                                    a.mov(x86::rdx, startPointReg);
                                    a.mov(x86::qword_ptr(x86::rdi), x86::rdx);
                                } else if (startPointReg == x86::rdi) {
                                    a.mov(x86::rdx, (uint64_t)_hyperValue__startpoint);
                                    a.mov(x86::qword_ptr(x86::rdi), x86::rdx);
                                }
                            } else {
                                m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("Required special keyword"));
                                m_LuaErrorHandler->setFatal(true);
                                return nullptr;
                            }
                            
                        }
                        // Ez way to get numbers.
                        // Create labels.
                        Label _STARTPOINT = a.new_label();
                        Label _ENDPOINT = a.new_label();
                        a.bind(_STARTPOINT);
                        scopeBlocks.push_back(std::pair<Label, Label>(_STARTPOINT, _ENDPOINT));
                        // When reaching _ENDPOINT, must verify if num == final.
                        qlog0._log2("mid::For__<MainStartScope>\n");
                        qlog0._log2("\033[0m");
                        break;
                    }
                }
                qlog0._log2("mid::For__<MainStartScope>\n");
                break;
            }
            case l_b_o_c_ELI: {
                
                break;
            }
            case l_b_o_c_ELS: {
                // Get the label on the latest part
                Label _STARTPOINT = a.new_label();
                a.jmp(_STARTPOINT);
                a.bind(scopeBlocks.back().second);
                scopeBlocks.pop_back();
                Label _ENDPOINT = a.new_label();
                scopeBlocks.push_back(std::pair<Label, Label>(_STARTPOINT, _ENDPOINT));
                break;
            }
            case l_b_o_c_SCE: {
                // Let's see if FOR parent are there
                qlog0._log2("\033[31m");
                if (closures.back().closureType == 2) {
                    for_cnt_--;
                    qlog0._log2("end::For__<ReachedPoint>\n");
                    int64_t goal_ = closures.back().toCmp_;
                    // Counter is at some state..
                    // Uhm, let's check registers.
                    x86::Gp _uReg = closures.back()._uReg;
                    if (_uReg != x86::noReg) {
                        // We can do direct arithmetic.
                        x86::Gp gpToUse = x86::r8;
                        if (closures.back()._toCmp == x86::rax) { // Pointer
                            a.mov(x86::r8, static_cast<uint64_t>(closures.back().toCmp_));
                            a.mov(x86::r8, x86::qword_ptr(x86::r8));
                        } else if (closures.back()._toCmp == x86::rdi) {
                            a.mov(x86::r8, (uint64_t)goal_);
                        } else if (closures.back()._toCmp.id() > 11) {
                            //a.mov(x86::r8, closures.back()._toCmp);
                            gpToUse = closures.back()._toCmp;
                        }
                        a.cmp(_uReg, gpToUse);
                        a.jge(scopeBlocks.back().second);
                        if (closures.back()._stepReg == x86::rax) { // Pointer
                            a.mov(x86::r8, static_cast<uint64_t>(closures.back().step_));
                            a.mov(x86::r8, x86::qword_ptr(x86::r8));
                            a.add(_uReg, x86::r8);
                        } else if (closures.back()._stepReg == x86::rdi) {
                            a.mov(x86::r8, (uint64_t)closures.back().step_);
                            a.add(_uReg, x86::r8);
                        } else if (closures.back()._stepReg.id() > 11 && closures.back()._stepReg != x86::noReg) {
                            a.add(_uReg, closures.back()._stepReg);
                        } else {
                            a.inc(_uReg);
                        }
                        //a.ud2();
                        a.jmp(scopeBlocks.back().first);
                    } else {
                        std::pair<x86::Gp, x86::Gp> register_ = _F_ASM_PUTVARIABLEONTOFUNCTION_RAX(closures.back()._vName, ActualScope, &a, false, false, true);
                        if (register_.first != x86::rdi) {
                            // We can do direct arithmetic.
                            x86::Gp gpToUse = x86::r8;
                            if (closures.back()._toCmp == x86::rax) { // Pointer
                                a.mov(x86::r8, static_cast<uint64_t>(closures.back().toCmp_));
                                a.mov(x86::r8, x86::qword_ptr(x86::r8));
                            } else if (closures.back()._toCmp == x86::rdi) {
                                a.mov(x86::r8, (uint64_t)goal_);
                            } else if (closures.back()._toCmp.id() > 11) {
                                //a.mov(x86::r8, closures.back()._toCmp);
                                gpToUse = closures.back()._toCmp;
                            }
                            a.cmp(x86::qword_ptr(register_.first), gpToUse);
                            a.jge(scopeBlocks.back().second);
                            
                            if (closures.back()._stepReg == x86::rax) { // Pointer
                                a.mov(x86::r8, static_cast<uint64_t>(closures.back().step_));
                                a.mov(x86::r8, x86::qword_ptr(x86::r8));
                                a.add(x86::qword_ptr(register_.first), x86::r8);
                            } else if (closures.back()._stepReg == x86::rdi) {
                                a.mov(x86::r8, (uint64_t)closures.back().step_);
                                a.add(x86::qword_ptr(register_.first), x86::r8);
                            } else if (closures.back()._stepReg.id() > 11 && closures.back()._stepReg != x86::noReg) {
                                a.add(x86::qword_ptr(register_.first), closures.back()._stepReg);
                            } else {
                                a.inc(x86::qword_ptr(register_.first));
                            }
                            //a.inc(x86::qword_ptr(register_.first));
                            a.jmp(scopeBlocks.back().first);
                        } else {
                            // We can do direct arithmetic.
                            a.inc(x86::qword_ptr(register_.second));
                            a.cmp(x86::rdi, x86::r8); // the same thing..
                            a.jae(scopeBlocks.back().second);
                            a.jmp(scopeBlocks.back().first);
                        }
                    }
                    closures.pop_back();
                    qlog0._log2("end::For__<END>\n");
                } else {
                    closures.pop_back();
                }
                // At the most top must close.
                a.bind(scopeBlocks.back().second);
                scopeBlocks.pop_back();
                // Exit scope.
                ActualScope = ActualScope->rSCOPE;
                updateCacheRegisters(&a, ActualScope, symbols);
                qlog0._log2("\033[0m");
                break;
            }
            case l_b_o_c_STO: {
                //a.mov(x86::rdi, (uint64_t)cache.V);
                //getValueRequestedNput_to(x86::rbx, &cache.p.at(0));
                // GET THE VALUE.
                // always the first value, the multiple values one are STM
                std::string aB = "Search var to use: ";
                //aB.append(std::string(((TString*)cache.p.at(0).at(0).a)->data),((TString*)cache.p.at(0).at(0).a)->len);
                //aB.append("\n");
                qlog0._log2(aB.c_str());
                //qlog0._log2(cache.p.at(0).at(0).addr->getHeaderVarString().c_str());
                qlog0._log2("\n");
                x86::Gp reg = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, x86::Gp>(true, x86::rcx), false);
                //_F_ASM_MultiUse_EvalUntil(&cache.p.at(0), &a, ActualScope, _L_NONE, &_);
                int32_t offset = 520;
                offset += cache.toMemOffset;
                qlog0._log2("Save to PTR\n");
                if (!Script)
                    a.mov(x86::qword_ptr(x86::rbp, (offset*-1)), reg);
                else {
                    if (reg == x86::rsi) {
                        a.mov(x86::r9, (uint64_t)fMem);
                        a.mov(x86::qword_ptr(x86::r9, cache.toMemOffset), reg);
                        goto _LKUPDT;
                    }
                    a.mov(x86::rsi, (uint64_t)fMem);
                    a.mov(x86::qword_ptr(x86::rsi, cache.toMemOffset), reg);
                }
                qlog0._log2("END save to PTR 'var'\n");
                qlog0._log2("\n");
                _LKUPDT:
                _LK = cache.OPCODE;
                break;
            }
            case l_b_o_c_FUN: {
                qlog0._log2("FunctionGeneration:Start\n");
                a.mov(x86::rdi, (uint64_t)cache.FuncPTR2);
                a.mov(x86::rsi, (uint64_t)cache.SCOPE);
                a.mov(x86::rdx, (uint64_t)true);
                //a.movabs(x86::rcx, (uint64_t));
                //auto func_ptr = static_cast<FunctionPointer(*)(std::vector<lua_biOpCode>*, lua_Scope*, bool)>(&luaBundleFunction); 
                a.call((uint64_t)luaBundleFunction);
                qlog0._log2("FunctionGeneration:End\n");
                qlog0._log2("FunctionGeneration:SaveStart\n");
                a.mov(x86::rbx, x86::rax);
                x86::Gp uGp = _ASM__getPathToSelGp(cache.path->getData(), x86::rdx, ActualScope, true);
                a.mov(x86::qword_ptr(uGp), x86::rbx);
                qlog0._log2("FunctionGeneration:SaveEnd\n\n");
                // Save function
                if (cache._F_LOCAL) {
                    
                } else {
                    if (_LK == l_b_o_c_DEC) {
                        //We got the func, now store it.
                    } else {
                        // If it has a declaration...
                    }
                }
                _LK = cache.OPCODE;
                break;
            }
            case l_b_o_c_CFN: {
                if (cache.ATR == 0) {
                    // Let's check if has a fixed address
                    if (cache.fixedaddr != 0) {
                        // Yay!
                        // Move arguments.
                        qlog0._log2("CallFunc( fixedaddr )::Start\n");
                        qlog0._log2("CallFunc( fixedaddr )::_args[START]\n");
                        qlog0._log2("CallFunc( fixedaddr )::_args[CONTENTS]: ");
                        qlog0._log2(std::to_string(cache.p.at(0).size()).c_str());
                        qlog0._log2("\n");
                        qlog0._log2(dumpinf1(&cache.p.at(0)).c_str());
                        qlog0._log2("\n");
                        _F_ASM_MAKEFUNCTIONARGUMENTS(&cache.p, &a, ActualScope, false, 0); // Should contain rdi, rsi and rdx
                        qlog0._log2("CallFunc( fixedaddr )::_args[END]\n");
                        // Now the func address
                        //a.mov(x86::r9, (uint64_t)0x0000FFFFFFFFFFFFULL);
                        //a.and_(x86::r8, x86::r9);
                        // No need to jump 8bytes.
                        //qlog0._log2("CallFunc( fixedaddr )::_addr\n");
                        qlog0._log2("CallFunc( fixedaddr )::Call: ");
                        qlog0._log2(std::to_string((uintptr_t)cache.fixedaddr).c_str());
                        qlog0._log2("\n");
                        a.call(lua_getPtr(*(Values*)cache.fixedaddr));
                        
                        qlog0._log2("CallFunc( fixedaddr )::End\n");
                    } else {
                        // Let's find their address.
                        qlog0._log2("CallFunc( common )::Start\n");
                        uint32_t pos = 0;
                        //_F_ASM_SEARCHVALUE(cache.path->getData(), &pos, &a, ActualScope, false);
                        x86::Gp reg = CLUA_EvalExprNReturn(&cache.LLF, ActualScope, std::pair<bool, x86::Gp>(true, x86::rbx), false);
                        //a.mov(x86::rbx, reg);
                        //a.ud2(); 
                        _F_ASM_MAKEFUNCTIONARGUMENTS(&cache.p, &a, ActualScope, false, 0);
                        a.movabs(x86::rax, (uint64_t)0x0000FFFFFFFFFFFFULL);
                        a.and_(x86::rbx, x86::rax);
                        //a.shl(x86::r10, 16);
                        //a.sar(x86::r10, 16); 
                        //a.ud2();
                        a.call(x86::rbx);
                        qlog0._log2("CallFunc( common )::End\n");
                    }
                }
                _LK = cache.OPCODE;
                break;
            }
            case l_b_o_c_VTN: { // Maybe its like this: local var0 = hallo["there"];
                if (_LK == l_b_o_c_DEC) {
                } else {
                    //No other usage.. Crash.
                }
            }
            default: {
                // Huh?
            }
        }
        pos++;
    }
    //Free those pointers.
    _END_:
    a.xor_(x86::rax, x86::rax);
    a.add(x86::rsp, finalAllocMem);
    frontNlowerPushes(&a, _CODE, false);
    a.mov(x86::rsp, x86::rbp);
    a.pop(x86::rbp);
    a.ret();
    
    //Emit
    //a.emit_epilog(frame);
    
    a.finalize();
    
    std::cout << "\033[1;33mAssembly Code:\033[0m \n" << qlog0.data() << "" << std::endl;
    void *toalloc = nullptr;
    Error ERR = rt.add(&toalloc, &code);
    if (ERR != Error::kOk) {
        return nullptr;
    }
    //K(new FuncArgs());
    return toalloc;
}












































