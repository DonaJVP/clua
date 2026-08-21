#include "lua.hpp"
#include "cllex.hpp"
#include <cstdint>
#include <luaconf.h>
#include <stdexcept>
#include <iostream>
#include <stdint.h>
#include <string>
#include <sys/mman.h>

std::string luaLexFrameKeyToString(_Lua_Lex_Keys k) {
    switch (k) {
        case _L_SYNTAX_DEC: {
            return "< - >";
        }
        case _L_SYNTAX_DIV: {
            return "< / >";
        }
        case _L_SYNTAX_MUL: {
            return "< * >";
        }
        case _L_SYNTAX_SUM: {
            return "< + >";
        }
    }
    return "";
}

#include "ltable.hpp"

//BEGIN lua_AddrPath
lua_AddrPath::lua_AddrPath() {}
bool lua_AddrPath::needToResolveAddr() { return ATTRIB ? false : (rawAddr.size() > 1); }
LuaLexFrame *lua_AddrPath::getHeader() { return &rawAddr.at(0); }
LuaLexFrame *lua_AddrPath::getBack() { return &rawAddr.at(rawAddr.size()-1); }
std::string lua_AddrPath::getHeaderVarString() {
    LuaLexFrame *header = getHeader();
    //std::cout << header << "; " << rawAddr.size() << std::endl;
    std::string a = std::string(header->_data.begin(), header->_data.end());
    return a;
}
void lua_AddrPath::assignNewAddr(const std::vector<LuaLexFrame> new_) {
    rawAddr = std::move(new_);
}
std::vector<LuaLexFrame> *lua_AddrPath::getData() { return &rawAddr; }
//May include something related.
LuaLexFrame makeSingleExprP(std::vector<LuaLexFrame> *keys, uint32_t *pos) {
    // Parenthesis
    LuaLexFrame res = LuaLexFrame(_L_EXPRESSION);
    std::vector<std::vector<LuaLexFrame>> contents{};
    LuaLexFrame cache = LuaLexFrame();
    uint16_t level = 0;
    contents.push_back(std::vector<LuaLexFrame>());
    while (true) {
        try {
            cache = keys->at(*pos);
        } catch (std::out_of_range &e) {
            res.key = _L_EXPRESSION;
            res.EXPR = contents;
            return res;
        }
        switch (cache.key) {
            case _L_TABLE_END: {
                break;
            }
            case _L_TABLE_START: {
                *pos = *pos + 1;
                contents.at(level).push_back(makeSingleTable(keys, pos));
                break;
            }
            case _L_VARNAME: {
                LuaLexFrame path = makeSinglePath(keys, pos);
                contents.at(level).push_back(path);
                *pos = *pos - 1;
                break;
            }
            case _L_SEPARATOR: {
                if (cache.ATTRIB) {
                    contents.push_back(std::vector<LuaLexFrame>());
                    level++;
                } else {
                    goto _DEFAULT;
                }
                break;
            }
            case _L_F_ARGS_START: {
                *pos = *pos + 1;
                LuaLexFrame _F = makeSingleExprP(keys, pos);
                if (contents.at(level).size() > 1) {
                    LuaLexFrame _C = std::move(contents.at(level).at(contents.at(level).size()-1));
                    if (_C.key == _L_PATH) {
                        // Erase that key and then push new.
                        contents.at(level).pop_back();
                        LuaLexFrame _CALL(_L_CALL);
                        _CALL.addr = _C.addr;
                        _CALL.EXPR = _F.EXPR;
                        contents.at(level).push_back(_CALL);
                        break;
                    } else {
                        goto _CC_;
                    }
                    break;
                }
                _CC_:
                contents.at(level).push_back(_F);
                break;
            }
            case _L_F_ARGS_END: {
                res.key = _L_EXPRESSION;
                res.EXPR = contents;
                return res;
            }
            default: {
                _DEFAULT:
                contents.at(level).push_back(cache);
            }
        }
        *pos = *pos + 1;
    }
}
LuaLexFrame makeSingleExprB(std::vector<LuaLexFrame> *keys, uint32_t *pos) {
    LuaLexFrame res;
    std::vector<LuaLexFrame> contents;
    LuaLexFrame cache;
    while (true) {
        try {
            cache = keys->at(*pos);
        } catch (std::out_of_range &e) {
            res.declr = cache.declr;
            res.EXPR_BRKT = contents;
            res.key = _L_EXPRESSION_BRKT;
            return res;
        }
        switch (cache.key) {
            case _L_VARNAME: {
                LuaLexFrame r_ = makeSinglePath(keys, pos);
                contents.push_back(r_);
                *pos = *pos - 1;
                break;
            }
            case _L_F_ARGS_START: {
                LuaLexFrame sfr = makeSingleExprP(keys, pos);
                //*pos = *pos + 1;
                contents.push_back(cache);
                break;
            }
            case _L_ON_TO_GO_END: {
                // This key has declr value, so, pass it to res
                res.declr = cache.declr;
                res.EXPR_BRKT = contents;
                res.key = _L_EXPRESSION_BRKT;
                return res;
            }
            default: {
                contents.push_back(cache);
            }
        }
        *pos = *pos + 1;
    }
}
LuaLexFrame makeSinglePath(std::vector<LuaLexFrame> *keys, uint32_t *pos) {
    lua_AddrPath *addr = new lua_AddrPath();
    LuaLexFrame toRet;
    std::vector<LuaLexFrame> *data = addr->getData();
    LuaLexFrame cache;
    bool _FIRSTADDR = false;
    bool declr = false;
    bool local = false;
    uint8_t atr = 0;
    while (true) {
        try {
            cache = keys->at(*pos);
        } catch (std::out_of_range &e) {
            goto _END_;
        }
        switch (cache.key) {
            case _L_VARNAME: {
                if (!_FIRSTADDR) {
                    local = cache.local;
                    toRet.subkey = cache.subkey;
                    toRet.ATTRIB = cache.ATTRIB;
                    _FIRSTADDR = true;
                }
                declr = cache.declr;
                data->push_back(cache);
                break;
            }
            case _L_ON_TO_GO: {
                if (cache.ATTRIB == 0xFF) { // A call between a Object.
                    atr = cache.ATTRIB;
                    data->push_back(cache);
                    break;
                }
                if (cache.ATTRIB == 1) { // Non ".", but []
                    // Compile their contents & put to vector.
                    *pos = *pos + 1;
                    data->push_back(makeSingleExprB(keys, pos));
                } else {
                    data->push_back(cache);
                }
                break;
            }
            default: {
                try {
                    if (!data->at(data->size()-1).declr) {
                        (&data->at(data->size()-1))->declr = declr;
                    }
                } catch (std::out_of_range &e) {
                    // Do nothing.
                }
                toRet.key = _L_PATH;
                toRet.addr = addr;
                toRet.addr->getBack()->_LK = true;
                toRet.addr->ATTRIB = atr;
                return toRet;
            }
        }
        *pos = *pos + 1;
    }
    _END_:
    toRet.key = _L_PATH;
    toRet.addr = addr;
    toRet.addr->getBack()->_LK = true;
    return toRet;
}
//END lua_AddrPath

struct _VarHeader {
    lua_AddrPath *Addr;
    std::vector<LuaType> howIsUsed;
    bool inmutable_by_def = false;
    LuaType FinalType = LuaUnknown;
    std::vector<LuaLexFrame*> _ASSOC_VARS;
};

// An expression like ("a") or (1321) are predictable than (arr + 2)
bool isExprPredictable(std::vector<LuaLexFrame> *k) {
    for (LuaLexFrame &ele: *k) {
        if (ele.key == _L_VARNAME) {
            return false; // Not predictable
        }
    }
    return true;
}

