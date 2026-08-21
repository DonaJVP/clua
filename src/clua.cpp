#include "cljit.hpp"
#include "lua.hpp"
#include "clua.hpp"
#include "clua_settings.hpp"
#include "terminalbuff.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include "clibInit.hpp"
#include "clobject.hpp"
#include "cllex.hpp"

LuaLex *LEXER = nullptr;
lua_ErrHandler *FIFO = nullptr;

//BEGIN NAMESPACE CLUA
Lua *CLUA::create(lua_ErrHandler *fifo) {
    FIFO = fifo;
    Lua *m_lua = new Lua();
    // Initialize seed
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    CLUA::seed = dist(gen);
    // Initialize error handler
    LuaErrorHandler *leh = new LuaErrorHandler(fifo);
    m_LuaErrorHandler = leh;
    // Lexer
    LuaLex *lex = new LuaLex(m_lua, leh);
    m_lua->m_LuaErrorHandler = leh;
    LEXER = lex;
    // world
    lua_Table *TBL = CLUA::table::createTable();
    m_General = TBL;
    CLUA::world::m_general = TBL;
    // Libraries
    LIBC__InitializeStreamLibrary(m_General);
    LIBC__InitializeStringLibrary(m_General);
    LIBC__InitializeDebugKit(m_General);
    _TEST__registerObject(m_General); //TEST
    return m_lua;
}
// CODE
// Preprocess data
std::vector<LuaLexFrame> CLUA::code::preprocess(const std::vector<uint8_t> data) { // Unknown.
    // Multi func.
    // Separates keywords
    std::vector<std::string> _f_phase = LEXER->ParserFirstStage(data);
    if (FIFO->err) {
        return std::vector<LuaLexFrame>{_L_NOP};
    }
    // Defines keys
    std::vector<LuaLexFrame> _s_phase = LEXER->ParserSecondStage(_f_phase);
    if (FIFO->err) {
        return std::vector<LuaLexFrame>{_L_NOP};
    }
    // Compiles strings
    std::vector<LuaLexFrame> _t_phase = LEXER->ParserThirdStage(_s_phase);
    if (FIFO->err) {
        return std::vector<LuaLexFrame>{_L_NOP};
    }
    return _t_phase;
}
// Organize keywords and organize them for assembler
lua_Scope *mainScope = nullptr;
std::vector<lua_biOpCode> CLUA::code::compiler(std::vector<LuaLexFrame> data) {
    uint32_t pos = 0;
    lua_Scope *mScope = new lua_Scope();
    mainScope = mScope;
    mScope->_002 = true;
    std::vector<lua_biOpCode> _f_phase = lua_B_F_OP(&data, &pos, mScope);
    if (FIFO->err) {
        return std::vector<lua_biOpCode>{};
    }
    return _f_phase;
}
// Assembler
CLUA::function CLUA::code::assembler(std::vector<lua_biOpCode> data) {
    void *f = luaBundleFunction(&data, mainScope, false, nullptr, nullptr, true);
    if (FIFO->err) {
        return nullptr;
    }
    return (CLUA::function)f;
}
CLUA::function CLUA::gen(const std::vector<uint8_t> data) {
    std::vector<LuaLexFrame> _LLFVCT = code::preprocess(data);
    if (FIFO->err) {
        return nullptr;
    }
    std::vector<lua_biOpCode> _LBOPVCT = code::compiler(_LLFVCT);
    if (FIFO->err) {
        return nullptr;
    }
    CLUA::function f = code::assembler(_LBOPVCT);
    if (FIFO->err) {
        return nullptr;
    }
    return f;
}
// Table access.
lua_Table *CLUA::table::createTable() {
    lua_Table *tbl = new lua_Table();
    tbl->hmask = 0xFFF;
    tbl->array = new Values[0xFF];
    tbl->nodes = new Node[0xFFF];
    tbl->asize = 0xFF;
    tbl->hsize = 0xFFF*sizeof(Node);
    return tbl;
}
// Array type.
CLUA::VALUE *CLUA::table::getPtr(lua_Table *table, size_t slot) {
    if (table->asize < slot)
        return 0;
    return &table->array[slot];
}
CLUA::VALUE CLUA::table::getData(lua_Table *table, size_t slot) {
    if (table->asize < slot)
        return 0;
    return reinterpret_cast<CLUA::VALUE>(table->array[slot]);
}
CLUA::VALUE *CLUA::table::setDataNoCheck(lua_Table *table, size_t slot, CLUA::VALUE data) {
    Values *v = &table->array[slot];
    *v = data;
    return v;
}
CLUA::VALUE *CLUA::table::setData(lua_Table *table, size_t slot, CLUA::VALUE data) {
    if (table->asize > slot) {
        Values *v = &table->array[slot];
        *v = data;
        return v;
    } else {
        if (table->asize*2 < slot) {
            m_LuaErrorHandler->reportError(_lua_es_oversizedSave, 0, "Used CLUA::table::setData(...) which slot are larger than table->arraySize * 2");
            m_LuaErrorHandler->setFatal(true);
        } else {
            // Resize.
            uint64_t nSize = table->asize * 2;
            void *nM = malloc(nSize);
            memcpy(nM, table->array, table->asize);
            table->asize *= 2;
            Values *v = &table->array[slot];
            *v = data;
            return v;
        }
    }
}
// String type
CLUA::VALUE *CLUA::table::setData_String(lua_Table *table, TString *slot, VALUE data) {
    size_t idx = slot->IDX & table->hmask;
    Node* n = &table->nodes[idx];
    if (n->key == nullptr) {
        n->key = slot;
        n->val = data;
        n->next = nullptr;
        table->used_on_smap++;
        return &n->val;
    }
    if (table->used_on_smap *4 >= table->hmask * 3) { //More than 75%
        //Resize this map.
        table->nodes = _F_ASM_MAKETABLENREHASH(table, (table->hsize*2));
    }
    while (true) {
        if (std::strcmp(n->key->data, slot->data) == 0) {
            n->val = data; // overwrite
            return &n->val;
        }
        if (!n->next)
            break;
        n = n->next;
    }
    n->next = new Node{slot, data, nullptr};
    table->used_on_smap++;
    return &n->val;
}
CLUA::VALUE CLUA::table::getData_String(lua_Table *table, TString *slot, VALUE v) {
    VALUE val = v;
    size_t idx = slot->IDX & table->hmask;
    Node* n = &table->nodes[idx]; // sizeof(Node) * idx
    if (n != nullptr) {
        if (n->key) {
            return n->val;
        } else {
            Node *_n = n->next;
            while (_n) { // o(n)
                if (_n->key) {
                    if (std::strcmp(_n->key->data, slot->data) == 0) {
                        return _n->val;
                    }
                    _n = _n->next;
                }
            }
        }
    }
    return val;
}
CLUA::VALUE *CLUA::table::getPtr_String(lua_Table *table, TString *slot) {
    size_t idx = slot->IDX & table->hmask;
    Node* n = &table->nodes[idx];
    if (!n->key) {
        // Not allocated.
        return _F_ASM_NOTGUARANTEED_SETVALUE(table, slot, 0);
    } else {
        if (!n->next) {
            return &n->val; // o(5)
        } else {
            Node *_n = n->next;
            while (_n) { // o(n)
                if (_n->key) {
                    if (std::strcmp(_n->key->data, slot->data) == 0) {
                        return &_n->val;
                    }
                    _n = _n->next;
                }
            }
        }
    }
    return _F_ASM_NOTGUARANTEED_SETVALUE(table, slot, 0);
}
CLUA::VALUE *CLUA::table::getPtr_String_noAlloc(lua_Table *table, TString *slot, VALUE *fv) {
    size_t idx = slot->IDX & table->hmask;
    Node* n = &table->nodes[idx];
    if (!n->key) {
        // Not allocated.
        return fv;
    } else {
        if (!n->next) {
            return &n->val; // o(5)
        } else {
            Node *_n = n->next;
            while (_n) { // o(n)
                if (_n->key) {
                    if (std::strcmp(_n->key->data, slot->data) == 0) {
                        return &_n->val;
                    }
                    _n = _n->next;
                }
            }
        }
    }
    return fv;
}
TString *CLUA::doString(const char *str, const size_t size) {
    returnCompiledString(std::string(str, size));
}
//END NAMESPACE

