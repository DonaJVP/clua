#pragma once

#include <vector>
#include <string>
#include <cstdint>

enum _Lua_Lex_Keys {					// Lua Default	|	Addon				| Usable code for scripting | Usage in code lang
    // Declare Keys						//				|						| ***						| 
    _L_FUNCTION				=	1,		// function		|	__func				| $01						| 001 "Example"()
    _L_LOCAL				=	2,		// local		|	__onThisSector		| $02						| 002 "a_variable" <A raw lua value>
    _L_WHILE				=	3,		// while		|	__loop				| $03						| 003 <A raw lua value, shouln't be a string or number or else: true> "\n"
    _L_UNTIL				=	4,		// until		|	__cancelOn			| $04						| B} 004
    _L_FOR					=	5,		// for			|	__getElements		| $05						| 005 B{<007>
    _L_REPEAT				=	6,		// repeat		|	__loop_until		| $06						| 006 B{<007>
    // Misc								//				|						| ***						|
    _L_BOOL					=	7,		// -			|	bool				| $10						| bool Var1 = false		EQUALS TO "var = $"
    _L_INT					=	8,		// -			|	int					| $11						| int Var1 = 0			EQUALS TO "var = $"
    _L_STRING2				=	9,		// -			|	string				| $12						| string Var1 = ""		EQUALS TO "var = $"
    // Lexical Keys						//				|						| ***						| 
    _L_BlockStart			=	10,		// do			|	B{					| $07						| same
    _L_BlockEnd				=	11,		// end			|	B}					| $08						| same
    _L_AND					=	12,		// and			|	&&					| $09						| same
    _L_ELSE					=	13,		// else			|	|%					| $13						| same
    _L_ELSEIF				=	14,		// elseif		|	|&					| $14						| same
    _L_OR					=	15,		// or			|	||					| $15						| same
    _L_IF					=	16,		// if			|	-					| $16						| !
    _L_THEN					=	10,		// then			|	-					| $17						| !
    _L_BREAK				=	18,		// break		|	stop				| $18						| same
    _L_IN					=	19,		// in			|	-					| $19						| same
    _L_NIL					=	20,		// nil			|	-					| $20						| !
    _L_NOT					=	21,		// not			|	!					| $21						| same
    _L_RETURN				=	22,		// return		|	-					| $22						| same
    // Some Values			=			//				|						| ***						| 
    _L_TRUE					=	23,		// true			|	_1					| $23						| same
    _L_FALSE				=	24,		// false		|	_0					| $24						| same
    _L_NUMBER				=	25,		// -			|	-					| ***						| !
    _L_STRING				=	26,		// -			|	-					| ***						| !
    _L_VARNAME				=	27,		// -			|	-					| ***						| !
    _L_EOF					=	28,		// -			|	-					| ***						| !
    // Logical keys						//				|						| ***						|	
    _L_CONCAT				=	29,		// ..			|	&+					| $25						| same
    _L_MULTIPLEARGS			=	34,		// ...			|	__args				| $26						| same
    _L_EQUALS				=	30,		// ==			|	-					| $27						| same
    _L_EQUALS_OR_MORE		=	31,		// >=			|	__equals_or_more	| $28						| same
    _L_EQUALS_OR_MINUS		=	32,		// <=			|	__equals_or_minus	| $29						| same
    _L_DOESNT_EQUALS		=	33,		// ~=			|	__doenst_equals		| $30						| same
    _L_MAX					=	35, // Will be multiplied by 3. > (x * 3) - 14
    _L_DECLR				=	36, // .KEY. .ARGS/VALUE.
    _L_ON_TO_GO				=	37, // It must look like this OBJ->kad[()]
    _L_INCREMENT_VARNUM		=	38, // [VARNUM]++
    _L_DECREMENT_VARNUM		=	39, // [VARNUM]--
    _L_SYNTAX_SUM			=	40, // +
    _L_SYNTAX_DEC			=	41, // -
    _L_SYNTAX_MUL			=	42, // *
    _L_SYNTAX_DIV			=	43, // /
    _L_SYNTAX_EXP			=	44, // ^
    _L_SYNTAX_PER			=	45, // Addon: 45% (Divide 45 with 100 for 0.45)
    _L_TABLE_START			=	46, // {
    _L_TABLE_END			=	47, // }
    _L_SEPARATOR2			=	48, // ;
    _L_ON_TO_GO_P			=	49, // [ // Cover
    _L_CALL					=	50, // *
    _L_LABEL				=	51,
    _L_SEPARATOR			=	52,
    _L_FOR_INDEX			=	53,
    _L_FOR_DATA				=	54,
    _L_IS					=	55,
    _L_F_ARGS_START			=	56,
    _L_F_ARGS_END			=	57,
    _L_END					=	11,
    _L_THREAD				=	59,		// thread		|	thread				| $31						| same
    _L_NEWLINE				=	60,
    _L_VARTYPE				=	61,
    _L_MORETHAN				=	62,
    _L_MINUSTHAN			=	63,
    _L_NONE					=	64,
    _L_NOP					=	65,
    _L_START				=	66,
    _L_ON_TO_GO_END			=	67,
    _L_FUNCTIONPOINTER		=	68,
    _L_PATH					=	69,
    _L_EXPRESSION			=	70,
    _L_EXPRESSION_BRKT		=	71,
    _L_UNKNOWN				=	72,
    _L_DOUBLE				=	73,
    _L_ARGS_CONTROL			=	74,
    _L_TABLE				=	75,
    _L_OBJECT				=	76,
    _L_DECLR_PLUS_DATA		=	77,
    _L_OVERALLTYPECHECKER	=	78,
    // Special CLua keys, which they allow direct manipulation of CLua code generation
    _L_FLAG_IGNORE_VARCHECK	=	80,
    _L_FLAG_CONTINUE_FRAME2	=	81,
    // Some preconfigured keys
    _L_CONSTTABLE			=	82,
    _L_OBJECTCODENAME		=	83,
    _L_ON_TO_GO_OBJECT		=	84,
    _L_STRING_CTRL			=	85,
};