LuaLexFrame *getFrameFromPathFront(std::vector<LuaLexFrame> *Keys, uint32_t *pos, std::vector<LuaLexFrame> *accu = nullptr, bool *predictable = nullptr) {
    LuaLexFrame *GOT;
    LuaLexFrame *To;
    bool _001 = false;
    bool _TOENCLOSE = false;
    std::vector<LuaLexFrame> PRDT;
    while (true) {
        try {
            GOT = &Keys->at(*pos);
        } catch (std::out_of_range &e) {
            return GOT;
        }
        switch (GOT->key) {
            case _L_VARNAME: {
                if (!_001) {
                    _001 = true;
                    To = GOT;
                }
                if (accu != nullptr)
                    accu->push_back(*GOT);
                break;
            }
            case _L_ON_TO_GO: {
                if (accu != nullptr)
                    accu->push_back(*GOT);
                if (GOT->ATTRIB) {
                    _TOENCLOSE = true;
                    PRDT.push_back(*GOT);
                }
                break; // Ignore
            }
            case _L_ON_TO_GO_END: {
                if (accu != nullptr)
                    accu->push_back(*GOT);
                if (_TOENCLOSE) {
                    _TOENCLOSE = false;
                    PRDT.push_back(*GOT);
                }
            }
            default: {
                if (!_TOENCLOSE) {
                    //Proc predictament
                    if (predictable != nullptr)
                        *predictable = isExprPredictable(&PRDT);
                    return To;
                }
            }
        }
        *pos = *pos + 1;
    }
}

struct _SCOPE_EMU {
    _SCOPE_EMU *predessor = nullptr;
    std::vector<_SCOPE_EMU*> succesors;
    std::unordered_map<std::string, _VarHeader> vars;
    std::unordered_map<std::string, _VarHeader*> usedVarsExternal;
    _VarHeader *SEARCH_VAR(std::string a, bool onlyThisScope = false);
    void sSearchNaddVal(LuaLexFrame *a, LuaType b);
    void sSearchVal(LuaLexFrame *a);
    LuaLexFrame *LexHeader;
    uint64_t _scopeID; // Should make differences between others.
    bool _toCheckAvailable = false;
    bool mainScope = false;
    
};

void _SCOPE_EMU::sSearchVal(LuaLexFrame *a) {
    _VarHeader *val = SEARCH_VAR(a->addr->getHeaderVarString());
    if (val != nullptr) {
        std::cout << "Added variable: " << a->addr->getHeaderVarString() << std::endl;
        val->_ASSOC_VARS.push_back(a);
    }
        
}

void _SCOPE_EMU::sSearchNaddVal(LuaLexFrame *a, LuaType b) {
    _VarHeader *val = SEARCH_VAR(a->addr->getHeaderVarString());
    if (val == nullptr)
        return;
    if (val != nullptr) {
        val->howIsUsed.push_back(b);
        val->_ASSOC_VARS.push_back(a);
    }
}

_VarHeader *_SCOPE_EMU::SEARCH_VAR(std::string a, bool onlyThisScope) {
    // Search on the predessor and then there and then there and many many more.
    if (vars.find(a) == vars.end()) {
        if (predessor != nullptr) 
            if (!onlyThisScope) {
                _VarHeader *u = predessor->SEARCH_VAR(a);
                if (u == nullptr)
                    return nullptr;
                usedVarsExternal.insert(std::pair<std::string, _VarHeader*>(u->Addr->getHeaderVarString(), u));
                return u;
            } else
                return nullptr;
        else
            return nullptr;
    } else {
        return &vars.at(a);
    }
    return nullptr; // I don't want ud2 here. [If the func reaches this point would be diabolical lol]
}

// std::vector<LuaTypes>
LuaType getOverallType(std::vector<LuaType> Types) {
    LuaType sel = LuaUnknown;
    bool _NOT_SEL = false;
    for (LuaType &t: Types) {
        if (!_NOT_SEL) {
            sel = t;
            _NOT_SEL = true;
        } else {
            if (t == LuaInteger) {
                if (sel == LuaNumber) {
                    sel = LuaNumber;
                } else if (sel == LuaInteger) {
                    sel = LuaInteger;
                } else {
                    return LuaUnknown;
                }
            } else if (t == LuaNumber) {
                if (sel == LuaInteger) {
                    sel = LuaNumber;
                } else if (sel == LuaNumber) {
                    sel = LuaNumber;
                } else {
                    return LuaUnknown;
                }
            } else {
                if (sel != t) {
                    return LuaUnknown;
                }
            }
        }
    }
    return sel;
}

// Simplify each character and numbers/integers in the expression.
// stops at _L_F_ARGS_END and _L_ON_TO_GO_END

static std::string pushDump(std::vector<LuaLexFrame> *k) {
    std::string buffer = "| ";
    for (LuaLexFrame &l: *k) {
        buffer.append("$[");
        buffer.append(std::to_string(l.key));
        buffer.append("]");
        if (l.key == _L_PATH) {
            buffer.append("+=Path{");
            buffer.append(l.addr->getHeaderVarString());
            buffer.append("}");
        }
        if (l.key > 39 && l.key < 46) {
            buffer.append("+=MathOperator");
            buffer.append(luaLexFrameKeyToString(l.key));
        }
        buffer.append(" | ");
    }
    return buffer;
}

