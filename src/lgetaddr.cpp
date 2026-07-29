#include "lua.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

/*
 * _laddr_searchPlaces priority:
 * FuncArgs*: 1st
 * ENV: 2nd
 * GENERAL: 3rd
 */

/*
 * This gets ONLY a address to a Value specified in the Keys*
 * Like: Values* of Table1->Table2->Label, which we want to know the value of Label
 */
Values *luaGetAddress(std::vector<LuaLexFrame> *Keys, _laddr_searchPlaces *places, uint32_t _LINES, uint32_t &pos) {
    bool _base_set = false;
    Values *cache = nullptr;
    uint32_t _starting_point = pos;
    uint8_t attrib = 0;
    while (true) {
        LuaLexFrame _now;
        try {
            _now = Keys->at(_starting_point);
        } catch (std::out_of_range &e) {
            break;
        }
        switch (_now.key) {
            case _L_VARNAME: {
                // Name
                std::string n = std::string(_now._data.begin(), _now._data.end());
                // In case we din't set a base, see if we can set
                // Also, proceed if _L_ON_TO_GO is on _starting_point+1
                size_t hash = std::hash<std::string_view>{}(std::string_view(n.c_str(), strlen(n.c_str())));
                if (cache == nullptr) {
                    // Search on FuncArgs
                    if (places->arg != nullptr) {
                        if (places->arg->size > 0) {
                            for (int i = 0; i < places->arg->size; i++) {
                                Values N = places->arg->data[i];
                                if (N.vrn == hash) {
                                    cache = &places->arg->data[i];
                                    break;
                                }
                            }
                        }
                    }
                    // Search in other places... Gen & Env
                    Values *_v_h = places->gen->getValueAddrIgnWarn(hash);
                    if (_v_h == nullptr)
                        _v_h = places->env->getValueAddr(hash);
                    cache = _v_h;
                } else {
                    // Search this var in the cache if we found out
                    // Check if cache.
                    if (cache->TYPO != LuaTable) {
                        m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Not table: " + getLine(_LINES) + " ;; 'Table' name: " + n));
                        m_LuaErrorHandler->setFatal(true);
                        return new Values();
                    }
                    cache = cache->val3->Nodes->getValueAddr(hash);
                }
                break;
            }
            case _L_ON_TO_GO: {
                attrib = _now.ATTRIB;
                break;
            }
            case _L_STRING: {
                if (attrib != 1) {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string(getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                }
                std::string n = std::string(_now._data.begin(), _now._data.end());
                size_t hash = std::hash<std::string_view>{}(std::string_view(n.c_str(), strlen(n.c_str())));
                if (cache == nullptr) {
                    m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Keyword not available: " + getLine(_LINES) + " ;; 'Addr' name: " + n));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                } else {
                    // Search this var in the cache if we found out
                    // Check if cache.
                    if (cache->TYPO != LuaTable) {
                        m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Not table: " + getLine(_LINES) + " ;; 'Table' name: " + n));
                        m_LuaErrorHandler->setFatal(true);
                        return new Values();
                    }
                    cache = cache->val3->Nodes->getValueAddr(hash);
                }
                break;
                    
            }
            case _L_NUMBER: {
                if (attrib != 1) {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string(getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                }
                float num = std::stof(std::string(_now._data.begin(), _now._data.end()));
                if (cache == nullptr) {
                    m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Keyword not available: " + getLine(_LINES) + " ;; 'Addr' name: " + std::to_string(num)));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                } else {
                    // Search this var in the cache if we found out
                    // Check if cache.
                    if (cache->TYPO != LuaTable) {
                        m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Not table: " + getLine(_LINES) + " ;; 'Table' name: " + std::to_string(num)));
                        m_LuaErrorHandler->setFatal(true);
                        return new Values();
                    }
                    cache = cache->val3->Nodes->getRawAddrValueIDX(num);
                }
                break;
            }
            default: {
                pos = _starting_point;
                return cache;
            }
        }
        _starting_point++;
    }
    return cache;
}

Values *luaGetAddress(std::vector<LuaLexFrame> *Keys, _laddr_searchPlaces *places, uint32_t _LINES) {
    bool _base_set = false;
    Values *cache = nullptr;
    uint32_t _starting_point = 0;
    uint8_t attrib = 0;
    while (true) {
        LuaLexFrame _now;
        try {
            _now = Keys->at(_starting_point);
        } catch (std::out_of_range &e) {
            break;
        }
        switch (_now.key) {
            case _L_VARNAME: {
                // Name
                std::string n = std::string(_now._data.begin(), _now._data.end());
                // In case we din't set a base, see if we can set
                // Also, proceed if _L_ON_TO_GO is on _starting_point+1
                size_t hash = std::hash<std::string_view>{}(std::string_view(n.c_str(), strlen(n.c_str())));
                if (cache == nullptr) {
                    // Search on FuncArgs
                    if (places->arg != nullptr) {
                        if (places->arg->size > 0) {
                            for (int i = 0; i < places->arg->size; i++) {
                                Values N = places->arg->data[i];
                                if (N.vrn == hash) {
                                    cache = &places->arg->data[i];
                                    break;
                                }
                            }
                        }
                    }
                    // Search in other places... Gen & Env
                    Values *_v_h = places->gen->getValueAddrIgnWarn(hash);
                    if (_v_h == nullptr)
                        _v_h = places->env->getValueAddr(hash);
                    cache = _v_h;
                } else {
                    // Search this var in the cache if we found out
                    // Check if cache.
                    if (cache->TYPO != LuaTable) {
                        m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Not table: " + getLine(_LINES) + " ;; 'Table' name: " + n));
                        m_LuaErrorHandler->setFatal(true);
                        return new Values();
                    }
                    cache = cache->val3->Nodes->getValueAddr(hash);
                }
                break;
            }
            case _L_ON_TO_GO: {
                attrib = _now.ATTRIB;
                break;
            }
            case _L_STRING: {
                if (attrib != 1) {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string(getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                }
                std::string n = std::string(_now._data.begin(), _now._data.end());
                size_t hash = std::hash<std::string_view>{}(std::string_view(n.c_str(), strlen(n.c_str())));
                if (cache == nullptr) {
                    m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Keyword not available: " + getLine(_LINES) + " ;; 'Addr' name: " + n));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                } else {
                    // Search this var in the cache if we found out
                    // Check if cache.
                    if (cache->TYPO != LuaTable) {
                        m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Not table: " + getLine(_LINES) + " ;; 'Table' name: " + n));
                        m_LuaErrorHandler->setFatal(true);
                        return new Values();
                    }
                    cache = cache->val3->Nodes->getValueAddr(hash);
                }
                break;
                
            }
            case _L_NUMBER: {
                if (attrib != 1) {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string(getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                }
                float num = std::stof(std::string(_now._data.begin(), _now._data.end()));
                if (cache == nullptr) {
                    m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Keyword not available: " + getLine(_LINES) + " ;; 'Addr' name: " + std::to_string(num)));
                    m_LuaErrorHandler->setFatal(true);
                    return new Values();
                } else {
                    // Search this var in the cache if we found out
                    // Check if cache.
                    if (cache->TYPO != LuaTable) {
                        m_LuaErrorHandler->reportError(_lua_es_NotTable, 0, std::string("Not table: " + getLine(_LINES) + " ;; 'Table' name: " + std::to_string(num)));
                        m_LuaErrorHandler->setFatal(true);
                        return new Values();
                    }
                    cache = cache->val3->Nodes->getRawAddrValueIDX(num);
                }
                break;
            }
            default: {
                return cache;
            }
        }
        _starting_point++;
    }
    return cache;
}