class lua_AddrPath;
class lua_biOpCode;
class lua_Scope;

struct __lua_blk_ver_frmPlusType {
    lua_AddrPath *addr_toVerify;
    uint64_t type; //If LuaUnknown pass.
};


struct LuaLexFrame {
    LuaLexFrame() { key = _L_NONE; };
    LuaLexFrame(_Lua_Lex_Keys KEY): key(KEY) {};
    _Lua_Lex_Keys key = _L_NONE;
    std::vector<uint8_t> _data; //Contents data
    std::string header;
    _Lua_Lex_Keys subkey = _L_NONE;
    std::string keystring;
    uint8_t ATTRIB;
    uint8_t LuaTYPE = 0;
    uint32_t t_string_idx = 0;
    lua_biOpCode *_OPCODE = nullptr;
    void *a;
    std::vector<std::vector<LuaLexFrame>> EXPR;
    std::vector<LuaLexFrame> EXPR_BRKT;
    bool _LK = false;
    bool skipcheck = false;
    bool local = false;
    bool declr = false;
    bool multipleway = false;
    lua_AddrPath *addr = nullptr; // Same as the old EXPR_BRKT
    // Block part (Verify variables)
    std::string *debugSymbolLine = nullptr;
    std::vector<__lua_blk_ver_frmPlusType> _TOVERIFYBLOCK_;
};

// AddrPath
class lua_AddrPath {
public:
    lua_AddrPath();
    bool needToResolveAddr();
    LuaLexFrame *getHeader();
    LuaLexFrame *getBack();
    std::vector<LuaLexFrame> *getData();
    std::string getHeaderVarString();
    uint64_t ATTRIB = 0;
    void assignNewAddr(const std::vector<LuaLexFrame> new_);
private:
    std::vector<LuaLexFrame> rawAddr;
};

