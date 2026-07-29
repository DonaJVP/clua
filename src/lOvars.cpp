#include "lua.hpp"
#include <cstdint>
#include <set>
#include <deque>
#include <string>
#include <stdexcept>
#include <iostream>

// This should help to optimize the variables for each scope.

static void _H_EVAL_HOTTESTVARIABLESNPTHVAR(lua_Scope *state) {
    // Eval every variable (Which should exist a top 5) and put to HVtoCompiler.
    uint_fast64_t count = state->HottestVariables.size();
    std::cout << "Checking point: " << std::to_string(state->HottestVariables.size()) << std::endl;
    std::deque<std::string> classifiedVars;
    std::set<std::string> alreadySet;
    while (count) {
        uint32_t maxPts = 0;
        std::string cache;
        for (auto[name, countAnt]: state->HottestVariables) {
            if (alreadySet.find(name) != alreadySet.end())
                continue;
            if (countAnt >= maxPts) {
                cache = name;
                maxPts = countAnt;
            }
            alreadySet.insert(name);
            // Set it to classifiedVars.
            classifiedVars.push_front(name);
            std::cout << "VARIABLE ANOTTHED: " << name << std::endl;
        }
        count--;
    }
    // Classify those first 5
    for (int i = 0; i < 5; i++) {
        if (i < classifiedVars.size()) {
            std::cout << "STARTED, OUTPUTTING: " <<classifiedVars.at(i) << std::endl;
            state->HVtoCompiler.push_back(std::pair<std::string, uint32_t>(classifiedVars.at(i), -1));
        }
            
    }
}

static lua_Scope *_H_ADD_TO_HOTTESTVARIABLESNPROC(lua_Scope *state, const std::string vname, lua_Scope *rSTATE = nullptr) {
    if (state->HottestVariables.find(vname) != state->HottestVariables.end()) {
        state->HottestVariables.at(vname) = state->HottestVariables.at(vname)+1;
    } else {
        // Search if they are on the parent scope.
        if (state->rSCOPE != nullptr) 
            _H_ADD_TO_HOTTESTVARIABLESNPROC(state->rSCOPE, vname, rSTATE);
        else {
            rSTATE->HottestVariables.insert(std::pair<std::string, uint32_t>(vname,1));
            return rSTATE;
        }
    }
    return nullptr;
}

static void _H_SEARCHnADDTOHOTTESTVARIABLES(lua_Scope *state, lua_Expression *E) {
    for (std::vector<LuaLexFrame> &V: *E) {
        for (LuaLexFrame &F: V) {
            // Search if it are an ADDRESS or a EXPRESSION.
            if (F.key == _L_PATH) {
                std::string _vn = F.addr->getHeaderVarString();
                _H_ADD_TO_HOTTESTVARIABLESNPROC(state, _vn, state);
            } else if (F.key == _L_EXPRESSION) {
                _H_SEARCHnADDTOHOTTESTVARIABLES(state, &F.EXPR);
            }
        }
    }
}

std::vector<lua_biOpCode> *lua_Scope::updateHottestVariablesForKeys(lua_Scope *MAINSCOPE, std::vector<lua_biOpCode> *DTA) {
    lua_Scope *actualScope = MAINSCOPE;
    lua_biOpCode *opcode = nullptr;
    uint_fast64_t pos = 0;
    // Search per every variable usage.
    while (true) {
        try {
            opcode = &DTA->at(pos);
        } catch (std::out_of_range &e) {
            break;
        }
        switch (opcode->OPCODE) {
            case l_b_o_c_AND: {
                // Nothing.
                break;
            }
            case l_b_o_c_NOT: {
                // Nothing.
                break;
            }
            case l_b_o_c_OR_: {
                // Nothing.
                break;
            }
            case l_b_o_c_VTN: {
                // Deprecated.
                break;
            }
            case l_b_o_c_IFS: {
                _H_SEARCHnADDTOHOTTESTVARIABLES(actualScope, &opcode->p);
                actualScope = opcode->SCOPE;
                break;
            }
            case l_b_o_c_ELS: {
                // Nothing.
                break;
            }
            case l_b_o_c_ELI: {
                _H_SEARCHnADDTOHOTTESTVARIABLES(actualScope, &opcode->p);
                break;
            }
            case l_b_o_c_SCP: {
                // Opens a new scope.
                actualScope = opcode->SCOPE;
                break;
            }
            case l_b_o_c_SCE: {
                // Goes back to the last scope.
                _H_EVAL_HOTTESTVARIABLESNPTHVAR(actualScope);
                actualScope = actualScope->rSCOPE;
                break;
            }
            case l_b_o_c_FOR: {
                // Nothing.
                // The first two variables (If they exist..) we need to add a point.
                actualScope = opcode->SCOPE;
                if (opcode->LLF.empty())
                    break;
                for (LuaLexFrame &I:opcode->LLF) {
                    //_H_ADD_TO_HOTTESTVARIABLESNPROC(actualScope, I.addr->getHeaderVarString(), actualScope);
                    actualScope->HottestVariables.insert(std::pair<std::string, uint32_t>(I.addr->getHeaderVarString(), 100));
                }
                break;
            }
            case l_b_o_c_FEX: {
                // Nothing.
                break;
            }
            case l_b_o_c_CFN: {
                // Search their own value.
                if (opcode->fixedaddr != 0) {
                    break; // Ignore if this is an fixed address.
                }
                if (opcode->ATR != 0) {
                    break;
                }
                // Let's check their path.
                std::string _s0 = opcode->path->getHeaderVarString();
                // Okay.
                _H_ADD_TO_HOTTESTVARIABLESNPROC(actualScope, _s0, actualScope);
                break;
            }
            case l_b_o_c_DEC: {
                // Declaration, search between their expr.
                _H_SEARCHnADDTOHOTTESTVARIABLES(actualScope, &opcode->p);
                break;
            }
            case l_b_o_c_LXC: {
                // Nothing.
                break;
            }
            case l_b_o_c_SSE: {
                // Nothing.
                break;
            }
            case l_b_o_c_FUN: {
                // Nothing.
                break;
            }
            case l_b_o_c_RLE: {
                // Nothing.
                break;
            }
            case l_b_o_c_TBL: {
                // Nothing.
                break;
            }
            case l_b_o_c_MEM: {
                // Nothing.
                break;
            }
            case l_b_o_c_UPV: {
                // Nothing.
                break;
            }
            case l_b_o_c_STO: {
                _H_SEARCHnADDTOHOTTESTVARIABLES(actualScope, &opcode->p);
                break;
            }
            case l_b_o_c_EXP: {
                // Nothing.
                break;
            }
            case l_b_o_c_NUL: {
                // Nothing.
                break;
            }
            case l_b_o_c_DEP: {
                // Nothing.
                break;
            }
            case l_b_o_c_NOP: {
                // Nothing.
                break;
            }
            case l_b_o_c_STM: {
                // Nothing.
                break;
            }
            break;
        }
        pos++;
    }
    return DTA;
}
