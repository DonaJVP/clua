#include "lua.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

/*
 * 0: NOP
 * 1: Sum
 * 2: Dec
 * 3: Mul
 * 4: Div
 * 5: Exp
 * 6: Per [No.]
 */

// getExpr for fixed pos
Values *luaGetExpr(std::vector<LuaLexFrame> *Keys, _laddr_searchPlaces *P, _Lua_Lex_Keys end, uint32_t &pos, uint32_t _LINES) {
    Values *cache = nullptr;
    LuaLexFrame _cache;
    uint8_t op_typ_ = 0;
    while (true) {
        try {
            _cache = Keys->at(pos);
        } catch (std::out_of_range &e) {
            // Internal error
        }
        switch (_cache.key) {
            case _L_VARNAME: {
                std::string n = std::string(_cache._data.begin(), _cache._data.end());
                size_t hash = std::hash<std::string_view>{}(std::string_view(n.c_str(), strlen(n.c_str())));
                // Get direct addr
                Values *E = luaGetAddress(Keys, P, _LINES, pos);
                if (op_typ_ > 0) {
                    if (cache == nullptr) {
                        m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Not table: " + getLine(_LINES) + " ;; 'Table' name: -" + n));
                        m_LuaErrorHandler->setFatal(true);
                        return new Values();
                    }
                    if (cache->TYPO != LuaInteger) {
                        m_LuaErrorHandler->reportError(_lua_es_InvalidType, 0, std::string("Not a integer: " + getLine(_LINES) + " ;; 'Addr' name: -" + n));
                        m_LuaErrorHandler->setFatal(true);
                        return new Values();
                    }
                    switch (op_typ_) {
                        case 0: {
                            //Nop
                            break;
                        }
                        case 1: {
                            //Sum
                            cache->val0 = cache->val0 + E->val0;
                            break;
                        }
                        case 2: {
                            //Dec
                            cache->val0 = cache->val0 - E->val0;
                            break;
                        }
                        case 3: {
                            //Mul
                            cache->val0 = cache->val0 * E->val0;
                            break;
                        }
                        case 4: {
                            //Div
                            cache->val0 = cache->val0 / E->val0;
                            break;
                        }
                        case 5: {
                            //Exp
                            float _E = cache->val0;
                            for (int i = 0; i <= E->val0; i++) {
                                _E = _E * _E;
                            }
                            cache->val0 = _E;
                            break;
                        }
                    }
                    op_typ_ = 0;
                } else {
                    //Set statement
                    cache = E;
                    cache->__varname = n;
                }
                break;
            }
            case _L_F_ARGS_START: {
                // Messing around here...
                if (cache == nullptr) {
                    m_LuaErrorHandler->reportError(_lua_es_NonFunction, 0, std::string("Function not defined: " + getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                }
                if (cache->TYPO != LuaFunction) {
                    m_LuaErrorHandler->reportError(_lua_es_InvalidType, 0, std::string("Not a function: " + getLine(_LINES) + " ;; 'Addr' name: " + cache->__varname));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                }
                //Exec, search FuncArgs*
                FvNpos _k = getArgs(Keys, pos, P->gen, P->env, _LINES);
                pos = _k.pos;
                //Exec
                FuncArgs *_P = new FuncArgs(*cache->val4(_k.args));
                cache = &_P->data[0];
                break;
            }
            case _L_SYNTAX_SUM: {
                op_typ_ = 1;
                break;
            }
            case _L_SYNTAX_DEC: {
                op_typ_ = 2;
                break;
            }
            case _L_SYNTAX_MUL: {
                op_typ_ = 3;
                break;
            }
            case _L_SYNTAX_DIV: {
                op_typ_ = 4;
                break;
            }
            case _L_SYNTAX_EXP: {
                op_typ_ = 5;
                break;
            }
            case _L_SEPARATOR: {
                return cache;
            }
            default: {
                
            }
        }
    }
    return cache;
}