enum lua_1_biOpCode : uint8_t {
    l_b_o_c_AND = 0,	// and
    l_b_o_c_NOT = 1,	// not
    l_b_o_c_OR_ = 2,	// or
    l_b_o_c_VTN = 3,	// if (var): existence of var
    l_b_o_c_IFS = 4,	// if
    l_b_o_c_ELS = 5,	// else
    l_b_o_c_ELI = 6,	// elseif
    l_b_o_c_SCP = 7,	// do/then <scope>
    l_b_o_c_SCE = 8,	// end
    l_b_o_c_FOR = 9,	// for
    l_b_o_c_FEX = 10,	// for expression: ($1, $2 in $3) or ($1 = $I, $I2, $I3)
    l_b_o_c_CFN = 11,	// call [Should had func arguments for the last key.]
    l_b_o_c_DEC = 12,	// Declaration [LuaLexKeys array] [Index only.]
    l_b_o_c_LXC = 13,	// Some lexical keys that should not be used here.
    l_b_o_c_SSE = 14,	// Syntax separator
    l_b_o_c_FUN = 15,	// F U N C T I O N
    l_b_o_c_RLE = 16,	// Raw Lua to Eval (Expressions)
    l_b_o_c_TBL = 17,	// Table
    l_b_o_c_MEM = 18,	// Last opcode on every vector/array, defines how big must be the mem allocation size
    l_b_o_c_UPV = 19,	// Upvalue
    l_b_o_c_STO = 20,	// Store to local trim
    l_b_o_c_EXP = 21,	// BRKT EXPR
    l_b_o_c_NUL = 22,	// NULL
    l_b_o_c_DEP = 23,	// Dependency flow, just for nested functions
    l_b_o_c_NOP = 24,	// No operation
    l_b_o_c_STM = 25,	// declaration.Local.Multiple
    l_b_o_c_RET = 26,	// return <expr>
};

typedef std::vector<std::vector<LuaLexFrame>> lua_Expression;

struct lua_biOpCode {
    lua_biOpCode() :
    OPCODE(l_b_o_c_NOP),
    SCOPE(nullptr),
    KEY(_L_NONE),
    LLF(std::vector<LuaLexFrame>()),
    LLF2(std::vector<LuaLexFrame>()),
    FuncPTR2(nullptr),
    _F_LOCAL(false),
    size(0),
    p(lua_Expression()),
    ptr(nullptr),
    toMemOffset(0),
    V(0),
    nestedtoUpValues(std::vector<std::string>()),
    path(nullptr),
    ATR(0),
    fixedaddr(0)
    {}
    lua_biOpCode(lua_1_biOpCode K): lua_biOpCode() { OPCODE = K; }
    lua_1_biOpCode OPCODE;
    lua_Scope *SCOPE;
    std::vector<LuaLexFrame> LLF;
    std::vector<LuaLexFrame> LLF2;
    _Lua_Lex_Keys KEY;
    std::vector<lua_biOpCode> *FuncPTR2; // Replaced to online building to match local variables
    bool _F_LOCAL = false;
    uint32_t size;
    lua_Expression p;
    void *ptr;
    uint64_t toMemOffset;
    uint64_t V; 
    std::vector<std::string> nestedtoUpValues;
    lua_AddrPath *path;
    uint8_t ATR;
    uint64_t fixedaddr = 0;
};

class Lua;
class LuaErrorHandler;

// LEXER
class LuaLex {
public:
    LuaLex(Lua *vm);
    LuaLex(Lua *vm, LuaErrorHandler *leH): m_Lua(vm), m_LuaErrorHandler(leH) {  }
    void readDataAndloadToLua(std::vector<uint8_t> data);
    std::string dumpInfo(std::vector<LuaLexFrame> S);
    std::vector<std::string> ParserFirstStage(std::vector<uint8_t> data);
    std::vector<LuaLexFrame> ParserSecondStage(std::vector<std::string> data);
    std::vector<LuaLexFrame> ParserThirdStage(std::vector<LuaLexFrame> keys);
protected:
    LuaLexFrame resolveCommandString(std::string __cmd);
private:
    bool areNumber(uint8_t data);
    Lua *m_Lua = nullptr;
    LuaErrorHandler *m_LuaErrorHandler = nullptr;
    bool err = false;
};

extern std::string luaLexFrameKeyToString(_Lua_Lex_Keys k);
extern std::vector<LuaLexFrame> analizeNupdateConstantsNvars(std::vector<LuaLexFrame> *Keys);
extern uint64_t __LEX_KEY_TO_LuaType(_Lua_Lex_Keys a, uint8_t ATTR);
std::vector<std::vector<LuaLexFrame>> _CPP__insertToFirstPosition(std::vector<LuaLexFrame> toPush, lua_Expression *expr);
LuaLexFrame getExprValue(std::vector<LuaLexFrame> *k, uint32_t *pos, lua_Scope *scope = nullptr, bool _returnEmptyIfVariables = false);
LuaLexFrame makeSinglePath(std::vector<LuaLexFrame> *keys, uint32_t *pos);