std::vector<LuaLexFrame> simplifyExpression(std::vector<LuaLexFrame> *keys, _SCOPE_EMU *now) {
    LuaLexFrame Actual(_L_NONE);
    std::vector<LuaLexFrame> vct; // Simplified version of *keys
    bool concat = false;
    uint32_t pos = 0;
    int8_t quotient = 0;
    bool _VARNAME = false;
    _Lua_Lex_Keys MATHOP = _L_NONE;
    std::vector<LuaType> OverallType;
    LuaType _lastNumberOperand = LuaUnknown;
    while (true) {
        try {
            Actual = keys->at(pos);
        } catch (std::out_of_range &e) {
            goto __END;
        }
        switch (Actual.key) {
            case _L_CALL: {
                std::vector<LuaLexFrame> *_AUX_NOW;
                uint16_t _spos = 0;
                while (true) {
                    try {
                        _AUX_NOW = &Actual.EXPR.at(_spos);
                    } catch (std::out_of_range &e) {
                        break;
                    }
                    Actual.EXPR.at(_spos) = simplifyExpression(_AUX_NOW, now);
                    _spos++;
                }
                OverallType.push_back(LuaUnknown);
                if (concat) {
                    vct.push_back(LuaLexFrame(_L_CONCAT));
                    concat = false;
                }
                if (MATHOP != _L_NONE) {
                    MATHOP = _L_NONE;
                    vct.push_back(LuaLexFrame(MATHOP));
                }
                vct.push_back(Actual);
                break;
            }
            // Must be some variable or some like that..
            case _L_PATH: {
                // Check if theres some operations in the list...
                uint8_t _00 = 0;
                if (MATHOP != _L_NONE) {
                    // Inject first those expressions.
                    vct.push_back(LuaLexFrame(MATHOP));
                    // Should see which type is the operand..
                    OverallType.push_back(_lastNumberOperand);
                    _lastNumberOperand = LuaUnknown;
                    MATHOP = _L_NONE;
                    _00 = 1;
                }
                {
                    if (pos == 0) { // Initialize
                        _lastNumberOperand = getOverallType((now->SEARCH_VAR(Actual.addr->getHeaderVarString(), false) != nullptr) ? now->SEARCH_VAR(Actual.addr->getHeaderVarString(), false)->howIsUsed : std::vector<LuaType>(LuaUnknown));
                        vct.push_back(Actual);
                        _VARNAME = true;
                        now->sSearchVal(&vct.at(vct.size()-1)); //Hell yeah.
                        break;
                    }
                }
                if (concat) {
                    vct.push_back(LuaLexFrame(_L_CONCAT));
                    concat = false;
                    _00 = 2;
                }
                
                vct.push_back(Actual);
                LuaLexFrame *act = &vct.at(vct.size()-1);
                // Might update this variable from the past operations.
                if (_00 > 0) {
                    if (!Actual.addr->needToResolveAddr()) {
                        if (_00 == 1) {
                            _VarHeader *_ = now->SEARCH_VAR(act->addr->getHeaderVarString(), false);
                            if (!_) {
                                _lastNumberOperand = LuaUnknown;
                                goto _DECL_VARNAME;
                            }
                            auto pHolder = _->howIsUsed;
                            LuaType _c0 = getOverallType(pHolder);
                            now->sSearchNaddVal(act, _c0);
                            _lastNumberOperand = _c0;
                        } else {
                            now->sSearchNaddVal(act, LuaString);
                        }
                    }
                } else {
                    if (!Actual.addr->needToResolveAddr())
                        now->sSearchVal(act);
                }
                _DECL_VARNAME:
                _VARNAME = true;
                break;
            }
            // Math
            case _L_NUMBER: {
                if (concat) {
                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, "Concat operation with number/integer");
                    m_LuaErrorHandler->setFatal(true);
                    return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                }
                if (MATHOP != _L_NONE) {
                    OverallType.push_back(_lastNumberOperand);
                    _lastNumberOperand = LuaUnknown;
                    // Do not calculate data which they arent numbers!
                    LuaLexFrame *LF_RAW = nullptr;
                    quotient++;
                    try {
                        LF_RAW = &vct.at(vct.size()-quotient);
                    } catch (std::out_of_range &e) {
                        m_LuaErrorHandler->reportError(_lua_es_NotAIntNum, 0, "Attempt to do math with no value");
                        m_LuaErrorHandler->setFatal(true);
                        return std::vector<LuaLexFrame>();
                    }
                    if (quotient>1)
                        quotient-=2;
                    else
                        quotient--;
                    LuaLexFrame *LF = nullptr;
                    if (LF_RAW->key == _L_PATH) {
                        LF = LF_RAW->addr->getHeader();
                        if (!LF_RAW->addr->needToResolveAddr()) {
                            now->sSearchNaddVal(LF_RAW, Actual.ATTRIB == true ? LuaNumber : LuaInteger);
                        }
                        goto _0_0_1_MT;
                    } else
                        LF = LF_RAW;
                    if (LF->key != _L_NUMBER && LF->key != _L_PATH) {
                        m_LuaErrorHandler->reportError(_lua_es_NotAIntNum, 0, "Attempt to do math with a non integer/number value!");
                        m_LuaErrorHandler->reportError(_lua_es_NotAIntNum, 0, std::to_string(LF_RAW->key).c_str());
                        m_LuaErrorHandler->setFatal(true);
                        return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                    } else if (LF->key != _L_PATH) {
                        //OverallType.push_back(Actual.ATTRIB == true ? LuaNumber : LuaInteger);
                        goto _0_0_1_MT;
                    }
                    double _0 = std::stod(std::string(LF->_data.begin(), LF->_data.end()));
                    double _1 = std::stod(std::string(Actual._data.begin(), Actual._data.end()));
                    switch (MATHOP) {
                        case _L_SYNTAX_DEC: {
                            _0 = _0 - _1;
                            break;
                        }
                        case _L_SYNTAX_SUM: {
                            _0 = _0 + _1;
                            break;
                        }
                        case _L_SYNTAX_DIV: {
                            _0 = _0 / _1;
                            break;
                        }
                        case _L_SYNTAX_MUL: {
                            _0 = _0 * _1;
                            break;
                        }
                        default: {
                            m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, "Invalid arithmetic operation: " + std::to_string(MATHOP));
                            m_LuaErrorHandler->setFatal(true);
                            return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                        }
                    }
                    std::string res = std::to_string(_0);
                    LF->_data = std::vector<uint8_t>(res.begin(), res.end());
                    MATHOP = _L_NONE;
                } else {
                    _0_0_1_MT:
                    MATHOP = _L_NONE;
                    vct.push_back(Actual);
                }
                //OverallType.push_back(Actual.ATTRIB == true ? LuaNumber : LuaInteger);
                _lastNumberOperand = Actual.ATTRIB == true ? LuaNumber : LuaInteger;
                break;
            }
            case _L_SYNTAX_DIV: {
                if (MATHOP != _L_NONE) {
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, "Another operator exists at predessor of this operation: / [" + luaLexFrameKeyToString(Actual.key) + "]>> " + luaLexFrameKeyToString(Actual.key));
                    m_LuaErrorHandler->setFatal(true);
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, pushDump(keys));
                    return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                }
                MATHOP = Actual.key;
                if (_VARNAME) {
                    vct.push_back(Actual);
                    //MATHOP = _L_NONE;
                    quotient++;
                    _VARNAME = false;
                }
                //OverallType.push_back(LuaNumber);
                
                break;
            }
            case _L_SYNTAX_MUL: {
                if (MATHOP != _L_NONE) {
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, "Another operator exists at predessor of this operation: * [" + luaLexFrameKeyToString(Actual.key) + "]>> " + luaLexFrameKeyToString(Actual.key));
                    m_LuaErrorHandler->setFatal(true);
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, pushDump(keys));
                    return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                }
                MATHOP = Actual.key;
                if (_VARNAME) {
                    vct.push_back(Actual);
                    //MATHOP = _L_NONE;
                    _VARNAME = false;
                    quotient++;
                }
                //OverallType.push_back(LuaNumber);
                
                break;
            }
            case _L_SYNTAX_SUM: {
                if (MATHOP != _L_NONE) {
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, "Another operator exists at predessor of this operation: + [" + luaLexFrameKeyToString(Actual.key) + "]>> " + luaLexFrameKeyToString(Actual.key));
                    m_LuaErrorHandler->setFatal(true);
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, pushDump(keys));
                    return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                }
                MATHOP = Actual.key;
                if (_VARNAME) {
                    vct.push_back(Actual);
                    //MATHOP = _L_NONE;
                    _VARNAME = false;
                    quotient++;
                }
                //OverallType.push_back(LuaNumber);
                break;
            }
            case _L_SYNTAX_DEC: {
                if (MATHOP != _L_NONE) {
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, "Another operator exists at predessor of this operation: - [" + luaLexFrameKeyToString(Actual.key) + "]>> " + luaLexFrameKeyToString(Actual.key));
                    m_LuaErrorHandler->setFatal(true);
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, pushDump(keys));
                    return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                }
                MATHOP = Actual.key;
                if (_VARNAME) {
                    vct.push_back(Actual);
                    //MATHOP = _L_NONE;
                    _VARNAME = false;
                    quotient++;
                }
                //OverallType.push_back(LuaNumber);
                
                break;
            }
            // String
            case _L_STRING: {
                if (!concat) {
                    vct.push_back(Actual);
                } else {
                    //LuaLexFrame *LF = &vct.at(vct.size()-1);
                    
                    LuaLexFrame *LF_RAW = nullptr;
                    try {
                        LF_RAW = &vct.at(vct.size()-1);
                    } catch (std::out_of_range &e) {
                        // Must be an error.
                        m_LuaErrorHandler->reportError(_lua_es_NotAIntNum, 0, "Attempt to do concatenate with no value!");
                        m_LuaErrorHandler->setFatal(true);
                        return std::vector<LuaLexFrame>();
                    } 
                    if (LF_RAW->key == _L_PATH) {
                        if (!LF_RAW->addr->needToResolveAddr()) {
                            now->sSearchNaddVal(LF_RAW, LuaString);
                        }
                        vct.push_back(LuaLexFrame(_L_CONCAT));
                        vct.push_back(Actual);
                        break;
                    }
                    
                    if (LF_RAW->key != _L_STRING && LF_RAW->key != _L_CALL) {
                        m_LuaErrorHandler->reportError(_lua_es_NotAString, 0, "Attempt to concatenate with a non string value!");
                        m_LuaErrorHandler->setFatal(true);
                        return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                    } else {
                        if (LF_RAW->key == _L_CALL) {
                            vct.push_back(LuaLexFrame(_L_CONCAT));
                            vct.push_back(Actual);
                            concat = false;
                            break;
                        }
                    }
                    std::string a = std::string(LF_RAW->_data.begin(), LF_RAW->_data.end());
                    std::string b = std::string(Actual._data.begin(), Actual._data.end());
                    std::string c = a + b;
                    // Put result on the last key living on vct
                    LF_RAW->_data.clear();
                    std::cout << c << std::endl;
                    LF_RAW->_data = std::vector<uint8_t>(c.begin(), c.end());
                    // transform the .a pointer too
                    //LF_RAW->a = (TString*)
                    // Mhmhmmh, i think modifying it should be done.
                    TString *toMod = (TString*)LF_RAW->a;
                    free(toMod->data);
                    toMod->data = (char*)malloc(c.size());
                    toMod->len = c.size();
                    memcpy(toMod->data, c.data(), c.size());
                }
                OverallType.push_back(LuaString);
                break;
            }
            case _L_CONCAT: {
                if (MATHOP != _L_NONE) {
                    m_LuaErrorHandler->reportError(_lua_es_BadTyping, 0, "Arithmetic operator before concatenation operation is illegal [" + luaLexFrameKeyToString(Actual.key) + "/" + luaLexFrameKeyToString(MATHOP) + "]");
                    m_LuaErrorHandler->setFatal(true);
                    return std::vector<LuaLexFrame>({LuaLexFrame(_L_NONE)});
                }
                concat = true;
                /*if (_VARNAME) {
                    vct.push_back(Actual);
                    _VARNAME = false;
                    concat = false;
                }*/
                OverallType.push_back(LuaString);
                break;
            }
            // Default
            default: {
                if (concat) {
                    vct.push_back(LuaLexFrame(_L_CONCAT));
                    concat = false;
                }
                if (MATHOP != _L_NONE) {
                    MATHOP = _L_NONE;
                    vct.push_back(LuaLexFrame(MATHOP));
                }
                vct.push_back(Actual); // I do not wanna put checkers for ((function(aaa) return aaa*2+6 end)(81))
            }
        }
        pos++;
    }
    __END:
    if (_lastNumberOperand != LuaUnknown) {
        OverallType.push_back(_lastNumberOperand);
    }
    LuaLexFrame overall(_L_OVERALLTYPECHECKER);
    overall.ATTRIB = (uint8_t)getOverallType(OverallType);
    vct.push_back(overall);
    return vct;
}



