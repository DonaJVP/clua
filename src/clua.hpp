#include "cllex.hpp"
#include "ltable.hpp"
#include <cstdint>

#define LANGUAGE_NAME       "CLua"
#define CLUA_VERSION        "Earth v0.0.1"
#define CLUA_RELEASE        "Earth v0.0.1"
#define CLUA_VERNUM         001
#define CLUA_COPYRIGHT      "Copyright (C) 2025-2026, Donatto Viveros"
#define CLUA_AUTHORS        "Donatto Josue Viveros Pintos"

#define CLUA_NANMASK 0x7FF0000000000000ULL
#define CLUA_DATAMDK 0x0000FFFFFFFFFFFFULL

// This just includes a new set of ordered function initialization.

struct lua_ErrHandler;

namespace CLUA {
    // Theres no reason for the Lua class existence.
    typedef Values VALUE;
    typedef Values*(*function)(Values, Values, Values*); // rdi, rsi, rdx
    // STATE
    Lua *create(lua_ErrHandler *fifo);
    
    namespace code {
        // Process keywords
        std::vector<LuaLexFrame> preprocess(const std::vector<uint8_t> data);
        // Optimizes and prepare code for assembly
        std::vector<lua_biOpCode> compiler(std::vector<LuaLexFrame> data);
        // Builds the entire scripts
        function assembler(const std::vector<lua_biOpCode> code);
    }
    
    // Quick code
    function gen(const std::vector<uint8_t> data);
    
    // TABLE
    namespace table {
        // Creation
        lua_Table *createTable(); // Max: 0xFFF
        // Modifications
        // Read features
        VALUE *getPtr_String(lua_Table *table, TString *slot);
        VALUE *getPtr_String_noAlloc(lua_Table *table, TString *slot, VALUE *fv);
        VALUE getData_String(lua_Table *table, TString *slot, VALUE fallback);
        VALUE *getPtr(lua_Table *table, size_t slot);
        // VALUE *getPtr_noAlloc(lua_Table *table, size_t slot); // Maybe modify when it at full capacity.
        VALUE getData(lua_Table *table, size_t slot);
        // Write features
        VALUE *setData_String(lua_Table *table, TString *slot, VALUE data);
        VALUE *setData(lua_Table *table, size_t slot, VALUE data);
        VALUE *setDataNoCheck(lua_Table *table, size_t slot, VALUE data);
    }
    // STRING
    TString *doString(const char *str, const size_t size = 0);
    // TYPES
    enum TYPE: uint64_t {
        LuaInteger	=	0x8,
        LuaNumber	=	0x9,
        LuaString	=	0x4,
        LuaFunction =	0x6,
        LuaObject	=	0x7,
        LuaTable	=	0x5,
        LuaNil		=	0x1,
        LuaBoolean 	=	0x3,
        LuaUnknown	=	0x2,
        LuaERR		=	0xF,
    };
    // VALUE manipulation
    inline VALUE makeV(uint64_t d, TYPE t) { return (CLUA_NANMASK | (t << 48) | d); }
    inline VALUE makeV_D(double k) { return static_cast<uint64_t>(k); }
    inline TYPE  getVtype(VALUE d) { return TYPE((d >> 48) & 0xF); }  
    inline uint64_t getVdata(VALUE d) { return (uint64_t)(d & CLUA_DATAMDK); }
    inline bool isVdouble(VALUE k) { return ((k & CLUA_NANMASK) != CLUA_NANMASK); }
    // Script
    namespace world {
        lua_Table *m_general = nullptr;
    }
    // Misc
    uint32_t seed = 0;
}
