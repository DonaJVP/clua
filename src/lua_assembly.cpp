#include "lua.hpp"
#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>
#include <cstdint>
#include <cstring>
#include <linux/limits.h>
#include <iostream>
#include <algorithm>

// Here the magic occurs




std::string getLine(uint32_t line, uint32_t _FI) {
    return std::string(sizeof(char), static_cast<char>(line)) +  __LINES[(_FI != 0 ? _FI : luaCurrentFileId)][line];
}


typedef int (*SimpleFunc)();
using namespace asmjit;
using namespace asmjit::x86;

//BEGIN OPTIMIZE
// Should be used to optimize the usage of variables in a script, so we can save RAM
enum luaLatestRegUsageType {
    _L_R_FUNCARGS = 0,
    _L_R_LOCAL = 0,
    _L_R_DECLR = 0,
};
struct luaLatestRegisterDriv {
    x86::Gp reg;
    luaLatestRegUsageType where;
};
//END OPTIMIZE

enum _T_ASM {
    _MEM = 0,
    _GP = 1,
};

struct _asm_h {
    x86::Gp GP;
    x86::Mem MEM;
    _T_ASM T;
};

std::vector<LuaLexFrame> LuaLex::ParserThirdStage(std::vector<LuaLexFrame> keys) {
    // Transform some strings to TSTRING
    // Don't verify shit.
    std::vector<LuaLexFrame> FRAMES_NEW;
    std::unordered_map<std::string, LuaType*> varMap;
    uint64_t i = 0;
    while (true) {
        LuaLexFrame FRM;
        try {
            FRM = keys.at(i);
        } catch (std::out_of_range &e) {
            // Stop.
            return FRAMES_NEW;
        }
        switch (FRM.key) {
            case _L_VARNAME: {
                // Transform into TSTRING
                std::string data = std::string(FRM._data.begin(), FRM._data.end());
                FRM.a = returnCompiledString(data); // Already compiled.
                FRM.keystring = data;
                FRAMES_NEW.push_back(FRM);
                break;
            }
            case _L_STRING: {
                // Transform into TSTRING
                std::string data = std::string(FRM._data.begin(), FRM._data.end());
                FRM.a = returnCompiledString(data); // Already compiled.
                FRM.keystring = data;
                FRAMES_NEW.push_back(FRM);
                break;
            }
            default: {
                FRAMES_NEW.push_back(FRM);
                break;
            }
        }
        i++;
    }
}