// Get expr value

LuaLexFrame getExprValue(std::vector<LuaLexFrame> *k, uint32_t *pos, lua_Scope *scope, bool _returnEmptyIfVariables) {
    LuaLexFrame cache0(_L_NONE);
    uint32_t *to_it = new uint32_t(0);
    _Lua_Lex_Keys M_OP = _L_NONE;
    bool cnt = false;
    bool vnm = false;
    LuaLexFrame getted(_L_NONE);
    bool _0_hNumberDef = false;
    while (true) {
        try {
            cache0 = k->at(*pos);
            
        } catch (std::out_of_range &e) {
            return getted;
        }
        // Arithmetic
        if (cache0.key >= 40 && cache0.key <= 45) {
            M_OP = cache0.key;
            continue;
        }
        switch (cache0.key) {
            case _L_FALSE: {
                getted = cache0;
                break;
            }
            case _L_TRUE: {
                getted = cache0;
                break;
            }
            case _L_PATH: {
                vnm = true;
                if (_returnEmptyIfVariables) {
                    return LuaLexFrame(_L_NONE);
                } else {
                    //TODO: Compute manually those variables.
                }
                break;
            }
            case _L_EXPRESSION: {
                uint32_t _0 = 0;
                LuaLexFrame f = getExprValue(k, &_0, scope, _returnEmptyIfVariables);
                getted = f;
                break;
            }
            // Arithmetic
            case _L_NUMBER: {
                if (cnt) {
                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, "Mixing arithmetic operations with string operations");
                    m_LuaErrorHandler->setFatal(true);
                    return LuaLexFrame(_L_NOP);
                }
                getted.ATTRIB = cache0.ATTRIB & _0_hNumberDef;
                if (M_OP != _L_NONE) {
                    // Get value of the firstest.
                    std::vector<uint8_t> _0 = getted._data;
                    std::vector<uint8_t> _1 = cache0._data;
                    //Transform to real data.
                    double _0_0 = std::stod(std::string(_0.begin(), _0.end()));
                    double _1_0 = std::stod(std::string(_1.begin(), _1.end()));
                    switch (M_OP) {
                        case _L_SYNTAX_DEC: {
                            _0_0 = _0_0 - _1_0;
                            break;
                        }
                        case _L_SYNTAX_DIV: {
                            _0_0 = _0_0 / _1_0;
                            break;
                        }
                        case _L_SYNTAX_MUL: {
                            _0_0 = _0_0 * _1_0;
                            break;
                        }
                        case _L_SYNTAX_SUM: {
                            _0_0 = _0_0 + _1_0;
                            break;
                        }
                        default: {
                            m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, "Unrecognized command for Arithmetic operation: " + std::to_string(M_OP));
                            m_LuaErrorHandler->setFatal(true);
                            return LuaLexFrame(_L_NOP);
                        }
                    }
                    //Save to getted the returned value.
                    std::string _DATA = std::to_string(_0_0);
                    getted._data = std::vector<uint8_t>(_DATA.begin(), _DATA.end());
                    M_OP = _L_NONE; // Reset.
                } else {
                    //Save getted for later operations.
                    getted = cache0;
                }
                break;
            }
            // Strings
            case _L_STRING: {
                if (!cnt) {
                    // Save string to getted.
                    // Check for errors
                    if (M_OP != _L_NONE) {
                        m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, "Mixing arithmetic operations with string operations");
                        m_LuaErrorHandler->setFatal(true);
                        return LuaLexFrame(_L_NOP);
                    }
                    //Save right now
                    getted = cache0;
                } else {
                    std::string a = std::string(getted._data.begin(), getted._data.end());
                    std::string b = std::string(cache0._data.begin(), cache0._data.end());
                    // Proc
                    std::string c = a + b;
                    // Save
                    getted._data = std::vector<uint8_t>(c.begin(), c.end());
                    getted.key = _L_STRING;
                    // Reset some vars
                    cnt = false;
                }
                break;
            }
            case _L_CONCAT: {
                cnt = true;
                break;
            }
            case _L_OVERALLTYPECHECKER: {
                break; // Skip this.
            }
            // Other
            default: {
                return getted;
            }
        }
        if (cache0.key == _L_NUMBER && cache0.ATTRIB)
            _0_hNumberDef = true;
        *pos = *pos + 1;
    }
}

void updateVariable(std::vector<LuaLexFrame> *Keys, LuaLexFrame K) {
    uint32_t pos = 0;
    LuaLexFrame *AF;
    while (true) {
        try {
            AF = &Keys->at(pos);
        } catch (std::out_of_range &e) {
            return;
        }
        //Catchie-catchie
        switch (AF->key) {
            case _L_VARNAME: {
                LuaLexFrame *FRM = getFrameFromPathFront(Keys, &pos);
                if (K._data == FRM->_data) { // Slow, but good.
                    FRM->subkey = K.subkey;
                }
                break;
            }
            default: {
                //nothing
            }
        }
        pos++;
    }
    //Update variable means put its true type along all function.
}

static Values nullPtr = 0xF00000000000000F;

Values *getValueFromtable(std::vector<LuaLexFrame> *path) {
    Values *ptr = nullptr;
    if (path == nullptr) {
        m_LuaErrorHandler->reportWarning(_lua_es_Illegal, 0, "getValueFromtable(): Received nullptr!");
        return &nullPtr;
    }
    lua_Table *nt = nullptr;
    LuaLexFrame NOW;
    uint32_t pos = 0;
    bool gotHeader = false;
    while (true) {
        try {
            NOW = path->at(pos);
        } catch (std::out_of_range &e) {
            return ptr;
        }
        ///
        switch (NOW.key) {
            case _L_VARNAME: {
                if (!gotHeader) {
                    //Must search somewhere...
                    Values *_VL = (Values*)_F_ASM_NOTGUARANTEED_GETPTR_NOALLOC(m_General, (TString*)NOW.a, &nullPtr);
                    if (*_VL != nullPtr) {
                        /*if (lua_getVarType(gVal) != LuaTable) {
                            m_LuaErrorHandler->reportWarning(_lua_es_FutureCrashAtRuntime, 0, std::string("Expected table got " + getLuaTypeString(lua_getVarType(gVal))));
                            return nullptr;
                        }*/
                        // Search value and the put it
                        ptr = _VL;//lua_getPtr(gVal);
                    } else {
                        // Do nothing, expect at future.
                        return nullptr;
                    }
                    gotHeader = true;
                } else {
                    // Ptr should be a lua_Table*
                    if (nt == nullptr) {
                        m_LuaErrorHandler->reportWarning(_lua_es_FutureCrashAtRuntime, 0, std::string("Expected table got " + getLuaTypeString(lua_getVarType(*(Values*)ptr))));
                        return nullptr;
                    }
                    void *V = _F_ASM_NOTGUARANTEED_GETPTR_NOALLOC(nt, (TString*)NOW.a, nullptr);
                    ptr = (Values*)V;
                }
                break;
            }
            case _L_ON_TO_GO: {
                if (!NOW.ATTRIB) {
                    Values *k = (Values*)ptr;
                    nt = (lua_Table*)lua_getPtr(*k);
                } else {
                    //Uh oh..
                    pos++;
                    LuaLexFrame CNT = getExprValue(path, &pos);
                    switch (CNT.key) {
                        case _L_NUMBER: {
                            int IDX = (int)std::stod(std::string(CNT._data.begin(), CNT._data.end()));
                            // Number numbery, so, index it is!
                            // Number/Integer index is easier on programming, but strings.
                            if (nt->asize >= IDX) {
                                //Might be there!
                                ptr = &nt->array[IDX];
                            } else {
                                ptr = nullptr;
                                return ptr; // Unpredictable
                            }
                            break;
                        }
                        case _L_STRING: {
                            // Not that ez but i can still persisting...
                            std::string name = std::string(CNT._data.begin(), CNT._data.end());
                        }
                        default: {
                            //Bad prediction...
                            return nullptr;
                        }
                    }
                }
                break;
            }
            default: {
                
            }
        }
        pos++;
    }
    return ptr;
}