std::vector<char*> filesToLoad;

// Parse options.
bool _main__processOptions(int count, char *args[]) {
    if (count == 1) {
        return false;
    }
    int countercount = 0;
    while (count) {
        if (!std::strcmp(args[countercount], "--showcode")) {
            showGeneratedCode = true;
        } else if (!std::strcmp(args[countercount], "--exportToFile")) {
            exportCodeToUniqueFile = true;
            _ECTUF_Filename = args[countercount+1];
            countercount++;
        } else if (!std::strcmp(args[countercount], "--loadfile")) {
            filesToLoad.push_back(args[countercount+1]);
            countercount++;
        }
        countercount++;
        count--;
    }
    return true;
}

void _CALLBACK_(const std::vector<char> data) {
    CLUA::gen(std::vector<uint8_t>(data.begin(), data.end()))(0, 0, nullptr);
}

// The core.

int main(int argc, char* argv[]) {
    // Initialize CLua.
    lua_ErrHandler *f = new lua_ErrHandler();
    Lua *m_Lua = CLUA::create(f);
    bool res = _main__processOptions(argc, argv);
    if (!res) {
        // Initialize terminal api.
        CL_TerminalBuffer *terminalBuffer = new CL_TerminalBuffer();
        terminalBuffer->_callback = (_CL_TB_CLBK_String)_CALLBACK_;
        // Maybe... attach.
        terminalBuffer->loop();
    }
    return 0;
}