/*
void LuaLex::ParserThirdStage(std::vector<LuaLexFrame> keys) {
    
    if (keys.size() < 2) { //Not usable.
        m_LuaErrorHandler->reportError(_lua_es_TooSmallEntry, 0, "");
        m_LuaErrorHandler->setFatal(true);
        return;
    }
    
    // Let's see how we can make order into the keys... And transform into assembly code.
    Script SCR = Script();
    uint32_t size = keys.size();
    uint32_t _pos = 0;
    _Lua_Lex_Keys _LAST_KEY = _L_NIL;
    uint32_t _LINES = 0;
    uint16_t _END_CASES = 0;
    CacheAttributes ATR = CacheAttributes();
    TypeData TD;

    // ASMJIT
    JitRuntime rt;
    CodeHolder code;
    CodeHolder code2;
    code.init(Environment::host());
    code2.init(Environment::host());
    FunctionPointer FUNCTION;
    x86::Assembler F_PRIM(&code);
    x86::Assembler F_SCR(&code2);
    Section* dataPRIM;
    Section* dataSCRP;
    //Error errPRM = code.new_section(Out(dataPRIM), ".data", SIZE_MAX, asmjit::SectionFlags::kNone);
    //Error errSCR = code2.new_section(Out(dataSCRP), ".data", SIZE_MAX, asmjit::SectionFlags::kNone);
    FuncNode *PRIM;
    FuncNode *SCRP;
    
    //std::unordered_map<size_t, x86::Gp> LocalVariables;
    //std::vector<std::unordered_map<size_t, _asm_h>> LocalVariables; // Leveled, because of ATR.TOENCLOSE
    std::vector<size_t> FunctionArguments;
    std::vector<std::unordered_map<size_t, uint64_t*>> LocalVariables;
    std::vector<std::vector<LuaLexFrame>> _FuncArgs;
    std::vector<FuncArgs*> _FuncRetArgs; 
    uint16_t _FuncArgsLevel = 0;
    std::vector<LuaLexFrame> _TableSeek;
    LuaTableBase *Table = nullptr;
    LuaTableBase *ENV = new LuaTableBase(m_Lua);
    bool table_Form = false;
    Values *VAR = nullptr;
    luaLatestRegisterDriv lreg;
    
    FuncSignature sig = FuncSignature::build<void>();
    
    FuncArgs *ret = nullptr;
    
    FvNpos _AG;

    
    // Manual Assembly.
    //char ASM[] = { 0x55 };
    
    while (_pos < size) {
        LuaLexFrame _FRAME = keys[_pos];
        switch (_FRAME.key) {
            // See if it are a VARNAME [LOCAL]
            // ATTRIBUTES
            case _L_LOCAL: {
                if (keys[_pos-1].key != _L_NEWLINE || keys[_pos-1].key != _L_SEPARATOR) { // Local must be always in position.
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad usage of 'local' in line: " + getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return;
                }
                ATR.LOCAL = true;
                break;
            }
            // MODELS
            case _L_FUNCTION: {
                // Check if types are present before this.
                if (ATR.TYPES) {
                    m_LuaErrorHandler->reportError(_lua_es_NotCorrect, 0, std::string("Bad usage of 'function' in line: " + getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return;
                }
                // Let's get all function expressions
                ATR.TOENCLOSE++;
                ATR.FUNC = true;
                // Build a new space for unordered_map
                LocalVariables.push_back(std::unordered_map<size_t, uint64_t*>());
                // Initialize arguments...
                lreg.reg = x86::rdi; 
                lreg.where = _L_R_FUNCARGS;
                // Save Args variable
                F_PRIM.mov(x86::rax, (uint64_t)&ret);
                F_PRIM.mov(x86::ptr(x86::rax), x86::rdi);
                // At seek
                F_PRIM.mov(x86::rax, x86::ptr(x86::rdi)); // data
                F_PRIM.mov(x86::r9, x86::ptr(x86::rdi, 8)); // size
                // Let's seek at function arguments.
                uint32_t pos = _pos;
                uint32_t args = 0;
                while (true) {
                    LuaLexFrame FRM = keys[pos];
                    switch (FRM.key) {
                        case _L_VARNAME: {
                            args++;
                            //Compress, and port it to FunctionArguments
                            std::string name = std::string(FRM._data.begin(), FRM._data.end());
                            size_t hashed_id = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            // Build up a assembler section for FuncArgs->data[i].size_t = hashed_id
                            // Just to write THERE
                            // Seek
                            // WARNING: Must check to don't go outbounds, sometimes, the args aren't fully populated
                            label _con = F_PRIM.new_label();
                            label _end = F_PRIM.new_label();
                            F_PRIM.cmp(x86::r9, args); // args > size then get out.
                            F_PRIM.jb(_end);
                            
                            F_PRIM.bind(_con);
                            F_PRIM.mov(x86::rbx, sizeof(Values));   // -> size
                            F_PRIM.mul(x86::rbx, args);            // args * sizeof(Values)
                            F_PRIM.add(x86::rbx, x86::rax);         // data + rbx
                            // Write
                            F_PRIM.mov(x86::r8, hashed_id);
                            F_PRIM.mov(x86::ptr(x86::rbx, 0x65), x86::r8); //FuncArgs->data[i].size_t = hashed_id
                            F_PRIM.bind(_end);
                            break;
                        }
                        case _L_SEPARATOR: {
                            // huh
                            break;
                        }
                        default: {
                            m_LuaErrorHandler->reportError(_lua_es_ArgIsProblem, 0, std::string("Function arguments: " + getLine(_LINES)));
                            m_LuaErrorHandler->setFatal(true);
                            return;
                        }
                    }
                }
                break;
            }
            // OTHER
            case _L_VARNAME: { // local <varname>
                if (ATR.FUNCARGS) {
                    LuaLexFrame FRM = _FRAME;
                    if (keys[_pos+1].key == _L_F_ARGS_START)
                        FRM.ATTRIB = 1; 
                    _FuncArgs[_FuncArgsLevel].push_back(FRM); // Ignore everything.
                    break;
                }
                // Let's see if it are a chain of FULL tables
                if (keys[_pos+1].key == _L_ON_TO_GO && !ATR.FUNC) {
                    // Rather to use another tool.
                    ValNpos V_ = returnSingleFrame(&keys, _pos, m_Lua->m_General, ENV, _LINES, false); // errchech0
                    VAR = V_.val;
                    _pos = V_.pos;
                    std::string _NAME = std::string(_FRAME._data.begin(), _FRAME._data.end());
                    ATR.VARNAME_STR = _NAME;
                    break;
                }
                // Here we might close this shit, or else continue with other things.....
                if ((keys[_pos+1].key != _L_DECLR) && (keys[_pos+1].key != _L_F_ARGS_START)) { // local var;
                    // Must be only the var...
                    if (keys[_pos+1].key != _L_NEWLINE) {
                        m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad syntax in line: " + getLine(_LINES)));
                        m_LuaErrorHandler->setFatal(true);
                        return;
                    }
                    if (ATR.LOCAL) { // Likely: local var;
                        if (!ATR.FUNC) {
                            LuaLexFrame _LOCAL;
                            _LOCAL.key = _L_LOCAL;
                            LuaLexFrame _VAR;
                            _VAR.key = _L_VARNAME;
                            _VAR._data = _FRAME._data;
                            std::vector<LuaLexFrame> _V;
                            _V.push_back(_LOCAL);
                            _V.push_back(_VAR);
                            SCR.Variables.push_back(_V);
                        } else {
                            // Maybe a variable in the touchstone.
                        }
                        break;
                    } else {
                        m_LuaErrorHandler->reportError(_lua_es_NotCorrect, 0, std::string("Bad usage of 'local' in line: " + getLine(_LINES)));
                        m_LuaErrorHandler->setFatal(true); // <var>
                        return;
                    }
                } else if (keys[_pos+1].key == _L_F_ARGS_START) { // ... func ()
                    // I have a little sense which this will be useless, use the standart orders which the function arguments did drop.
                    Values *to_save = VAR;
                    _pos++;
                    ValNpos V_ = returnSingleFrame(&keys, _pos, m_Lua->m_General, ENV, _LINES, true);
                    VAR = V_.val;
                    _pos = V_.pos;
                    _AG = getArgs(&keys, _pos, m_Lua->m_General, ENV, _LINES);
                    _pos = _AG.pos;
                    FuncArgs *Args = _AG.args;
                    // Conclusivity.
                    if (VAR->TYPO != LuaFunction) {
                        m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad call in line [Not a function: " + getLuaTypeString(VAR->TYPO) + "]: " + getLine(_LINES)));
                        m_LuaErrorHandler->setFatal(true);
                        return;
                    } else {
                        FUNCTION = VAR->val4;
                    }
                    if (ATR.FUNC) {
                        // Prepare function arguments...
                        // Get function arguments, with FuncArgs* (Specified by RDI) [Live reading]
                        //BEGIN OPTIMIZE001
                        if (lreg.where == _L_R_FUNCARGS) { // May need to optimize to use less ram, um.. Let's check if _L_FUNCTION was initialized later...
                            F_PRIM.mov(x86::rcx, lreg.reg); //Fifth argument
                        } else {
                            F_PRIM.mov(x86::rsi, (uint64_t)&ret);
                            F_PRIM.mov(x86::rcx, x86::ptr(x86::rsi));
                        }
                        //END OPTIMIZE001
                        // Build up the function address...
                        // This must look like this: ...<THIS VARNAME>( [Path to the function]
                        // Build a vector.
                        std::vector<LuaLexFrame> *_K_P = new std::vector<LuaLexFrame>();
                        uint32_t pos = _pos-1; // _L_VARNAME
                        while (true) {
                            LuaLexFrame FRM = keys[pos];
                            switch (FRM.key) {
                                // Go reverse until _L_START; _L_NEWLINE; _L_DECLR; _L_SEPARATOR
                                case _L_START: {
                                    _K_P->push_back(FRM);
                                    goto _C_0_1;
                                }
                                case _L_NEWLINE: {
                                    _K_P->push_back(FRM);
                                    goto _C_0_1;
                                }
                                case _L_DECLR: {
                                    _K_P->push_back(FRM);
                                    goto _C_0_1;
                                }
                                case _L_SEPARATOR: {
                                    _K_P->push_back(FRM);
                                    goto _C_0_1;
                                }
                                default: {
                                    _K_P->push_back(FRM);
                                    break;
                                }
                            }
                            pos--;
                        }
                        _C_0_1:
                        std::reverse(_K_P->begin(), _K_P->end());
                        // Push function on to the stack.
                        // Search func reference with returnSingleFrame2 [Just for assembly]
                        
                        // Second <Arguments>, get all vector struct
                        pos = _pos-1;
                        uint16_t parent_to_enclose_ = 0;
                        std::vector<LuaLexFrame> *nKeys = new std::vector<LuaLexFrame>();
                        while (true) {
                            LuaLexFrame FRM = keys[pos];
                            switch (FRM.key) {
                                case _L_F_ARGS_START: {
                                    nKeys->push_back(FRM);
                                    parent_to_enclose_++;
                                    break;
                                }
                                case _L_F_ARGS_END: {
                                    nKeys->push_back(FRM);
                                    parent_to_enclose_--;
                                    if (parent_to_enclose_ == 0) {
                                        goto _1_1_END;
                                    }
                                    break;
                                }
                                case _L_NEWLINE: {
                                    break; //Don't add this
                                }
                                default: {
                                    nKeys->push_back(FRM);
                                    break;
                                }
                            }
                            pos++;
                        }
                        F_PRIM.mov(x86::rdi, (uint64_t)nKeys);
                        F_PRIM.mov(x86::rsi, (uint64_t)m_Lua->m_General);
                        F_PRIM.mov(x86::rdx, (uint64_t)ENV);
                        F_PRIM.mov(x86::r8, _LINES);
                        F_PRIM.call(Imm(getArgs2));
_1_1_END:
                        // Make a valid asm struct...
                        F_PRIM.mov(x86::rdi, x86::rax);
                        F_PRIM.call(Imm(FUNCTION));
                        if (ATR.DECLR) {
                            if (ATR.LOCAL) { // local variable = FUNC(FuncArgs*)
                                size_t _ADDR = std::hash<std::string_view>{}(std::string_view(ATR.VARNAME_STR.c_str(), strlen(ATR.VARNAME_STR.c_str())));
                                F_PRIM.mov(x86::rdi, (uint64_t)ENV);
                                F_PRIM.mov(x86::rsi, _ADDR);
                                F_PRIM.mov(x86::rdx, x86::rax);
                                F_PRIM.call(Imm(setValueHashed));
                            } else { // a.b.c.variable = FUNC(FuncArgs*)
                                std::vector<LuaLexFrame> *Frames = new std::vector<LuaLexFrame>();
                                uint32_t __pos = _pos-1;
                                while (true) {
                                    LuaLexFrame KEY = keys[__pos];
                                    if ((KEY.key != _L_START) && (KEY.key != _L_NEWLINE) && (KEY.key != _L_DECLR) && (KEY.key != _L_SEPARATOR)) {
                                        Frames->push_back(KEY);
                                        __pos--;
                                    } else {
                                        break;
                                    }
                                }
                                std::reverse(Frames->begin(), Frames->end());
                                uint64_t *M1 = nullptr;
                                F_PRIM.mov(x86::rdi, (uint64_t)&M1);
                                F_PRIM.mov(x86::rcx, x86::ptr(x86::rax, 0)); //Get Values*
                                F_PRIM.mov(x86::ptr(x86::rdi), x86::rcx);
                                F_PRIM.mov(x86::rdi, (uint64_t)Frames);
                                F_PRIM.mov(x86::rsi, 0);
                                F_PRIM.mov(x86::rdx, (uint64_t)m_Lua->m_General);
                                F_PRIM.mov(x86::rcx, (uint64_t)ENV);
                                F_PRIM.mov(x86::r8, _LINES);
                                F_PRIM.mov(x86::r9, true);
                                F_PRIM.call(Imm(returnSingleFrame));
                                Label _OK = F_PRIM.new_label();
                                F_PRIM.cmp(x86::rdx, 0);
                                F_PRIM.jnz(_OK);
                                F_PRIM.mov(x86::ptr(x86::rcx), x86::rdi); // rcx is a pointer to Values*
                                F_PRIM.mov(x86::rdi, _lua_es_InvalidType);
                                F_PRIM.mov(x86::rsi, luaCurrentFileId);
                                F_PRIM.mov(x86::rdx, _LINES);
                                F_PRIM.call(Imm(luaErr));
                                F_PRIM.ret();
                                F_PRIM.bind(_OK);
                                F_PRIM.mov(x86::rdi, (uint64_t)&M1);
                                F_PRIM.mov(x86::rdx, x86::ptr(x86::rdi));
                                F_PRIM.mov(x86::ptr(x86::rcx), x86::rdx); // a.b.c = Values*
                            }
                        }
                    } else {
                        if (ATR.TOENCLOSE == 0) {
                            F_SCR.mov(x86::rdi, Args);
                            F_SCR.call(Imm(FUNCTION));
                            std::vector<LuaLexFrame> *Frames = new std::vector<LuaLexFrame>();
                            uint32_t __pos = _pos-1;
                            while (true) {
                                LuaLexFrame KEY = keys[__pos];
                                if ((KEY.key != _L_START) && (KEY.key != _L_NEWLINE) && (KEY.key != _L_DECLR)) {
                                    Frames->push_back(KEY);
                                    __pos--;
                                } else {
                                    break;
                                }
                            }
                            std::reverse(Frames->begin(), Frames->end());
                            uint64_t *M1 = nullptr;
                            F_SCR.mov(x86::rdi, (uint64_t)&M1);
                            F_SCR.mov(x86::rcx, x86::ptr(x86::rax, 0)); //Get Values*
                            F_SCR.mov(x86::ptr(x86::rdi), x86::rcx);
                            F_SCR.mov(x86::rdi, (uint64_t)Frames);
                            F_SCR.mov(x86::rsi, 0);
                            F_SCR.mov(x86::rdx, (uint64_t)m_Lua->m_General);
                            F_SCR.mov(x86::rcx, (uint64_t)ENV);
                            F_SCR.mov(x86::r8, _LINES);
                            F_SCR.mov(x86::r9, true);
                            F_SCR.call(Imm(returnSingleFrame));
                            Label _OK = F_SCR.new_label();
                            F_SCR.cmp(x86::rdx, 0);
                            F_SCR.jnz(_OK);
                            F_SCR.mov(x86::ptr(x86::rcx), x86::rdi); // rcx is a pointer to Values*
                            F_SCR.mov(x86::rdi, _lua_es_InvalidType);
                            F_SCR.mov(x86::rsi, luaCurrentFileId);
                            F_SCR.mov(x86::rdx, _LINES);
                            F_SCR.call(Imm(luaErr));
                            F_SCR.ret();
                            F_SCR.bind(_OK);
                            F_SCR.mov(x86::rdi, (uint64_t)&M1);
                            F_SCR.mov(x86::rdx, x86::ptr(x86::rdi));
                            F_SCR.mov(x86::ptr(x86::rcx), x86::rdx); // a.b.c = Values*
                        }
                    }
                } else if (!ATR.LOCAL) {
                    // If not local... then.f
                    ATR.VARNAME_STR = std::string(_FRAME._data.begin(), _FRAME._data.end());
                    ATR.VARNAME = true;
                }
                break;
            }
            case _L_DECLR: {
                ATR.DECLR = true;
                break;
            }
            case _L_F_ARGS_START: {
                ATR.F_AG_ENC++;
                // Mhmhmh, explore through here...
                // Function arguments can't be proc., here because of _L_VARNAME
                // How to breakthrough this: a == b and t > 2  IS  (a==b) && (t>2)
                uint32_t pos = _pos;
                while (true) {
                    // Find & Asseemble
                    LuaLexFrame key = keys[pos];
                    switch (key.key) {
                        default: {
                            break;
                        }
                    }
                    pos++;
                }
                break;
            }
            case _L_F_ARGS_END: {
                ATR.F_AG_ENC--;
                break;
            }
            //BEGIN TYPES
            case _L_TRUE: {
                if (ATR.VARNAME) {
                    ATR.VARNAME = false;
                    if (ATR.DECLR) {
                        ATR.DECLR = false;
                        if (!ATR.LOCAL) {
                            // Assign in the _G the var.
                            lua_AddrSpec __pos;
                            std::string name = ATR.VARNAME_STR;
                            __pos.Type = _last_Hashed;
                            __pos.Addr = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            __pos.Info = _luaBase_VariableInfo(name);
                            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
                            Values *VAL = new Values();
                            VAL->TYPO = LuaBoolean;
                            VAL->__varname = name;
                            VAL->val5 = true; // THE BASE
                            m_Lua->m_General->Nodes->setValue(__pos, VAL);
                        } else {
                            lua_AddrSpec __pos;
                            std::string name = ATR.VARNAME_STR;
                            __pos.Type = _last_Hashed;
                            __pos.Addr = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            __pos.Info = _luaBase_VariableInfo(name);
                            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
                            Values *local = new Values();
                            local->TYPO = LuaBoolean;
                            local->val5 = true;
                            ENV->setValue(__pos, local);
                        }
                    } else {
                        m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad syntax in line: " + getLine(_LINES)));
                        m_LuaErrorHandler->setFatal(true);
                        return;
                    }
                }
                break;
            }
            case _L_FALSE: {
                if (ATR.VARNAME) {
                    ATR.VARNAME = false;
                    if (ATR.DECLR) {
                        ATR.DECLR = false;
                        if (!ATR.LOCAL) {
                            // Assign in the _G the var.
                            lua_AddrSpec __pos;
                            std::string name = ATR.VARNAME_STR;
                            __pos.Type = _last_Hashed;
                            __pos.Addr = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            __pos.Info = _luaBase_VariableInfo(name);
                            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
                            Values *VAL = new Values();
                            VAL->TYPO = LuaBoolean;
                            VAL->__varname = name;
                            VAL->val5 = false; // THE BASE
                            m_Lua->m_General->Nodes->setValue(__pos, VAL);
                        } else {
                            lua_AddrSpec __pos;
                            std::string name = ATR.VARNAME_STR;
                            __pos.Type = _last_Hashed;
                            __pos.Addr = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            __pos.Info = _luaBase_VariableInfo(name);
                            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
                            Values *local = new Values();
                            local->TYPO = LuaBoolean;
                            local->val5 = false;
                            ENV->setValue(__pos, local);
                        }
                    } else {
                        m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad syntax in line: " + getLine(_LINES)));
                        m_LuaErrorHandler->setFatal(true);
                        return;
                    }
                }
                break;
            }
            case _L_STRING: {
                if (ATR.VARNAME) {
                    ATR.VARNAME = false;
                    if (ATR.DECLR) {
                        ATR.DECLR = false;
                        if (!ATR.LOCAL) {
                            // Assign in the _G the var.
                            lua_AddrSpec __pos;
                            std::string name = ATR.VARNAME_STR;
                            __pos.Type = _last_Hashed;
                            __pos.Addr = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            __pos.Info = _luaBase_VariableInfo(name);
                            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
                            Values *VAL = new Values();
                            VAL->TYPO = LuaString;
                            VAL->__varname = name;
                            VAL->val1 = std::string(_FRAME._data.begin(), _FRAME._data.end()); // THE BASE
                            m_Lua->m_General->Nodes->setValue(__pos, VAL);
                        } else {
                            // Assign in the _G the var.
                            lua_AddrSpec __pos;
                            std::string name = ATR.VARNAME_STR;
                            __pos.Type = _last_Hashed;
                            __pos.Addr = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            __pos.Info = _luaBase_VariableInfo(name);
                            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
                            Values *VAL = new Values();
                            VAL->TYPO = LuaString;
                            VAL->__varname = name;
                            VAL->val1 = std::string(_FRAME._data.begin(), _FRAME._data.end()); // THE BASE
                            ENV->setValue(__pos, VAL);
                        }
                    } else {
                        m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad syntax in line: " + getLine(_LINES)));
                        m_LuaErrorHandler->setFatal(true);
                        return;
                    }
                }
                break;
            }
            case _L_NUMBER: {
                if (ATR.VARNAME) {
                    ATR.VARNAME = false;
                    if (ATR.DECLR) {
                        ATR.DECLR = false;
                        if (!ATR.LOCAL) {
                            // Assign in the _G the var.
                            lua_AddrSpec __pos;
                            std::string name = ATR.VARNAME_STR;
                            __pos.Type = _last_Hashed;
                            __pos.Addr = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            __pos.Info = _luaBase_VariableInfo(name);
                            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
                            Values *VAL = new Values();
                            VAL->TYPO = LuaInteger;
                            VAL->__varname = name;
                            VAL->val0 = std::stof(std::string(_FRAME._data.begin(), _FRAME._data.end()));
                            m_Lua->m_General->Nodes->setValue(__pos, VAL);
                        } else {
                            // Assign in the _G the var.
                            lua_AddrSpec __pos;
                            std::string name = ATR.VARNAME_STR;
                            __pos.Type = _last_Hashed;
                            __pos.Addr = std::hash<std::string_view>{}(std::string_view(name.c_str(), strlen(name.c_str())));
                            __pos.Info = _luaBase_VariableInfo(name);
                            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
                            Values *VAL = new Values();
                            VAL->TYPO = LuaInteger;
                            VAL->__varname = name;
                            VAL->val0 = std::stof(std::string(_FRAME._data.begin(), _FRAME._data.end()));
                            ENV->setValue(__pos, VAL);
                        }
                    } else {
                        m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad syntax in line: " + getLine(_LINES)));
                        m_LuaErrorHandler->setFatal(true);
                        return;
                    }
                }
                break;
            }
            //END TYPES
            //BEGIN SYNTAX
            case _L_IF: {
                // CMP or TEST.
                // Need to read the args.
                ATR.IF = true;
                break;
            }
            //END SYNTAX
            case _L_SEPARATOR: {
                // Reset some parts.
                if (ATR.DECLR) {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Declaration not closed: " + getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return;
                } else {
                    ATR.DECLR = false;
                }
                ATR.LOCAL = false;
                ATR.FUNCARGS = false;
                ATR.VARNAME = false;
                ATR.VARNAME_STR = "";
                FvNpos _AG = FvNpos();
                break;
            }
            case _L_NEWLINE: {
                // Reset some parts.
                _LINES++;
                if (ATR.DECLR) {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Declaration not closed: " + getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    return;
                } else {
                    ATR.DECLR = false;
                }
                ATR.LOCAL = false;
                ATR.FUNCARGS = false;
                ATR.VARNAME = false;
                ATR.VARNAME_STR = "";
                FvNpos _AG = FvNpos();
                break;
            }
            case _L_EOF: {
                // Reset some parts.
                _LINES++;
                if (ATR.DECLR) {
                    m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Declaration not closed: " + getLine(_LINES)));
                    m_LuaErrorHandler->setFatal(true);
                    break;
                } else {
                    ATR.DECLR = false;
                }
                ATR.LOCAL = false;
                ATR.FUNCARGS = false;
                ATR.VARNAME = false;
                ATR.VARNAME_STR = "";
                FvNpos _AG = FvNpos();
                break;
            }

        }
        _pos++;
    }
    //Bundle & Run
    F_SCR.ret();
    F_SCR.finalize();
    SimpleFunc FN;
    rt.add(&FN, &code2);
    FN();
    // Check some boundaries...
    if (ATR.DECLR) {
        // Must be saved.
        // Get only the first index (Soon we will accept more than 1 idx)
        Values V_ = ret->data[0]; //Must be executed later! after the function
        if (!ATR.LOCAL) {
            lua_AddrSpec __pos;
            __pos.Type = _last_Hashed;
            __pos.Addr = std::hash<std::string_view>{}(std::string_view(ATR.VARNAME_STR.c_str(), strlen(ATR.VARNAME_STR.c_str())));
            __pos.Info = _luaBase_VariableInfo(ATR.VARNAME_STR);
            findEqualsToVarNdelete(__pos.Addr, m_Lua->m_General->Nodes);
            Values *VAL = new Values();
            VAL->TYPO = V_.TYPO;
            VAL->__varname = ATR.VARNAME_STR;
            VAL->val1 = V_.val1;
            VAL->val2 = V_.val2;
            VAL->val3 = V_.val3;
            VAL->val4 = V_.val4;
            VAL->val5 = V_.val5;
            m_Lua->m_General->Nodes->setValue(__pos, VAL);
        } else {
            // ASM CODE. UP.
        }
    }
}
*/


