_Lua_Lex_Keys __LuaType_toLexKey(LuaType _0) {
    switch (_0) {
        case LuaInteger: {
            return _L_INT;
        }
        case LuaNumber: {
            return _L_DOUBLE;
        }
        case LuaString: {
            return _L_STRING;
        }
        case LuaFunction: {
            return _L_FUNCTIONPOINTER;
        }
        case LuaObject: {
            return _L_OBJECT;
        }
        case LuaTable: {
            return _L_TABLE;
        }
        case LuaNil: {
            return _L_NIL;
        }
        case LuaBoolean: {
            return _L_BOOL;
        }
        case LuaUnknown: {
            return _L_UNKNOWN;
        }
        case LuaERR:
            return _L_UNKNOWN;
          break;
        }
    return _L_UNKNOWN;
}

lua_AddrPath *reCheckNsimplifyAddr(lua_AddrPath *toSimplify, _SCOPE_EMU *scope) {
    lua_AddrPath *nw = new lua_AddrPath();
    LuaLexFrame *pFrm;
    uint16_t pos = 0;
    while (true) {
        try {
            pFrm = &toSimplify->getData()->at(pos);
        } catch (std::out_of_range &e) {
            break;
        }
        
    }
    free(toSimplify);
    return nw;
}

lua_Expression computeExpression(lua_Expression tc, _SCOPE_EMU *scope, std::vector<LuaType> *ovrType = nullptr, bool forRelSearch = false) {
    lua_Expression new_;
    bool _enableOverallType = false;
    if (tc.size() == 1) {
        _enableOverallType = true;
    }
    for (std::vector<LuaLexFrame> &v: tc) {
        std::vector<LuaLexFrame> _N{};
        // Let's proc the vectors.
        bool _0_P = false;
        LuaLexFrame *now;
        LuaLexFrame cache_0_;
        uint16_t pos = 0;
        while (true) {
            try {
                now = &v.at(pos);
            } catch (std::out_of_range &e) {
                if (_0_P) {
                    _N.push_back(cache_0_);
                    _0_P = false;
                }
                goto _ENDZONE;
            }
            switch (now->key) {
                case _L_FUNCTION: {
                    m_LuaErrorHandler->reportError(_lua_es_Illegal, 0, std::string("Function inside expression are not legal!"));
                    return lua_Expression();
                }
                case _L_PATH: {
                    _0_P = true;
                    cache_0_ = *now;
                    break;
                }
                case _L_EXPRESSION: {
                    if (_0_P) {
                        // Call.
                        LuaLexFrame _CALL(_L_CALL);
                        _CALL.addr = cache_0_.addr;
                        _CALL.EXPR = now->EXPR;
                        //_CALL.ATTRIB = 0;
                        
                        if (scope->mainScope) {
                            if (now->ATTRIB == 1) { // Is it a non addressable address? [lol]
                                _CALL.skipcheck = false;
                                _CALL.a = nullptr;
                                goto _FINAL;
                            }
                            Values *ptr = getValueFromtable(cache_0_.addr->getData());
                            if ((uintptr_t)ptr != 0xF00000000000000F) { // That number is represented as NULL for it.
                                _CALL.a = ptr; // Skip the first 8 bytes
                                _CALL.skipcheck = ptr != nullptr ? true : false; // Mark it as skippable check
                            } else {
                                _CALL.a = nullptr;
                                _CALL.skipcheck = false;
                            }
                        } else {
                            _CALL.a = nullptr;
                            _CALL.skipcheck = false;
                        }
                        _FINAL:
                        
                        // Might need to know their return value
                        
                        _N.push_back(_CALL);
                        _0_P = false;
                    } else {
                        LuaType res;
                        now->EXPR = computeExpression(now->EXPR, scope);
                        now->subkey = __LuaType_toLexKey(res);
                        _N.push_back(*now);
                    }
                    break;
                }
                default: {
                    if (_0_P) {
                        _N.push_back(cache_0_);
                    }
                    _0_P = false;
                    _N.push_back(*now);
                }
            }
            pos++;
        }
        /// Push
        _ENDZONE:
        new_.push_back(_N);
    }
    if (forRelSearch) {
        // Just add new structures usage in there.
        uint8_t _loopPhases = 0;
        for (std::vector<LuaLexFrame> &vct: static_cast<std::vector<std::vector<LuaLexFrame>>>(new_)) {
            if (vct.at(0).key != _L_PATH) {
                if (_loopPhases == 0) {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, "_L_PATH possible not found");
                }
                break;
            }
            _VarHeader _U;
            _U.Addr = vct.at(0).addr;
            _loopPhases++;
            if (vct.size() > 1) {
                if (vct.at(1).key == _L_DECLR) {
                    _U.howIsUsed.push_back(LuaInteger);
                }
            }
            scope->vars[vct.at(0).addr->getHeaderVarString()] = _U;
        }
    }
    return new_;
}

void _UPI000_PreProcInstances(std::vector<LuaLexFrame> *vct, const std::string vName, uint8_t type) {
    uint32_t pos = 0;
    LuaLexFrame *frm = nullptr;
    while (true) {
        try {
            frm = &vct->at(pos);
        } catch (std::out_of_range &e) {
            break;
        }
        switch (frm->key) {
            case _L_PATH: {
                if (frm->addr->getHeaderVarString() == vName) {
                    frm->LuaTYPE = type;
                }
                break;
            }
            case _L_CALL: {
                // Address.
                if (!frm->addr->needToResolveAddr()) {
                    _UPI000_PreProcInstances(frm->addr->getData(), vName, type);
                }
                // Data.
                uint32_t zpos = 0;
                std::vector<LuaLexFrame> *cache = nullptr;
                while (true) {
                    try {
                        cache = &frm->EXPR.at(zpos);
                    } catch (std::out_of_range &e) {
                        break;
                    }
                    _UPI000_PreProcInstances(cache, vName, type);
                    zpos++;
                }
                break;
            }
            default: {
                break;
            }
        }
        pos++;
    }
}

