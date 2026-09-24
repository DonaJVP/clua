#include "lua.hpp"
#include "cllex.hpp"
#include <asmjit/core/builder.h>
#include <asmjit/x86/x86builder.h>
#include <asmjit/x86/x86globals.h>
#include <asmjit/x86/x86operand.h>
#include <cstddef>
#include <luaconf.h>
#include <random>
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
#include "ltable.hpp"
#include "cljit.hpp"
#include "clregalloc.hpp"
#include "clobject.hpp"
#include "clcompiledcodec.hpp"

// This runs on duct tapes, if you remove one this wont work..

using namespace asmjit;
using namespace asmjit::x86;

asmjit::x86::Gp RR = x86::noReg;
CL_RegisterAllocator *R = nullptr;
asmjit::x86::Builder *a = nullptr;

JitRuntime rt;

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

std::string convertCounterIdToUniqueRegIdString(uint64_t _z) {
    switch (_z) {
        case 0: return "rdi";
        case 1: return "rsi";
        case 2: return "rdx";
    }
    return "NULL0";
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

std::vector<uint64_t> localVariablesCountST = {};

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
    LuaLexFrame _CODENAME(_L_NONE);
    uint64_t localVarsCount = 0;
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
                        localVarsCount++;
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
            case _L_RETURN: {
                lua_biOpCode c;
                c.OPCODE = l_b_o_c_RET;
                if (_FRM.ATTRIB) { // Empty.
                    c.ATR = 0;
                } else {
                    c.ATR = 1;
                    c.LLF = _FRM.EXPR_BRKT;
                }
                opcodes.push_back(c);
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
                            } else {
                                sym.qID = 9;
                                sym.slot = _counter2*8;
                                _counter2++;
                            }
                            sym.id = _LABEL->addr->getHeaderVarString();
                            if (_current < 2) {
                                sym.availReg = "QRT0__ARGUMENTS__"+convertCounterIdToUniqueRegIdString(_current);
                                sym.rawdata = 1;
                            }
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
            case _L_OBJECTCODENAME: {
                _CODENAME = _FRM;
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
                    if (_CODENAME.key != _L_NONE) {
                        std::string *str = new std::string();
                        str->append(std::string(_CODENAME._data.begin(), _CODENAME._data.end()));
                        c.ptr = str; // Assign a new.
                        _CODENAME = LuaLexFrame(_L_NONE);
                    }
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
                        localVarsCount++;
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

std::string sGenStringLiteralRandom(const std::string P) {
    std::string _0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    uint32_t K = dist(gen);
    ///
    _0.append(P+std::to_string(K));
    return _0;
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
                return new lua_localSymbol{0, 3, 0, "", stringID};
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

lua_localSymbol *acquireVariableFromExtensionsPtr(TString *stringID, lua_Scope *T) {
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
            return &actual->symbols.find(std::string(stringID->data, stringID->len))->second;
        } else {
            if (actual->rSCOPE != nullptr) {
                actual = actual->rSCOPE;
            } else {
                // Limit reached
                return nullptr;
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

void _F_ASM_CRASH(const lua_ErrSignals ERR, TString *str) {
    m_LuaErrorHandler->reportError(ERR, 0, std::string(str->data, str->len));
    m_LuaErrorHandler->setFatal(true);
}

uint64_t __LEX_KEY_TO_LuaType(_Lua_Lex_Keys a, uint8_t ATTR) {
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

x86::Gp _CLHASM__getGPbyNumber(uint64_t i) {
    switch (i) {
        case 0: {return x86::rdi;}
        case 1: {return x86::rsi;}
        case 2: {return x86::rdx;}
    }
    return x86::noReg;
}

_CLHASM__funcArgs *_CLHASM__buildArgs(std::vector<std::vector<LuaLexFrame>> &pCDATA, lua_Scope *scope) {
    _CLHASM__funcArgs *res = new _CLHASM__funcArgs;
    uint64_t counter = 0; // 0 == rdi, 1 == rsi, 2 == rdx, ... = stack.
    bool useStask = 0;
    int32_t startingOffset = 0;
    GeneralRegister **args = reinterpret_cast<GeneralRegister**>(res);
    if (pCDATA.size() == 0) {
        return nullptr; // No arguments, just run that function
    } else {
        for (std::vector<LuaLexFrame> &i: pCDATA) {
            if (i.at(0).key == _L_OVERALLTYPECHECKER) { // Ignore space.
                continue;
            }
            if (useStask) {
                // Using stack as mainframe... SUpposedd.
                std::string _tmpRegName = sGenStringLiteralRandom("TMP0");
                GeneralRegister *tmpReg = R->createGR(_tmpRegName);
                ///
                std::tuple<bool, x86::Gp, const std::string> k = CLUA_EvalExprNReturn(&i, scope, std::pair<bool, const std::string>(true, _tmpRegName), false);
                auto [way, registerRaw, registerName] = k;
                if (way) {
                    // Using registerName
                    int32_t memOffset = R->getNextQwordNupdate();
                    if (!startingOffset)
                        startingOffset = memOffset;
                    a->mov(x86::qword_ptr(x86::rbp, memOffset), S(registerName));
                } else {
                    int32_t memOffset = R->getNextQwordNupdate();
                    if (!startingOffset)
                        startingOffset = memOffset;
                    a->mov(x86::qword_ptr(x86::rbp, memOffset), registerRaw);
                }
                R->destroyGR(_tmpRegName);
            } else {
                // Push those registers.
                x86::Gp reg_0_ = _CLHASM__getGPbyNumber(counter);
                if (reg_0_ == x86::noReg) {
                    // Use stack
                    useStask = !useStask;
                    continue;
                } else {
                    std::string _regName = "_ARGUMENTSOPTION@"+std::to_string(counter);
                    GeneralRegister *reg = R->createGR(_regName, false, reg_0_);
                    CLUA_EvalExprNReturn(&i, scope, std::pair<bool, const std::string>(true, _regName), false);
                    // Save
                    args[counter] = reg;
                }
            }
            counter++;
        }
    }
    if (useStask) {
        // Pass it like an object.
        a->lea(x86::rdx, x86::qword_ptr(x86::rbp, startingOffset));
    }
    res->PADDING = counter;
    return res;
}

static void _CLHASM__callFuncOBJ(lua_biOpCode &data, lua_Scope *scope) {
    if (data.ptr) {
        std::string objName = *((std::string*)data.ptr);
        if (ObjectFuncIds.find(objName) == ObjectFuncIds.end()) {
            m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, ("Invalid object name: "+objName).c_str());
            m_LuaErrorHandler->reportWarning(_lua_es_UnknownDataIdx, 0, "Skipping object execution.");
        } else {
            std::unordered_map<std::string, uint64_t> *_FUNCS = &ObjectFuncIds.at(objName);
            if (data.LLF.at(0).key == _L_PATH) {
                if (data.LLF.at(0).addr->needToResolveAddr()) {
                    m_LuaErrorHandler->reportWarning(_lua_es_UnknownDataIdx, 0, "Needed to resolve address, but object execution denied.");
                    goto _END;
                } else {
                    uint64_t fAddr = 0;
                    LuaLexFrame *FRB = data.LLF.at(0).addr->getBack();
                    std::string fName = std::string(FRB->_data.begin(), FRB->_data.end());
                    try {
                        fAddr = _FUNCS->at(fName);
                    } catch (std::out_of_range &e) {
                        m_LuaErrorHandler->reportWarning(_lua_es_NonFunction, 0, "Object's required function doesn't exist!");
                        goto _END;
                    }
                    a->mov(x86::rbx, fAddr);
                    ///
                    LuaLexFrame _SELF(_L_PATH); 
                    lua_AddrPath *p = new lua_AddrPath();
                    p->assignNewAddr(std::vector<LuaLexFrame>{*FRB});
                    p->getBack()->_LK = true;
                    _SELF.addr = p;
                    _SELF.ATTRIB = 0;
                    lua_Expression E = _CPP__insertToFirstPosition(std::vector<LuaLexFrame>{_SELF}, &data.p);
                    ///
                    _CLHASM__funcArgs *ARGS = _CLHASM__buildArgs(E, scope);
                    R->emitCall();
                    a->call(fAddr);
                    // Clean up those registers.
                    uint16_t addernum = 0;
                    while (ARGS->PADDING) {
                        if (ARGS->PADDING > 2) {
                            ARGS->PADDING = 3;
                        }
                        std::string _regName = "_ARGUMENTSOPTION@"+std::to_string(addernum);
                        R->destroyGR(_regName);
                        ARGS->PADDING--;
                        addernum++;
                    }
                }
            } 
        }
    }
    _END:
}

static void _CLHASM__call(lua_biOpCode &data, lua_Scope *scope) {
    // First search address for this.
    if (!data.ATR) { // Non expression type.
        if (data.fixedaddr) {
            _CLHASM__funcArgs *ARGS = _CLHASM__buildArgs(data.p, scope);
            R->emitCall();
            a->call(lua_getPtr(*(Values*)(data.fixedaddr)));
            uint16_t addernum = 0;
            while (ARGS->PADDING) {
                if (ARGS->PADDING > 2) {
                    ARGS->PADDING = 3;
                }
                std::string _regName = "_ARGUMENTSOPTION@"+std::to_string(addernum);
                R->destroyGR(_regName);
                ARGS->PADDING--;
                addernum++;
            }
        } else {
            // Common way
            if (data.ptr) {
                // Object callings on the other side!
                _CLHASM__callFuncOBJ(data, scope);
            } else {
                auto [way, regRaw, regName] = CLUA_EvalExprNReturn(&data.LLF, scope, std::pair<bool, std::string>(false, ""), false);
                a->mov(x86::rbx, way ? S(regName) : regRaw);
                if (way) {
                    a->mov(x86::r11, PTR_MASK);
                    a->and_(x86::rbx, x86::r11);
                }
                cont0:
                _CLHASM__funcArgs *ARGS = _CLHASM__buildArgs(data.p, scope);
                R->emitCall();
                a->call(x86::rbx);
                // Clean up used registers.
                uint16_t addernum = 0;
                while (ARGS->PADDING) {
                    if (ARGS->PADDING > 2) {
                        ARGS->PADDING = 3;
                    }
                    std::string _regName = "_ARGUMENTSOPTION@"+std::to_string(addernum);
                    R->destroyGR(_regName);
                    ARGS->PADDING--;
                    addernum++;
                }
            }
        }
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

void frontNlowerPushes(x86::Builder *a, std::vector<lua_biOpCode> *quote, bool way) {
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
void updateCacheRegisters(x86::Builder *a, lua_Scope *Scope, std::unordered_map<uint8_t, lua_localSymbol*> *Symbols) {
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
    for (uint8_t i = 0; i < 4; i++) {
        lua_localSymbol *sym = Symbols->at(i);
        if (sym) {
            _optRegisterAlloc[i] = true;
        } else
            _optRegisterAlloc[i] = false; // Do not take or save a register which has unknown data
    }
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
                _optRegisterAlloc[_register] = true;
            }
            _register++;
        }
        qlog0._log2("Occupying for: ");
        qlog0._log2(name.c_str());
        qlog0._log2("\n");
        if (canOccupyReg) {
            freeRegisters.push_back(_register_0);
        }
        _justSum:
        _register_0++;
    }
    // Use freed registers.
    uint8_t _c = 0;
    //GeneralRegister *mask = R->createGR("maskForPtrs");
    //a->mov(S("maskForPtrs"), (uint64_t)PTR_MASK);
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
                GeneralRegister *r0 = R->createGR("r0"+sym->id);
                _ASM__searchSymbolToUse("r0"+sym->id, returnCompiledString(sym->id), Scope, 2);
                a->mov(x86::qword_ptr(SX("r0"+sym->id)), _ASMH__parseVarCacheRef(sym->cacheReg));
                R->destroyGR("r0"+sym->id);
                qlog0._log2(">>> END SAVE ");
                qlog0._log2(sym->id.c_str());
                qlog0._log2(" :: ");
                qlog0._log2(std::to_string(sym->cacheReg).c_str());
                qlog0._log2("\n");
                //sym->cacheReg = 0;
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
            qlog0._log2(">>> Loading ");
            qlog0._log2(slot->id.c_str());
            qlog0._log2("\n");
            GeneralRegister *reg = R->createGR("load"+slot->id);
            _ASM__searchSymbolToUse("load"+slot->id, returnCompiledString(slot->id), Scope, false);
            a->mov(x86::r11, PTR_MASK);
            a->and_(S("load"+slot->id), x86::r11);
            a->mov(_ASMH__parseVarCacheRef(regId+1), SX("load"+slot->id));
            R->destroyGR("load"+slot->id, true);
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
            GeneralRegister *r0 = R->createGR("r0"+sym->id);
            _ASM__searchSymbolToUse("r0"+sym->id, returnCompiledString(sym->id), Scope, 2);
            a->mov(x86::qword_ptr(SX("r0"+sym->id)), _ASMH__parseVarCacheRef(sym->cacheReg));
            R->destroyGR("r0"+sym->id, true);
            qlog0._log2(">>> END SAVE ");
            qlog0._log2(sym->id.c_str());
            qlog0._log2(" :: ");
            qlog0._log2(std::to_string(sym->cacheReg).c_str());
            qlog0._log2("\n");
            //sym->cacheReg = 0; // But theyre never 0.
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
    for (lua_biOpCode &k: *c) {
        std::cout << "$(" << std::to_string(k.OPCODE);
        if (k.OPCODE == l_b_o_c_LXC) {
            std::cout << ";LXC=$[" << std::to_string(k.KEY) << "]) ";
        } else if (k.OPCODE == l_b_o_c_UPV) {
            std::cout << ";SlotsAsRegisters=" << (k.ATR ? "true" : "false") << ") ";
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
    std::string mainReg;
    std::string cmpReg;
    std::string stepReg;
    uint8_t closureType = 0;
    int64_t toCmp_ = 0;
    int64_t step_ = 0;
    TString *_vName;
    int64_t mainReg_HSV = 0;
};

static std::string dumpinf1(std::vector<LuaLexFrame> *c) {
    std::string q0 = "";
    std::cout << "EvaluateExpression: ";
    for (LuaLexFrame &k: *c) {
        q0.append("$[");
        q0.append(std::to_string(k.key));
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
#include "clobject.hpp"
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
    x86::Builder a_(&code);
    a = &a_;
    code.set_logger(&qlog0);
    initializeRegistersData((void*)a);
    _lua_Table__initializeAssembler(a);
    CL_RegisterAllocator *CLRA = new CL_RegisterAllocator((void*)a);
    R = CLRA;
    // Initialize registers allocator
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
    a->emit_prolog(frame);*/
    
    
    // magic
    lua_biOpCode cache;
    lua_1_biOpCode _LK = l_b_o_c_NUL;
    uint32_t pos = 0;
    Label k1;
    Label k2;
    Label k3;
    Label k4;
    //FuncSignature sig = FuncSignature::build<FuncArgs*, FuncArgs*>(CallConv::kIdHost);
    //a->emit_prolog(sig);
    Label _ENDPOINT_ = a->new_label();
    Label _ENDPOINT_NONXORRAX_ = a->new_label();
    a->push(x86::rbp);
    a->mov(x86::rbp, x86::rsp);
    frontNlowerPushes(a, _CODE, true);
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
    GeneralRegister *fM_ = nullptr;
    if (!Script) {
        //May sum some other bytes for those variables that are outside this @nested function
        bool _allocatedMemorySave = false;
        GeneralRegister *tmp0;
        if (!_CODE->at(_CODE->size()-2).nestedtoUpValues.empty()) {
            fM_ = CLRA->createGR("MemoryScript0");
            tmp0 = CLRA->createGR("temporal0_regFunc0");
            _allocatedMemorySave = true;
            // Alloc
            size_t tAlloc = _CODE->at(_CODE->size()-2).nestedtoUpValues.size();
            tAlloc--;
            fMem = mmap(nullptr, tAlloc*8, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            // Sum to the offset of our main variables.
            uint32_t base = 0;
            std::vector<std::pair<std::string, lua_localSymbol>> Alloc;
            a->movabs(x86::r10, (uint64_t)fMem);
            for (std::string &var: _CODE->at(_CODE->size()-2).nestedtoUpValues) {
                //Time to recover those vars
                lua_localSymbol k;
                k.qID = 4;
                k.slot = base;
                Alloc.push_back(std::pair<std::string, lua_localSymbol>(var, k)); //Register the local
                a->mov(tmp0->GR_ID, (uint64_t)m64bit[acquireVariableFromExtensions(returnCompiledString(var), THREADRIPPER).slot]); // Copy raw bytes
                a->mov(x86::qword_ptr(fM_->GR_ID, k.slot), tmp0->GR_ID);
                base = base + 8;
            }
            for (std::pair<std::string, lua_localSymbol> &j: Alloc) {
                //THREADRIPPER->symbols.insert(j);
            }
            // Destroy
            CLRA->destroyGR("temporal0_regFunc0");
        }
        uint64_t *p = nullptr;
        uint32_t _offset = s;
        R->localsAllocated(s);
        //finalAllocMem = 520 + s + (THREADRIPPER->lvl*8);
        // Check alignment of the stack.
        
        
        GeneralRegister *tmp1 = CLRA->createGR("f_mem_scr");
        
        //finalAllocMem = 0xFFFF;
        a->sub(x86::rsp, 0xFFF);
        _0_0_0_CMPTIME_ASM_localStackFrameBytes -= 520;
        _0_0_0_CMPTIME_ASM_localStackFrameBytes -= s;
        a->mov(tmp1->GR_ID, (uint64_t)F_MEM_SCR);
        _saveMem:
        if (_allocatedMemorySave) {
            a->mov(x86::qword_ptr(x86::rbp, -32), fM_->GR_ID);
            CLRA->destroyGR("temporal0_regFunc0");
        }
        a->mov(x86::qword_ptr(x86::rbp, -496), tmp1->GR_ID); // Save pointer in case it are used.
        _0_0_0_CMPTIME_ASM_isScript = false;
        CLRA->destroyGR("f_mem_scr");
        CLRA->destroyGR("MemoryScript0");
    } else { //All locals from script SHOULD be saved in a map.
        a->sub(x86::rsp, 0); 
        if (s > 0) {
            fMem = mmap(nullptr, s, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            _0_0_0_CMPTIME_ASM_scriptMem = fMem;
            _0_0_0_CMPTIME_ASM_isScript = true;
            GeneralRegister *tmp0 = CLRA->createGR("f_mem_scr");
            a->mov(S("f_mem_scr"), (uint64_t)fMem);
        }
    }
    std::vector<_closure_helper> closures;
    uint_fast16_t for_cnt_;
    ActualScope = THREADRIPPER;
    updateCacheRegisters(a, ActualScope, symbols);
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
                updateCacheRegisters(a, ActualScope, symbols);
                break;
            }
            case l_b_o_c_DEC: {
                // Declaration.
                bool _predef_one = false;
                LuaLexFrame k;
                uint32_t pos = 0;
                std::pair<bool, x86::Gp> chk = areThisVarInHotVars(std::string(cache.LLF.at(0)._data.begin(), cache.LLF.at(0)._data.end()), symbols);
                if (chk.first) { // High speed variable
                    qlog0._log2("prepare Pointer for data alloc: <ONLY REGISTER>\n");
                    std::tuple<bool, x86::Gp, const std::string> k = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, const std::string>(false, ""), false, true);
                    auto [way, regRaw, regName] = k;
                    _ASM__movToReg(chk.second, way ? S(regName) : regRaw);
                    R->destroyGR(regName);
                    qlog0._log2("prepare done <ONLY REGISTER>\n");
                    qlog0._log2("\n");
                    _LK = cache.OPCODE;
                    break;
                }
                qlog0._log2("prepare Pointer for data alloc:\n");
                bool _toHiSpeedReg = false;
                GeneralRegister *r0 = R->createGR("h0lbocD1");
                //r0->doNotSave = true;
                x86::Gp uGp = _ASM__getPathToSelGp(&cache.LLF, "h0lbocD1", ActualScope, true);
                if (uGp.id() > x86::Gp::Id::kIdR11) {
                    _toHiSpeedReg = true;
                } else
                    a->mov(x86::rbx, S("h0lbocD1"));
                qlog0._log2("Pointer saved to REGISTER:RBX\n");
                auto [way, regRaw, regName] = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, const std::string>(false, ""), false, _toHiSpeedReg);
                qlog0._log2("save [ret] to REGISTER:RBX>>\n");
                a->mov(x86::qword_ptr(x86::rbx), way ? S(regName) : regRaw);
                if (_toHiSpeedReg) {
                    // Should update hot variables register if available..
                    a->mov(x86::r11, PTR_MASK);
                    a->mov(uGp, way ? S(regName) : regRaw);
                    a->and_(uGp, x86::r11);
                }
                R->destroyGR("h0lbocD1");
                R->destroyGR(regName);
                qlog0._log2("[[UPPER DONE]]\n");
                qlog0._log2("\n");
                _LK = cache.OPCODE;
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
                auto [way, regRaw, regName] = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, const std::string>(false, ""), false, true);
                qlog0._log2("if() start::endData\n");
                // Let's see...
                // on rdi.
                Label _STARTPOINT = a->new_label();
                Label _ENDPOINT = a->new_label();
                a->test(way ? S(regName) : regRaw, way ? S(regName) : regRaw);
                a->jz(_ENDPOINT);
                a->bind(_STARTPOINT);
                scopeBlocks.push_back(std::pair<Label, Label>(_STARTPOINT, _ENDPOINT)); // startpoint and endpoint
                closures.push_back(_closure_helper{"", "", "", 1, 0, 0, nullptr});
                IF_statements++;
                // Scope start, has variables in it
                ActualScope = cache.SCOPE;
                _LK = cache.OPCODE;
                // Update the variables cache
                updateCacheRegisters(a, ActualScope, symbols);
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
                updateCacheRegisters(a, ActualScope, symbols);
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
                        int64_t _register1_HSV = 0;
                        std::string register_1_N = sGenStringLiteralRandom("l_b_o_c_FOR");
                        R->createGR(register_1_N);
                        std::pair<x86::Gp, bool> register_1 = _ASM__searchSymbolToUse(register_1_N, varname, ActualScope, true);
                        bool HighSpeedRegister = register_1.second;
                        if (HighSpeedRegister) {
                            R->destroyGR(register_1_N);
                            register_1_N = ":RR:";
                            _register1_HSV = register_1.first.id();
                        }
                        // Seek local variable info for next vars.
                        if (!_register1_HSV) {
                            lua_localSymbol *SYM = acquireVariableFromExtensionsPtr(varname, ActualScope);
                            SYM->availReg = register_1_N;
                        }
                        qlog0._log2("\033[31m");
                        std::string startPointReg = "NOP"; // Number which we need to reach.
                        std::string toCmpReg = "NOP"; // Reached point ptr
                        std::string stepReg = "NOP"; // How many steps should take every iteration.
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
                                    int64_t value = std::stoll(std::string(b._data.begin(), b._data.end()));
                                    if (startPointReg == "NOP") {
                                        startPointReg = "HYPERVALUE";
                                        _hyperValue__startpoint = value;
                                    } else if (toCmpReg == "NOP"){
                                        toCmpReg = "HYPERVALUE";
                                        _hyperValue__comparepoint = value;
                                    } else if (stepReg == "NOP") {
                                        stepReg = "HYPERVALUE";
                                        _hyperValue__stepPoint = value;
                                    }
                                } else {
                                    if (b.key == _L_EXPRESSION) {
                                        // SHOOOSH.
                                        uint64_t *toSave = nullptr;
                                        // Compare.
                                        std::tuple<bool, x86::Gp, const std::string> K = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, std::string>(true, ""), false, true); // rax will be ignored if using a high speed local var
                                        auto [way, regRaw, regName] = K;
                                        if (!way) { // it must be false to use regRaw
                                            if (regRaw.id() > 11) {
                                                if (startPointReg == "NOP") {
                                                    startPointReg = ":RR:";
                                                    _hyperValue__startpoint = regRaw.id();
                                                } else if (toCmpReg == "NOP") {
                                                    toCmpReg = ":RR:";
                                                    _hyperValue__comparepoint = regRaw.id();
                                                } else if (stepReg == "NOP") {
                                                    stepReg = ":RR:";
                                                    _hyperValue__stepPoint = regRaw.id();
                                                }
                                            } else {
                                                //!!!
                                            }
                                        } else { // Uses new architecture.
                                            if (startPointReg == "NOP") {
                                                startPointReg = regName; // No need to create new ones..
                                            } else if (toCmpReg == "NOP") {
                                                toCmpReg = regName;
                                            } else if (stepReg == "NOP") {
                                                stepReg = regName;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        closures.push_back(_closure_helper{register_1_N, toCmpReg, stepReg, 2, _hyperValue__comparepoint, _hyperValue__stepPoint, varname, _register1_HSV});
                        if (startPointReg == ":RR:") { // High Speed Local
                            x86::Gp RR = x86::Gp::make_r64(_hyperValue__startpoint);
                            a->mov(S(register_1_N), RR);
                        } else if (startPointReg == "HYPERVALUE") { // Direct value
                            if (register_1_N != ":RR:")
                                a->mov(S(register_1_N), _hyperValue__startpoint);
                        } else { // Custom name register
                            a->mov(x86::r11, PTR_MASK);
                            a->and_(S(startPointReg), x86::r11);
                            a->mov(S(register_1_N), S(startPointReg));
                            R->destroyGR(startPointReg);
                        }
                        // Create labels.
                        Label _STARTPOINT = a->new_label();
                        Label _ENDPOINT = a->new_label();
                        a->bind(_STARTPOINT);
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
                Label _STARTPOINT = a->new_label();
                a->jmp(_STARTPOINT);
                a->bind(scopeBlocks.back().second);
                scopeBlocks.pop_back();
                Label _ENDPOINT = a->new_label();
                scopeBlocks.push_back(std::pair<Label, Label>(_STARTPOINT, _ENDPOINT));
                break;
            }
            case l_b_o_c_SCE: {
                // Let's see if FOR parent are there
                uint16_t sv = 0;
                qlog0._log2("\033[31m");
                if (closures.back().closureType == 2) {
                    for_cnt_--;
                    qlog0._log2("end::For__<ReachedPoint>\n");
                    // Counter is at some state..
                    // Uhm, let's check registers.
                    std::string MR = closures.back().mainReg; // Unique register.
                    if (MR == ":RR:") {
                        x86::Gp RR = x86::Gp::make_r64(closures.back().mainReg_HSV); // CMP MAIN POINT
                        if (closures.back().cmpReg == ":RR:") {
                            a->cmp(RR, x86::Gp::make_r64(closures.back().toCmp_));
                        } else if (closures.back().cmpReg == "HYPERVALUE") {
                            a->mov(x86::r11, closures.back().toCmp_);
                            a->cmp(RR, x86::r11);
                        } else {
                            a->cmp(RR, S(closures.back().cmpReg));
                        }
                        a->jge(scopeBlocks.back().second);
                        a->inc(RR);
                        sv = 2;
                    } else {
                        // Direct.
                        if (closures.back().cmpReg == ":RR:") {
                            a->cmp(S(closures.back().mainReg), x86::Gp::make_r64(closures.back().toCmp_));
                        } else if (closures.back().cmpReg == "HYPERVALUE") {
                            a->mov(x86::r11, closures.back().toCmp_);
                            a->cmp(S(closures.back().mainReg), x86::r11);
                        } else {
                            a->cmp(RR, S(closures.back().cmpReg));
                        }
                        sv = 2;
                        a->jge(scopeBlocks.back().second);
                        if (closures.back().stepReg == ":RR:") {
                            a->add(S(closures.back().mainReg), x86::Gp::make_r64(closures.back().step_));
                            sv = 2; 
                        } else if (closures.back().stepReg == "HYPERVALUE") {
                            a->mov(x86::r11, closures.back().step_);
                            a->add(S(closures.back().mainReg), x86::r11);
                            sv = 1;
                        } else if (closures.back().stepReg == "NOP") {
                            a->inc(S(closures.back().mainReg));
                            sv = 1;
                        }
                    }
                    a->jmp(scopeBlocks.back().first);
                    
                    qlog0._log2("end::For__<END>\n");
                }
                // At the most top must close.
                a->bind(scopeBlocks.back().second);
                // Update mainReg if used.
                if (sv == 1) {
                    std::string register_1_N = sGenStringLiteralRandom("l_b_o_c_SCE");
                    R->createGR(register_1_N);
                    std::pair<x86::Gp, bool> z = _ASM__searchSymbolToUse(register_1_N, closures.back()._vName, ActualScope, 2);
                    a->mov(x86::r11, 0x7FF8);
                    a->shl(x86::r11, 12);
                    a->or_(S(closures.back().mainReg), x86::r11);
                    a->mov(x86::qword_ptr(z.first), S(closures.back().mainReg));
                    R->destroyGR(register_1_N);
                    // Make that register be unavailable tho.
                    R->destroyGR(closures.back().mainReg != ":RR:" ? closures.back().mainReg : "");
                    R->destroyGR(closures.back().stepReg);
                    R->destroyGR(closures.back().cmpReg);
                    if (closures.back().mainReg != ":RR:") {
                        lua_localSymbol *SYM = acquireVariableFromExtensionsPtr(closures.back()._vName, ActualScope);
                        SYM->availReg = "";
                    }
                } else if (sv == 2) {
                    R->destroyGR(closures.back().mainReg);
                    R->destroyGR(closures.back().stepReg);
                    R->destroyGR(closures.back().cmpReg);
                }
                closures.pop_back();
                scopeBlocks.pop_back();
                // Exit scope.
                ActualScope = ActualScope->rSCOPE;
                updateCacheRegisters(a, ActualScope, symbols);
                qlog0._log2("\033[0m");
                break;
            }
            case l_b_o_c_STO: {
                //a->mov(x86::rdi, (uint64_t)cache.V);
                //getValueRequestedNput_to(x86::rbx, &cache.p.at(0));
                // GET THE VALUE.
                // always the first value, the multiple values one are STM
                std::string aB = "Search var to use: ";
                //aB.append(std::string(((TString*)cache.p.at(0).at(0).a)->data),((TString*)cache.p.at(0).at(0).a)->len);
                //aB.append("\n");
                qlog0._log2(aB.c_str());
                //qlog0._log2(cache.p.at(0).at(0).addr->getHeaderVarString().c_str());
                qlog0._log2("\n");
                std::tuple<bool, x86::Gp, const std::string> K = CLUA_EvalExprNReturn(&cache.p.at(0), ActualScope, std::pair<bool, std::string>(false, ""), false);
                auto [way, regRaw, regName] = K;
                //_F_ASM_MultiUse_EvalUntil(&cache.p.at(0), &a, ActualScope, _L_NONE, &_);
                int32_t offset = 0;
                offset += cache.toMemOffset;
                qlog0._log2("Save to PTR\n");
                // script mem base string = f_mem_scr
                if (!Script)
                    a->mov(x86::qword_ptr(x86::rbp, (offset*-1)), way ? S(regName) : regRaw);
                else {
                    a->mov(x86::qword_ptr(S("f_mem_scr"), cache.toMemOffset), way ? S(regName) : regRaw);
                }
                if (way) {
                    R->destroyGR(regName);
                }
                qlog0._log2("END save to PTR 'var'\n");
                qlog0._log2("\n");
                _LKUPDT:
                _LK = cache.OPCODE;
                break;
            }
            case l_b_o_c_FUN: {
                qlog0._log2("FunctionGeneration:Start\n");
                R->emitCall();
                a->mov(x86::rdi, (uint64_t)cache.FuncPTR2);
                a->mov(x86::rsi, (uint64_t)cache.SCOPE);
                a->mov(x86::rdx, (uint64_t)true);
                a->call((uint64_t)luaBundleFunction);
                qlog0._log2("FunctionGeneration:End\n");
                qlog0._log2("FunctionGeneration:SaveStart\n");
                a->mov(x86::rbx, x86::rax); // Valid.
                std::string r0_N = sGenStringLiteralRandom("FUNCTION");
                R->createGR(r0_N);
                x86::Gp uGp = _ASM__getPathToSelGp(cache.path->getData(), r0_N, ActualScope, true);
                a->mov(x86::qword_ptr(uGp), x86::rbx);
                qlog0._log2("FunctionGeneration:SaveEnd\n\n");
                _LK = cache.OPCODE;
                break;
            }
            case l_b_o_c_CFN: {
                _CLHASM__call(cache, ActualScope);
                _LK = cache.OPCODE;
                break;
            }
            case l_b_o_c_RET: {
                if (cache.ATR) {
                    std::string name = "return"+sGenStringLiteralRandom("0");
                    R->createGR(name, false, x86::rax);
                    CLUA_EvalExprNReturn(&cache.LLF, ActualScope, std::pair<bool, std::string>(true, name), false);
                    R->destroyGR(name);
                    a->jmp(_ENDPOINT_NONXORRAX_);
                } else {
                    a->jmp(_ENDPOINT_);
                }
                break;
            }
            case l_b_o_c_VTN: { // Maybe its like this: local var0 = hallo["there"];
                //OBSOLETE.
                break;
            }
            default: {
                // Huh?
            }
        }
        pos++;
    }
    //Free those pointers.
    _END_:
    // Improve some memory allocs.
    int32_t toAlloc = -1 * R->getBytesToAlloc();
    if ((toAlloc & 0xF) == 0) {
        toAlloc += 8;
    }
    BaseNode *node = a->first_node();
    while (node) {
        if (node->is_inst()) {
            InstNode *IN = node->as<InstNode>();
            if (IN->inst_id() == x86::Inst::kIdSub && IN->op_count() > 1 && IN->op(1).is_imm()) {
                // Second test, do not override those sub instructions which should'nt be modified.
                if (IN->op(0).as<Reg>() == x86::rsp) {
                    IN->set_op(1, Imm(toAlloc));
                }
            }
        }
        node = node->next();
    }
    
    prepareFinalCode(a);
    
    a->bind(_ENDPOINT_);
    a->xor_(x86::rax, x86::rax);
    a->bind(_ENDPOINT_NONXORRAX_);
    a->add(x86::rsp, toAlloc);
    frontNlowerPushes(a, _CODE, false);
    a->mov(x86::rsp, x86::rbp);
    a->pop(x86::rbp);
    a->ret();
    
    a->finalize();
    
    std::cout << "\033[1;33mAssembly Code:\033[0m \n" << qlog0.data() << "\n\033[1;33mSize: " << code.code_size() << "\033[0m" << std::endl;
    void *toalloc = nullptr;
    Error ERR = rt.add(&toalloc, &code);
    if (ERR != Error::kOk) {
        return nullptr;
    }
    //K(new FuncArgs());
    return toalloc;
}












