void _UPDATE_INSTANCES_000(std::vector<LuaLexFrame> *pProcKeys, _SCOPE_EMU *scp) {
    std::unordered_map<std::string, _VarHeader> *contents = &scp->vars;
    for (auto it = contents->begin(); it != contents->end(); it++) {
        LuaType final__ = getOverallType(it->second.howIsUsed);
        it->second.FinalType = final__;
        
        // As we go uhh.. we should look at every variable which represents that instances.
        LuaLexFrame *LLF = nullptr;
        uint32_t pos = 0;
        uint64_t scpId = 0;
        while (true) {
            try {
                LLF = &pProcKeys->at(pos);
            } catch (std::out_of_range &e) {
                break;
            }
            if (LLF->key == _L_BlockStart) {
                scpId++;
                continue;
            }
            if (scpId == scp->_scopeID) {
                switch (LLF->key) {
                    case _L_DECLR_PLUS_DATA: {
                        if (LLF->local && !LLF->multipleway) {
                            if (LLF->addr->getHeaderVarString() == it->first) {
                                LLF->LuaTYPE = static_cast<uint8_t>(final__);
                            }
                        }
                        // Data.
                        uint8_t t = static_cast<uint8_t>(final__);
                        uint32_t zpos = 0;
                        std::vector<LuaLexFrame> *cache = nullptr;
                        while (true) {
                            try {
                                cache = &LLF->EXPR.at(zpos);
                            } catch (std::out_of_range &e) {
                                break;
                            }
                            _UPI000_PreProcInstances(cache, it->first, t);
                            zpos++;
                        }
                        break;
                    }
                    case _L_CALL:  {
                        uint8_t t = static_cast<uint8_t>(final__);
                        // Address.
                        if (!LLF->addr->needToResolveAddr()) {
                            _UPI000_PreProcInstances(LLF->addr->getData(), it->first, t);
                        }
                        // Data.
                        uint32_t zpos = 0;
                        std::vector<LuaLexFrame> *cache = nullptr;
                        while (true) {
                            try {
                                cache = &LLF->EXPR.at(zpos);
                            } catch (std::out_of_range &e) {
                                break;
                            }
                            _UPI000_PreProcInstances(cache, it->first, t);
                            zpos++;
                        }
                        break;
                    }
                    default: {
                        break;
                    }
                }
            }
            pos++;
        }
        if (scp->_toCheckAvailable) {
            if (scp->LexHeader != nullptr) {
                // if nullptr then it are ignored by a flag
                std::vector<__lua_blk_ver_frmPlusType> *onLex = &scp->LexHeader->_TOVERIFYBLOCK_;
                onLex->push_back(__lua_blk_ver_frmPlusType{it->second.Addr, final__});
            }
        }
    }
}

void _UPDATE_INSTANCES_001(std::vector<LuaLexFrame> *ppK, _SCOPE_EMU *SCOPE) {
    _UPDATE_INSTANCES_000(ppK, SCOPE);
    _SCOPE_EMU *mInstance = SCOPE;
    _SCOPE_EMU *c_cInstance = SCOPE;
    _SCOPE_EMU *nInstance = SCOPE;
    for (_SCOPE_EMU *scsr: nInstance->succesors) {
        // What do you mind?
        _UPDATE_INSTANCES_000(ppK,scsr);
        // re-run this part but with the other instances.
        for (_SCOPE_EMU *sc_2_0: scsr->succesors) {
            _UPDATE_INSTANCES_001(ppK, sc_2_0);
        }
    }
}



/*
 * Updates the variables by verifying their usage.
 * */
std::vector<LuaLexFrame> analizeNupdateConstantsNvars(std::vector<LuaLexFrame> *Keys) {
    std::vector<LuaLexFrame> NOW;
    LuaLexFrame AF;
    LuaLexFrame NW;
    LuaLexFrame LF = LuaLexFrame(_L_NONE);
    _Lua_Lex_Keys OP = _L_NONE;
    double _LE_ = 0.0f;
    std::string _STRTOCONCAT;
    uint32_t pos = 0;
    bool IGNTYPE = false;
    bool VARHEADER = false;
    bool FUNC_DEF = false;
    bool declr = false;
    LuaLexFrame *VarHeaderLLF = nullptr;
    LuaLexFrame *TblVarHeader = nullptr;
    //std::vector<LuaLexFrame> accu;
    std::unordered_map<std::string, _VarHeader> _VARS;
    bool predict_ = false;
    _SCOPE_EMU *nowScope = new _SCOPE_EMU();
    _SCOPE_EMU *startScope = nowScope;
    nowScope->mainScope = true;
    nowScope->_scopeID = 0;
    
    uint8_t _0_local = false;
    uint8_t _1_Path = false;
    uint8_t _1_Decl = false;
    uint8_t _1_Func = false;
    uint8_t _1_Lcal = false;
    uint8_t _1_Aadr = false;
    uint8_t _1_ifst = false;
    uint8_t _1_fors = false;
    uint8_t _1_whil = false;
    uint8_t _1_ret0 = false;
    uint8_t _2_0001 = false;
    std::vector<LuaLexFrame> preRes;
    std::vector<LuaLexFrame> *toFocus = &preRes;
    LuaLexFrame _1_cache_0;
    LuaLexFrame _1_cache_1;
    bool _2_FUNC = false; // frame 1 start
    uint16_t scopes__ = 0;
    uint16_t scopeAtFunc__ = UINT16_MAX;
    _Lua_Lex_Keys _latestKey = _L_NONE;
    std::string _s;
    bool _DBG_skipFirst = true;
    bool _DBG_skipSecond = true;
    bool _DBG_skipThird = true;
    
    // Check _L_FLAG_CONTINUE_FRAME2 flag
    if (Keys->at(Keys->size()-1).key == _L_FLAG_CONTINUE_FRAME2) {
        _2_0001 = true;
        goto _frame2;
    }
    
    while (true) {
        try {
            AF = Keys->at(pos);
        } catch (std::out_of_range &e) {
            break;
        }
        rerun_:
        switch (AF.key) {
            case _L_LOCAL: {
                _0_local = true; // Local shout not be a key in next steps.
                break;
            }
            case _L_FUNCTION: {
                AF.local = _0_local;
                NOW.push_back(AF);
                _0_local = false;
                break;
            }
            case _L_VARNAME: {
                NOW.push_back(makeSinglePath(Keys, &pos));
                (&NOW.at(NOW.size()-1))->local = _0_local;
                _0_local = false;
                pos--;
                break;
            }
            case _L_F_ARGS_START: {
                pos++;
                LuaLexFrame _SDATA = makeSingleExprP(Keys, &pos);
                if (AF.local) {
                    // This are a multiple declaration method.
                    _SDATA.key = _L_PATH;
                    _SDATA.multipleway = true;
                }
                NOW.push_back(_SDATA);
                _0_local = false;
                break;
            }
            case _L_ON_TO_GO: {
                if (AF.ATTRIB) {
                    NOW.push_back(makeSingleExprB(Keys, &pos));
                } else {
                    NOW.push_back(AF);
                }
                _0_local = false;
                break;
            }
            case _L_NEWLINE: {
                break;
            }
            case _L_SEPARATOR: {
                break;
            }
            case _L_F_ARGS_END: {
                break;
            }
            default: {
                _0_local = false;
                NOW.push_back(AF);
            }
        }
        pos++;
    }
    
    pos = 0;
    while (true) {
        try {
            AF = NOW.at(pos);
        } catch (std::out_of_range &e) {
            break;
        }
        switch (AF.key) {
            case _L_TABLE_END: {
                break; // A pot.
            }
            case _L_TABLE_START: {
                m_LuaErrorHandler->reportError(_lua_es_Illegal, 0, "Missed variable to store table?");
                m_LuaErrorHandler->setFatal(true);
                break;
            }
            case _L_BlockStart: {
                // Hoo lee sheet.
                if (_2_FUNC) {
                    scopes__++;
                }
                break;
            }
            case _L_BlockEnd: {
                if (scopes__ != 0)
                    scopes__--;
                if (scopes__ == scopeAtFunc__) {
                    _2_FUNC = false;
                    toFocus->push_back(LuaLexFrame(_L_FLAG_CONTINUE_FRAME2));
                    toFocus = &preRes;
                    //toFocus->push_back(AF);
                    scopeAtFunc__ = UINT16_MAX;
                } else {
                    goto _BlockEndStagePush;
                }
                break;
            }
            case _L_FOR: {
                scopes__++;
                if (AF.ATTRIB == 1) { // Already populated. Proceed.
                    goto _BlockEndStagePush;
                }
                _1_fors++;
                _latestKey = _L_FOR;
                break;
            }
            case _L_IF: {
                scopes__++;
                _1_ifst = true;
                break;
            }
            case _L_WHILE: {
                _1_whil++;
                scopes__++;
                _latestKey = _L_WHILE;
                break;
            }
            case _L_PATH: { // On local declarations this shouldn't exist.
                _1_Path = true;
                _1_cache_0 = AF;
                break;
            }
            case _L_FUNCTION: {
                _1_Func = true;
                _1_Lcal = AF.local;
                break;
            }
            case _L_DECLR: {
                _1_Decl = true;
                break;
            }
            case _L_RETURN: {
                if (AF.skipcheck) {
                    toFocus->push_back(AF);
                    break;
                }
                _1_ret0++;
                break;
            }
            case _L_EXPRESSION: {
                if (_1_ret0) {
                    LuaLexFrame _F(_L_RETURN);
                    if (AF.EXPR.size())
                        _F.EXPR_BRKT = std::move(computeExpression(AF.EXPR, nowScope, nullptr, false).at(0));
                    _F.ATTRIB = AF.EXPR.size() ? 0 : 1;
                    _F.skipcheck = true; // Huh
                    toFocus->push_back(_F);
                    _1_ret0--;
                    _latestKey = _L_NONE;
                    break;
                }
                if (_1_whil && _latestKey == _L_WHILE) {
                    LuaLexFrame _F(_L_WHILE);
                    _F.EXPR = std::move(computeExpression(AF.EXPR, nowScope, nullptr, true));
                    toFocus->push_back(_F);
                    _1_whil--;
                    _latestKey = _L_NONE;
                    break;
                }
                if (_1_fors && _latestKey == _L_FOR) {
                    LuaLexFrame _F(_L_FOR);
                    _F.EXPR = std::move(computeExpression(AF.EXPR, nowScope, nullptr, true));
                    toFocus->push_back(_F);
                    _F.ATTRIB = 1; // Tell other elements this is populated.
                    _latestKey = _L_NONE;
                    _1_fors--;
                    break;
                }
                if (_1_Decl && _1_Path) {
                    if (_1_cache_0.multipleway) {
                        LuaLexFrame _DECLR(_L_DECLR_PLUS_DATA);
                        if (_1_cache_0.EXPR.size() == 1)
                            _DECLR.addr = _1_cache_0.EXPR.at(0).at(0).addr;
                        else {
                            // Occupy EXPR_BRKT for this.
                            for (std::vector<LuaLexFrame> &VTX: _1_cache_0.EXPR) {
                                if (VTX.size() == 0) {
                                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("No varnames found for multiple declaration!"));
                                    m_LuaErrorHandler->setFatal(true);
                                    return std::vector<LuaLexFrame>{_L_NOP};
                                }
                                if (VTX.at(0).key != _L_PATH) {
                                    m_LuaErrorHandler->reportError(_lua_es_InvalidUsage, 0, std::string("On multiple declaration: Not an _L_PATH/_L_VARNAME object."));
                                    m_LuaErrorHandler->setFatal(true);
                                }
                                _DECLR.EXPR_BRKT.push_back(VTX.at(0));
                            }
                            _DECLR.multipleway = true;
                        }
                        _DECLR.EXPR = computeExpression(AF.EXPR, nowScope);
                        _DECLR.local = true;
                        toFocus->push_back(_DECLR);
                        _1_Path = false;
                        _1_Decl = false;
                        break;
                    }
                    if (!_1_Func) {
                        LuaLexFrame _DECLR(_L_DECLR_PLUS_DATA);
                        _DECLR.addr = _1_cache_0.addr;
                        _DECLR.local = false;
                        _DECLR.EXPR = computeExpression(AF.EXPR, nowScope);
                        
                        // SHOULD ROUND DATA.
                        
                        toFocus->push_back(_DECLR);
                    } else {
                        // Function declaration.
                        LuaLexFrame _FC(_L_FUNCTION);
                        _FC.ATTRIB = 1;
                        _FC.addr = _1_cache_0.addr;
                        _FC.EXPR = AF.EXPR;
                        _FC.local = _1_cache_0.local;
                        toFocus->push_back(_FC);
                        _1_Func = false;
                        _1_Path = false;
                        _1_Lcal = false;
                        _1_Decl = false;
                        _1_Aadr = false;
                        _2_FUNC = true;
                        scopeAtFunc__ = scopes__;
                        scopes__++;
                    }
                    _1_Path = false;
                    _1_Func = false;
                    _1_Decl = false;
                } else if (_1_Func && _1_Path) {
                    LuaLexFrame _FC(_L_FUNCTION);
                    _FC.ATTRIB = 2;
                    _FC.addr = _1_cache_0.addr;
                    _FC.EXPR = AF.EXPR;
                    _FC.local = _1_Lcal;
                    toFocus->push_back(_FC);
                    _1_Func = false;
                    _1_Path = false;
                    _1_Lcal = false;
                    _1_Decl = false;
                    _1_Aadr = false;
                    _2_FUNC = true;
                    toFocus = &toFocus->at(toFocus->size()-1).EXPR_BRKT;
                    scopeAtFunc__ = scopes__;
                    scopes__++;
                } else if (_1_Path || _1_Aadr) {
                    // Func call.
                    LuaLexFrame _FNCL(_L_CALL);
                    _FNCL.addr = _1_Path ? _1_cache_0.addr : nullptr;
                    _FNCL.ATTRIB = _1_Aadr ? 1 : 0;
                    if (_1_cache_1.EXPR.size() > 0)
                        _FNCL.EXPR_BRKT = _1_cache_1.EXPR.at(0);
                    _FNCL.EXPR = computeExpression(AF.EXPR, nowScope);
                    toFocus->push_back(_FNCL);
                    _1_Path = false;
                    _1_Aadr = false;
                } else if (_1_ifst) {
                    LuaLexFrame _IF(_L_IF);
                    _IF.EXPR = computeExpression(AF.EXPR, nowScope);
                    toFocus->push_back(_IF);
                    _1_ifst = false;
                } else {
                    //preRes.push_back(AF);
                    _1_cache_1 = AF;
                    _1_Aadr = true;
                }
                break;
            }
            default: {
                _BlockEndStage:
                if (_1_Path)
                    toFocus->push_back(_1_cache_0);
                _1_Decl = false;
                _1_Path = false;
                _BlockEndStagePush:
                toFocus->push_back(AF);
            }
        }
        pos++;
    }
    _frame2:
    if (_2_0001) {
        preRes = std::move(*Keys);
    }
    pos = 0;
    bool _2_Func = false;
    std::vector<LuaLexFrame> final_;
    //nowScope
    uint32_t *qPos = &pos;
    std::vector<LuaLexFrame> *qVector = &preRes;
    std::vector<LuaLexFrame> *rVector = &final_;
    LuaLexFrame *funcPtr = nullptr;
    std::vector<LuaLexFrame> nrVector;
    uint32_t zPos = 0;
    _reset:
    while (true) {
        try {
            AF = qVector->at(*qPos);
        } catch (std::out_of_range &e) {
            if (funcPtr != nullptr) {
                funcPtr->EXPR_BRKT = std::move(*rVector);
                rVector = &final_;
                qPos = &pos;
                pos++;
                funcPtr = nullptr;
                qVector = &preRes;
                nowScope = nowScope->predessor;
                goto _reset;
            }
            break;
        }
        switch (AF.key) {
            // Interleaving scopes.
            case _L_FUNCTION: {
                if (AF.ATTRIB > 0) {
                    _SCOPE_EMU *now = new _SCOPE_EMU();
                    _SCOPE_EMU *lScope = nowScope;
                    nowScope->succesors.push_back(now);
                    nowScope = now;
                    nowScope->_toCheckAvailable = true;
                    nowScope->predessor = lScope;
                    nowScope->_scopeID = now->predessor->_scopeID+1;
                    //Push some arguments to nowScope.
                    for (std::vector<LuaLexFrame> &a: AF.EXPR) {
                        LuaLexFrame *b = nullptr;
                        try {
                            b = &a.at(0);
                        } catch (std::out_of_range &e) {
                            continue;
                            // What ?
                        }
                        if (b != nullptr) {
                            if (b->key == _L_PATH) {
                                if (b->addr->needToResolveAddr()) {
                                    m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, std::string("For function arguments: Trying to put complete path to function arguments " + b->addr->getHeaderVarString()));
                                    goto _qEnd;
                                } else {
                                    _VarHeader _R;
                                    _R.Addr = b->addr;
                                    _R.howIsUsed = std::vector<LuaType>(LuaUnknown);
                                    _R._ASSOC_VARS.push_back(b); // Not viable.
                                    nowScope->vars.insert(std::pair<std::string, _VarHeader>(b->addr->getHeaderVarString(), _R));
                                }
                            }
                        }
                    }
                    
                }
                _qEnd:
                rVector->push_back(AF);
                funcPtr = &AF;
                qPos = &zPos;
                qVector = &AF.EXPR_BRKT;
                rVector = &nrVector;
                //pos++;
                goto _reset;
                break;
            }
            case _L_BlockStart: {
                _SCOPE_EMU *now = new _SCOPE_EMU();
                now->predessor = nowScope;
                nowScope->succesors.push_back(now);
                nowScope = now;
                nowScope->_scopeID = now->predessor->_scopeID+1;
                rVector->push_back(AF);
                break;
            }
            case _L_BlockEnd: {
                nowScope = nowScope->predessor;
                rVector->push_back(AF);
                break;
            }
            // Wiiiih, this language should be used inside Lifeys and MineStars
            case _L_DECLR_PLUS_DATA: {
                // Declaration.
                rVector->push_back(AF);
                LuaLexFrame *AF = &rVector->at(rVector->size()-1);
                if (AF->local) {
                    // Global variables should be treated as LuaUnknown values, bcoz i'm lazy to make optimized ones, also, running out of time!!
                    if (!AF->multipleway) {
                        if (nowScope->SEARCH_VAR(AF->addr->getHeaderVarString(), true) == nullptr) {
                            // Create a new slot, then check the local contents so we can eval
                            _VarHeader _U;
                            _U.Addr = AF->addr;
                            //_U.howIsUsed.push_back(__LEX_KEY_TO_LuaType(AF.addr->getHeader()->subkey, (AF.addr->getHeader()->subkey == _L_INT ? 0 : 1)));
                            //AF.addr->getHeader()->addr = AF.addr;
                            _U._ASSOC_VARS.push_back(AF);
                            nowScope->vars[AF->addr->getHeaderVarString()] = _U;
                        } else {
                            // Double?
                            m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, std::string("Double local declaration in one scope, not allowed.."));
                        }
                    } else {
                        for (LuaLexFrame &title: AF->EXPR_BRKT) {
                            if (nowScope->SEARCH_VAR(title.addr->getHeaderVarString(), true) == nullptr) {
                                // Create a new slot, then check the local contents so we can eval
                                _VarHeader _U;
                                _U.Addr = title.addr;
                                _U._ASSOC_VARS.push_back(&title);
                                nowScope->vars[title.addr->getHeaderVarString()] = _U;
                            } else {
                                // Double?
                                m_LuaErrorHandler->reportError(_lua_es_UnknownErr, 0, std::string("Double local declaration in one scope, not allowed.."));
                            }
                        }
                    }
                }
                // Look at addr struct of both sides.
                // Declr side
                uint16_t _pos = 0;
                //if (!AF.local) {
                std::vector<LuaLexFrame> *_CACHE;
                while (true) {
                    try {
                        _CACHE = &AF->EXPR.at(_pos);
                    } catch (std::out_of_range &e) {
                        break;
                    }
                    std::vector<LuaLexFrame> simplified = simplifyExpression(_CACHE, nowScope);
                    AF->EXPR.at(_pos) = simplified;
                    // Link. [one mode.]
                    if (!AF->multipleway)
                        nowScope->sSearchNaddVal(AF, (LuaType)simplified.at(simplified.size()-1).ATTRIB);
                    _pos++;
                }
                //}
                /*while (true) {
                    try {
                        _AUX = &_Q.at(_spos);
                    } catch (std::out_of_range &e) {
                        break;
                    }
                    while (true) {
                        try {
                            _c = &_AUX->at(_pos);
                        } catch (std::out_of_range &e) {
                            break;
                        }
                        if (_c->key == _L_EXPRESSION) {
                            // Just the first part, the secondary part FORGET IT.
                            std::vector<LuaLexFrame> _CACHE = simplifyExpression(&_c->EXPR.at(0), nowScope);
                            _c->EXPR.at(0) = _CACHE;
                        } else if (_c->key == _L_EXPRESSION_BRKT) {
                            std::vector<LuaLexFrame> simplified = simplifyExpression(&_c->EXPR_BRKT, nowScope);
                            _c->EXPR_BRKT = simplified;
                        }
                        _pos++;
                    }
                    _spos++;
                }*/
                
                
                break;
            }
            case _L_CALL: {
                // If this are a main scope, just get the function's address than calculating their address manually in the code.
                // Simplify their arguments.
                
                std::vector<LuaLexFrame> *_AUX_NOW;
                std::vector<std::vector<LuaLexFrame>> nExpr;
                uint16_t _spos = 0;
                uint16_t pos = 0;
                while (true) {
                    try {
                        _AUX_NOW = &AF.EXPR.at(_spos);
                    } catch (std::out_of_range &e) {
                        break;
                    }
                    std::vector<LuaLexFrame> res = simplifyExpression(_AUX_NOW, nowScope);
                    nExpr.push_back(res);
                    _spos++;
                }
                AF.EXPR = nExpr;
                if (nowScope->mainScope) {
                    if (AF.ATTRIB == 1) { // Is it a non addressable address? [lol]
                        AF.skipcheck = false;
                        AF.a = nullptr;
                        goto _PUSH_0000;
                    }
                    Values *ptr = getValueFromtable(AF.addr->getData());
                    if (ptr == nullptr) {
                        AF.skipcheck = false;
                        goto _PUSH_0000;
                    }
                    if ((uintptr_t)ptr != 0xF00000000000000F) { // That number is represented as NULL for it.
                        AF.a = ptr; // Skip the first 8 bytes
                        AF.skipcheck = true; // Mark it as skippable check
                    }
                } else {
                    AF.a = nullptr;
                    AF.skipcheck = false;
                }
                _PUSH_0000:
                rVector->push_back(AF);
                break;
            }
            case _L_EXPRESSION: {
                // Oops!
                //std::vector<LuaLexFrame> _F = simplifyExpression(&AF.EXPR_BRKT, nowScope);
                //AF.EXPR_BRKT = _F;
                std::vector<LuaLexFrame> *_AUX_NOW;
                uint16_t _spos = 0;
                uint16_t pos = 0;
                while (true) {
                    try {
                        _AUX_NOW = &AF.EXPR.at(_spos);
                    } catch (std::out_of_range &e) {
                        break;
                    }
                    std::vector<LuaLexFrame> res = simplifyExpression(_AUX_NOW, nowScope);
                    AF.EXPR.at(_spos) = res;
                    _spos++;
                }
                /*LuaLexFrame _RET;
                if (_returnKeyword) {
                    _RET = LuaLexFrame(_L_RETURN);
                    if (_RET.EXPR.size() >= 1)
                        _RET.EXPR_BRKT = AF.EXPR.at(0);
                    else
                        _RET.ATTRIB = 1;
                }
                rVector->push_back(_returnKeyword ? _RET : AF);
                _returnKeyword = false;*/
                break;
            }
            case _L_FOR: {
                _SCOPE_EMU *now = new _SCOPE_EMU();
                now->predessor = nowScope;
                nowScope->succesors.push_back(now);
                nowScope = now;
                nowScope->_scopeID = now->predessor->_scopeID+1;
                now->LexHeader = &AF;
                rVector->push_back(AF);
                //*qPos = *qPos + 1;
                break;
            }
            case _L_IF: {
                _SCOPE_EMU *now = new _SCOPE_EMU();
                now->predessor = nowScope;
                nowScope->succesors.push_back(now);
                nowScope = now;
                nowScope->_scopeID = now->predessor->_scopeID+1;
                now->LexHeader = &AF;
                rVector->push_back(AF);
                //*qPos = *qPos + 1;
                break;
            }
            //Flags
            case _L_FLAG_IGNORE_VARCHECK: {
                nowScope->LexHeader->_TOVERIFYBLOCK_.clear();
                nowScope->LexHeader = nullptr;
                break;
            }
            
            default: {
                if (AF.key != _L_NEWLINE)
                    rVector->push_back(AF);
            }
        }
        *qPos = *qPos + 1;
    }
    // First the main scope
    _UPDATE_INSTANCES_000(&final_, startScope);
    // Update every sigh of the variable from the scopes to exact LuaLexFrame key
    for (_SCOPE_EMU *scope: startScope->succesors) {
        _UPDATE_INSTANCES_001(&final_, scope);
    }
    return final_;

}










