/******************************************************************************
* Copyright (C) 1994-2012 Lua.org, PUC-Rio.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

/*

	SOFTWARE MODIFIED BY LOGIKI (Donatto J. V. P.) 2025

*/

#include <stdarg.h>
#include <stddef.h>
#include <cstdint>
#include <string>
#include <sys/ucontext.h>
#include <unordered_map>
#include <mutex>

// Converted from Stack to Tree-walk
// The fun is infinite.

#define LANGUAGE_NAME			"CLua"
#define LUA_VERSION				"Little Moon v0.1"
#define LUA_RELEASE				"CLua Little Moon [v0.1]"
#define LUA_VERSION_NUM			001
#define LUA_COPYRIGHT			"Copyright (C) 1994-2012 Lua.org, PUC-Rio"
#define CLUA_COPYRIGHT			"Copyright (C) 2025-2026, Logiki"
#define CLUA_AUTHORS			"Donatto Josue Viveros Pintos"
#define LUA_AUTHORS 			"R. Ierusalimschy, L. H. de Figueiredo & W. Celes"

#include <vector>

#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>

#pragma once

enum LuaType: uint64_t {
	LuaInteger	=	0x8,
	LuaNumber	=	0x9, // JUST FOR CONVERTIONS
	LuaString	=	0x4,
	LuaFunction =	0x6,
	LuaObject	=	0x7,
	LuaTable	=	0x5,
	LuaNil		=	0x1,
	LuaBoolean 	=	0x3,
	LuaUnknown	=	0x2, // Must be used ONLY for self discover
	LuaERR		=	0xF,
};
extern std::string getLuaTypeString(LuaType TYP);
typedef __uint64_t Values;
typedef __uint64_t FuncArgs;

/*
struct FuncArgs {
	FuncArgs(const FuncArgs &R): size(R.size), data(R.data)  {};
	FuncArgs() = default;
	uint16_t size; //0
	Values* data; //2
};*/

typedef FuncArgs*(*FunctionPointer)(Values, Values, FuncArgs*); // rdi, rsi, rdx

// Just for debugging and a good backtrace, we don't want a backtrace with full 0x000000... things
struct _luaBase_VariableInfo {
	_luaBase_VariableInfo() = default;
	_luaBase_VariableInfo(std::string str): _name(str) {}
	std::string _name;
	//std::string _desc; // Useless, but a reason for Variable Info
};

struct TString {
	uint64_t IDX;
	uint64_t len;
	char *data;
	uint64_t _unocc = 0;
};

struct lua_Table;


/*
struct Values {
	LuaType type;
	union {
		double num;
		TString* str;
		lua_Table* table;
		FunctionPointer func;
		void* userdata;
	};
};
*/

struct Node {
	TString *key = nullptr;
	Values val = 0;
	Node *next = 0;
	uint64_t trsh; // Complete 32bytes
};

// array, asize, nodes, hsize, metatable, 0, 1, hmask
// 0, 8, 16, 24, 32, 40, 48, 56
struct lua_Table {
	// Array part
	Values* array;
	uint64_t asize;
	Node* nodes;
	uint64_t hsize;
	lua_Table* metatable;
	//Sizes
	uint64_t used_on_smap = 0;
	uint64_t used_on_amap = 0;
	uint64_t hmask = 0;
	uint64_t kShape = 0;
	uint64_t _BOOL_constTable = false;
	uint64_t _padding;
};

// Values [Are all lua values]
/*struct Values {
	Values() = default;
	float val0; // Number														// 0
	std::string val1; // String													// 8
	void *val2; // Must be a pointer to a object								// 40
	FunctionPointer val4; // Address, valid if an function set					// 48
	lua_Table *val3 = nullptr;													// 56
	LuaType TYPO = LuaNil; // Type that will be used, other types are GARGABE	// 64
	std::string __varname; // Debug only										// 68
	bool val5; //104bytes														// 100
	size_t vrn; // 112bytes														// 101
};
struct ValuesMTX {
	ValuesMTX() = default;
	float val0; // Number
	std::string val1; // String
	void *val2; // Must be a pointer to a object
	FunctionPointer val4; // Address, valid if an function set
	lua_Table *val3 = nullptr;
	LuaType TYPO = LuaNil; // Type that will be used, other types are GARGABE
	bool val5;
	size_t vrn;
	std::string __varname; // Debug only
	std::mutex mtx;
};*/

enum _lua_IndexMethod {
	_lua_idxm_nil, // Should throw exception :)
	_lua_idxm_string,
	_lua_idxm_integer,
};

struct lua_IndexMethod {
	lua_IndexMethod(_lua_IndexMethod method = _lua_idxm_nil, size_t h = 0, uint32_t i = 0): idx_typo(method), idx_str_h(h), idx_int(i) {};
	lua_IndexMethod(bool idk) {};
	
	std::string idx_str;
	
	size_t idx_str_h;
	
	uint32_t idx_int;
	_lua_IndexMethod idx_typo;
};



// TableFields
struct lua_TableFields {
	size_t _FIELD_FIRST; // Object is not an valid identifier, we must remember that addressing tables must be fast, so IT WILL BE HASHED
	void *_FIELD_DATA; // Link to any object
};

// Memory Address helper; Crucial for the compiled lua code, as it will need a lot of this just to work
enum _lua_AddrSpecType {
	_last_Hashed,
	_last_Integer,
};
struct lua_AddrSpec {
	lua_AddrSpec() = default;
	lua_AddrSpec(_lua_AddrSpecType t, size_t addr = 0, uint32_t addr2 = 0): Type(t), Addr(addr), Addr2(addr2) {}
	_lua_AddrSpecType Type;
	size_t Addr;
	uint32_t Addr2;
	_luaBase_VariableInfo Info; // Can be null
};

// Backtrace Helpers
struct lua_VarOnMemInfo {
	lua_VarOnMemInfo(std::string v, size_t a, LuaType type_): varname(v), Addr(a), type(type_) {};
	lua_VarOnMemInfo() = default;
	std::string varname;
	size_t Addr;
	LuaType type;
};

// Helper
extern std::string luaVarInfoToString(lua_VarOnMemInfo data);

// Error handlers
// This should had a mutexed variable which contains the backtrace of the crash
enum lua_ErrSignals:uint64_t {
	// Errors
	_lua_es_NonFunction = 0,
	_lua_es_ValueIsNil = 1,
	_lua_es_NotCorrect = 2,
	_lua_es_ArgIsProblem = 3,
	_lua_es_BadString = 4,
	_lua_es_WhatTheHell = 5,
	_lua_es_InvalidUsage = 7,
	_lua_es_BadSyntax = 8,
	_lua_es_BadVariableNamingMethod = 9,
	_lua_es_NotTable = 10,
	_lua_es_InvalidType = 11,
	_lua_es_TooSmallEntry = 12, // They're testing the language on a invalid way
	_lua_es_BadTyping = 14,
	_lua_es_NotAIntNum = 15,
	_lua_es_NotAString = 16,
	_lua_es_UnknownErr = 17,
	_lua_es_Illegal = 18,
	// Warnings
	_lua_es_UnknownDataIdx = 6,
	_lua_es_FutureCrashAtRuntime = 13,
};
struct lua_ErrHandler {
	lua_ErrHandler() = default;
	std::string reason;
	bool fatal = false;
	bool err = false;
	bool nomutex = false;
	std::mutex mtx;
};
class LuaErrorHandler {
public:
	LuaErrorHandler(lua_ErrHandler *pipe): m_pipe(pipe) {};
	void reportError(const lua_ErrSignals signal, const size_t funcid = 9898986555, std::string reason = ""); // If not a function, only throw an reason with a ErrSignal.
	void reportWarning(const lua_ErrSignals signal, const size_t funcid = 9898986555, std::string reason = "");
	void setFatal(bool val) { m_pipe->mtx.lock(); m_pipe->fatal=true; m_pipe->mtx.unlock(); }
private:
	lua_ErrHandler *m_pipe = nullptr;
};

// Table Base 2

class Lua;

// The sauce for the table representation, this represents a single node
/*class LuaTableBase {
public:
	LuaTableBase(Lua *obj);
	Values getValueIDX(uint32_t val);
	Values *getRawAddrValueIDX(uint32_t val); // This might pass as the getValue value (std::move method) [UNSAFE]
	Values *getValueAddr(size_t val);
	Values *getValueAddrIgnWarn(size_t val);
	//void *getRawAddrValueAddr(size_t val); //Invalid. D>
	void setValue(lua_AddrSpec pos, Values val); // This doens't check
	void setValue(lua_AddrSpec pos, Values *val); // This is faster
	
	// Table manipulation
	size_t getSize() { return _Base.size() + _Base2.size(); }
	void clearTable();
	
	// Debugging
	void linkAddrToInfo(size_t addr, lua_VarOnMemInfo *L_vomi);
	lua_VarOnMemInfo *getAddrInfo(size_t addr) { return AddrToInfo.at(addr); }
	Lua *m_Lua = nullptr;
	void rmv_kw(size_t var) { _Base.erase(var); }
	void rmv_kwi(uint32_t var) { _Base2.erase(_Base2.begin() + var); }
private:
	std::unordered_map<size_t, Values*> _Base; // Always needs to be hashed, other value that are NOT PERMITTIED will be discarted
	std::vector<Values*> _Base2; // Otherwise if the vector has not names, like this: { 1, 2, 3 } then it should be indexed by values
	std::unordered_map<size_t, lua_VarOnMemInfo*> AddrToInfo;

	LuaErrorHandler *m_LuaErrorHandler = nullptr;
	
	// Apply some multithreaded environment
	//std::mutex m_mtx; //Might should only be mutexted in external environments.
	std::atomic<bool> appyable_mt_env_;
	std::mutex env_;
};

// Table
enum _lua_TablePos {
	_lua_tpos_node,
	_lua_tpos_meta
};
struct lua_Table {
	lua_Table(Lua *obj): Nodes(new LuaTableBase(obj)), Meta(new LuaTableBase(obj)) {};
	LuaTableBase *Nodes;
	LuaTableBase *Meta;
};*/

class Lua {
public:
	Lua() = default;
	//static lua_State *lua_newstate(lua_Alloc f, void *ud);
	// Functions
	//size_t callLuaFunction(lua_State *L, const char *name);  // U S E  T H E  B U N D L E D  C A L L E R .
	//size_t callLuaFunction(lua_State *L, std::string _F);    // U S E  T H E  B U N D L E D  C A L L E R .
	//FuncArgs *callLuaFunction(lua_Table *tbl, const lua_IndexMethod funcid, FuncArgs *args); // T H I S .
	//size_t registerCFunction(FunctionPointer fn, lua_IndexMethod to_index, lua_Table *tbl, _lua_TablePos pos); // By default, store into the data part, not metatable
	LuaErrorHandler *m_LuaErrorHandler = nullptr;
	void setNil(lua_Table *tbl, const lua_IndexMethod place);
	lua_Table *m_General = nullptr; // Might be _G
private:


	std::unordered_map<size_t, FunctionPointer> REGISTERED_FUNCTIONS; // Hashed names with std::hash<std::string || const char*> //NOTE: This will not always has all functions because of collisions, just have the globals
	//std::unordered_map<size_t, StringStruct> REGISTERED_STRINGS;
	// Some helpers to help the developer get it errors backtrace
};

// Some misc for API
typedef Values lua_Value;

// Lexical reader

/*
*
* On the lexical part, every line has not limits of characters. BUT: It shouln't jump to another new line if defining something, likely using this '"' character. It will crash.
* The space should act as a separator some times.
*
*/


// I've made new keys, for better scripting, hehe. E N J O Y .
// When saving a bytecode file, well, compiled, uhm, the code below (those numbers) will be used for better language understanding
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
	//_L_INDEX_START			=	48, // [
	//_L_INDEX_END			=	49, // ]
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
};

extern const std::string _LuaKeysString[85];

struct lua_biOpCode;
struct lua_AddrPath;

struct __lua_blk_ver_frmPlusType {
	lua_AddrPath *addr_toVerify;
	LuaType type; //If LuaUnknown pass.
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
	std::vector<__lua_blk_ver_frmPlusType> _TOVERIFYBLOCK_;
};


// lpath

class lua_AddrPath {
public:
	lua_AddrPath();
	bool needToResolveAddr();
	LuaLexFrame *getHeader();
	LuaLexFrame *getBack();
	std::vector<LuaLexFrame> *getData();
	std::string getHeaderVarString();
private:
	std::vector<LuaLexFrame> rawAddr;
};


typedef std::vector<std::vector<LuaLexFrame>> lua_Expression;

enum lua_OperatorKey : uint8_t {
	l_o_k_MAX,
	l_o_k_MIN,
	l_o_k_EQU,
	l_o_k_EMX,
	l_o_k_EMN,
};
struct lua_Operator {
	lua_OperatorKey key;
	_Lua_Lex_Keys arg1 = _L_NIL;
	_Lua_Lex_Keys arg2 = _L_NIL;
};

enum _lua_fex_expr_types:uint8_t {
	_L_FEX_EXPR_twodata_onetable = 0,
	_L_FEX_EXPR_counter_threeCfg = 1,
};
struct lua_biOpCodeFexExpr {
	_lua_fex_expr_types TYPE; // heh, ignore the names i'd give
	std::vector<LuaLexFrame> ARG2;				//	_			;;		
	LuaLexFrame ARG1;				//	i			;;		i
	std::vector<LuaLexFrame> ARG3;	//	pairs(core.get_connected_players()) -> _L_VARNAME _L_FUNC_ARGS_BEGIN _L_VARNAME _L_ON_TO_GO _L_VARNAME _L_FUNC_ARGS_BEGIN _L_FUNC_ARGS_END _L_FUNC_ARGS_END
	std::vector<LuaLexFrame> ARG4;	//	Size of max count
	//Huh
	bool setARG1 = false; // Must appear <both>
	bool setARG2 = false; // Must appear <Optional for _L_FEX_EXPR_twodata_onetable but STARTING POINT for _L_FEX_EXPR_counter_threeCfg>
	bool setARG3 = false; // Must appear <END POINT for _L_FEX_EXPR_counter_threeCfg >
	bool setARG4 = false; // Optional
};

//struct lua_CallSymbolConfig {
	//l_b_o_c_DEC should be used.
//};

struct lua_localSymbol {
	int64_t slot = 0;
	uint8_t qID = 0;
	uint8_t cacheReg = 0;
	asmjit::x86::Gp register_ = asmjit::x86::rax;
	std::string id = "";
	LuaType type = LuaUnknown;
};
struct _helperLua_ArgsPos {
	int32_t rdi = 0;
	int32_t rsi = 0;
};
using SymbolTable = std::unordered_map<std::string, lua_localSymbol>; // uint64_t = RSP pos
struct lua_Scope {
	lua_Scope();
	std::vector<lua_Scope*> lSCOPE; //Front
	lua_Scope* rSCOPE; //Reverse
	SymbolTable symbols;
	size_t base_slot = 0;
	bool _001; // Inside a function, should request to save to UpValues, this should be done after general function generation.
	bool _002; // Inside main script stack
	/*
	 * 0 = Script
	 * 1..N = Scopes
	 * N+1..N+1 = Actual Main scope
	 */
	uint32_t lvl = 0;
	uint32_t count = 0;
	uint32_t toEXbytes = 0;
	_helperLua_ArgsPos argPos{0,0};
	std::unordered_map<std::string, uint32_t> HottestVariables; // Top fourth hottest variables
	std::vector<std::pair<std::string, uint32_t>> HVtoCompiler;
	static std::vector<lua_biOpCode> *updateHottestVariablesForKeys(lua_Scope *MAINSCOPE, std::vector<lua_biOpCode> *DTA);
};

struct lua_UpValue {
	uint64_t offset;
	lua_Scope *scope;
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
	
};
struct lua_biOpCode {
	lua_biOpCode() :
	OPCODE(l_b_o_c_NOP),
	SCOPE(nullptr),
	KEY(_L_NONE),
	LLF(std::vector<LuaLexFrame>()),
	LLF2(std::vector<LuaLexFrame>()),
	FuncPTR(nullptr),
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
	lua_Operator OP;
	std::vector<LuaLexFrame> LLF;
	std::vector<LuaLexFrame> LLF2;
	_Lua_Lex_Keys KEY;
	lua_biOpCodeFexExpr FexEXPR;
	FunctionPointer FuncPTR;
	std::vector<lua_biOpCode> *FuncPTR2; // Replaced to online building to match local variables
	bool _F_LOCAL = false;
	uint32_t size;
	lua_Expression p;
	void *ptr;
	uint64_t toMemOffset;
	Values V; 
	std::vector<std::string> nestedtoUpValues;
	lua_AddrPath *path;
	uint8_t ATR;
	uint64_t fixedaddr = 0;
};
/*struct lua_biOpCode {
	lua_biOpCode() {
		//SCOPE = new lua_Scope();
	}
	lua_biOpCode(lua_1_biOpCode K): OPCODE(K) {}
	lua_1_biOpCode OPCODE;
	lua_Scope *SCOPE;
	lua_Operator OP;
	std::vector<LuaLexFrame> LLF; //
	std::vector<LuaLexFrame> LLF2;
	_Lua_Lex_Keys KEY;
	lua_biOpCodeFexExpr FexEXPR;
	FunctionPointer FuncPTR;
	std::vector<lua_biOpCode> *FuncPTR2; // Replaced to online building to match local variables
	bool _F_LOCAL = false;
	uint32_t size;
	lua_Expression p;
	void *ptr;
	uint64_t toMemOffset;
	Values V; 
	std::vector<std::string> nestedtoUpValues;
	lua_AddrPath *path;
	uint8_t ATR;
	uint64_t fixedaddr = 0;
};*/

struct CacheAttributes {
	bool LOCAL = false;
	uint16_t TOENCLOSE = 0;
	bool TYPES = false;
	bool FUNC = false;
	bool IF = false;
	bool SWITCH = false;
	bool WHILE = false;
	bool VARNAME = false;
	bool DECLR = false;
	bool FUNCARGS = false;
	std::string VARNAME_STR = "";
	uint16_t F_AG_ENC = 0;
	LuaType typo;
};

struct ValNpos {
	Values *val;
	uint32_t pos;
};

struct FvNpos {
	FuncArgs *args;
	uint32_t pos;
};

class LuaLex {
public:
	LuaLex(Lua *vm): m_Lua(vm) { m_LuaErrorHandler = vm->m_LuaErrorHandler; }
	LuaLex(Lua *vm, LuaErrorHandler *leH): m_Lua(vm), m_LuaErrorHandler(leH) {  }
	void readDataAndloadToLua(std::vector<uint8_t> data);
	std::string dumpInfo(std::vector<LuaLexFrame> S);
	std::vector<std::string> ParserFirstStage(std::vector<uint8_t> data);
	std::vector<LuaLexFrame> ParserSecondStage(std::vector<std::string> data);
	std::vector<LuaLexFrame> ParserThirdStage(std::vector<LuaLexFrame> keys);
	FuncArgs *getFuncArgs(uint32_t _LINES, std::vector<std::vector<LuaLexFrame>> *_FuncArgs, uint16_t _FuncArgsLevel, LuaLexFrame _FRAME, CacheAttributes *ATR, lua_Table *ENV, uint32_t POS, std::vector<LuaLexFrame> *Keys);
protected:
	LuaLexFrame resolveCommandString(std::string __cmd);
private:
	bool areNumber(uint8_t data);
	Lua *m_Lua = nullptr;
	LuaErrorHandler *m_LuaErrorHandler = nullptr;
	bool err = false;
};

struct FuncStructure {
	std::vector<LuaLexFrame> DIR;
	std::vector<LuaLexFrame> FUNC;
	bool local = false;
};

struct TypeData {
	_Lua_Lex_Keys TYPE;

};

struct _superBlockDataEnv;

struct _superBlockDataEnv {
	_superBlockDataEnv() = default;
	_superBlockDataEnv *superLevelUp = nullptr; //Must be an level lvl - 1
	uint8_t lvl = 0;
	std::unordered_map<size_t, Values*> TBL;
};

struct _laddr_searchPlaces {
	lua_Table *gen;
	lua_Table *env;
	FuncArgs *arg = nullptr;
	_superBlockDataEnv *sk = nullptr;
};

struct Script {
	Script() = default;
	std::vector<FuncStructure> Functions;
	std::vector<std::vector<LuaLexFrame>> LocalVariables;
	std::vector<std::vector<LuaLexFrame>> Variables;
	std::vector<LuaLexFrame> Quickrun;
};

using tStringTable = std::unordered_map<std::string, TString>;

extern void setValueHashed(lua_Table *base, const size_t hash, FuncArgs *Val);
extern void setValueInteger(lua_Table *base, const size_t int_, FuncArgs *Val);
//extern ValNpos returnSingleFrame(std::vector<LuaLexFrame> *Keys, uint32_t pos, lua_Table *General, LuaTableBase *ENV, uint32_t _LINES, bool allocVar = false, FuncArgs *ARGS = nullptr);
//extern Values *returnSingleFrame2(std::vector<LuaLexFrame> *Keys, lua_Table *General, LuaTableBase *ENV, FuncArgs *ARGS, uint32_t _LINES);
extern FvNpos getArgs(std::vector<LuaLexFrame> *Keys, uint32_t POS, lua_Table *General, lua_Table *ENV, uint32_t _LINES);
extern FuncArgs *getArgs2(std::vector<LuaLexFrame> *Keys, lua_Table *General, lua_Table *ENV, FuncArgs *ARGS, uint32_t _LINES);
extern LuaErrorHandler *m_LuaErrorHandler;
extern lua_Table *m_General;
extern std::vector<std::vector<std::string>> __LINES;
extern std::string getLine(uint32_t line, uint32_t _FI = 0);
extern void luaErr(lua_ErrSignals sgn, uint32_t file, uint32_t line);
extern uint32_t luaCurrentFileId;
extern tStringTable stringTable;

//getAddress()
Values *luaGetAddress(std::vector<LuaLexFrame> *Keys, _laddr_searchPlaces *places, uint32_t _LINES);
Values *luaGetAddress(std::vector<LuaLexFrame> *Keys, _laddr_searchPlaces *places, uint32_t _LINES, uint32_t &pos);

//bundleFunction()
//FunctionPointer luaBundleFunction(std::vector<LuaLexFrame> *Keys, lua_Table *ENV, uint32_t _LINES, uint32_t &pos);

//getExpr()
Values *luaGetExpr(std::vector<LuaLexFrame> *Keys, _laddr_searchPlaces *P, _Lua_Lex_Keys end, uint32_t &pos, uint32_t _LINES = 0);
Values *luaGetExpr(std::vector<LuaLexFrame> *Keys, _laddr_searchPlaces *P, _Lua_Lex_Keys end, uint32_t _LINES = 0);

//Tstring manipulation
extern TString *returnCompiledString(std::string N);
extern TString returnIndexOfString(uint32_t i);
extern TString *returnIndexOfStringPTR(uint32_t i);
extern void removeStringId(uint32_t I);
extern std::vector<TString> _L_L_C_S;
extern std::vector<uint32_t> _L_L_C_S_GC;

// Bidirectional Opcode Manipulation
extern std::vector<lua_biOpCode> lua_B_F_OP(std::vector<LuaLexFrame> *Keys, uint32_t *pos, lua_Scope *bulldozer, bool _ONLYFUNC = false, bool _INSIDEAFUNC = false);

// Build function by BiOp
extern void *luaBundleFunction(std::vector<lua_biOpCode> *_CODE, lua_Scope *THREADRIPPER, bool _online_gen, void *F_MEM_UF, void *F_MEM_SCR, bool Script);

// Table building
extern lua_Table *buildTableFromKeys(std::vector<LuaLexFrame> *Keys, uint32_t &pos);

extern Values _F_ASM_NOTGUARANTEED_GETVALUE(lua_Table *t, TString *k, Values nullPtr);
extern Node *_F_ASM_MAKETABLENREHASH(lua_Table *_T, uint32_t s_);
extern Values *_F_ASM_NOTGUARANTEED_SETVALUE(lua_Table *t, TString *key, Values v);
extern void *_F_ASM_NOTGUARANTEED_GETPTR(lua_Table *t, TString *k);
extern void *_F_ASM_NOTGUARANTEED_GETPTR_NOALLOC(lua_Table *t, TString *k, void *NPTR);

extern uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

extern lua_Expression lua_AcquireNAssembleLuaExpr(std::vector<LuaLexFrame> *Keys, uint32_t *pos, lua_Scope *S, std::vector<std::string> *TR, std::unordered_map<std::string, uint32_t> *hv);
extern std::vector<LuaLexFrame> lua_AcquireNAssembleLuaExprBRKT(std::vector<LuaLexFrame> *Keys, uint32_t *pos, lua_Scope *S, std::vector<std::string> *TR, std::unordered_map<std::string, uint32_t> *hv);
extern std::vector<LuaLexFrame> lua_AcquireNassembleLuaPath(std::vector<LuaLexFrame> *K, uint32_t *pos, lua_Scope *S, std::vector<std::string> *TR, std::unordered_map<std::string, uint32_t> *hv);

extern std::string luaLexFrameKeyToString(_Lua_Lex_Keys k);

extern std::vector<LuaLexFrame> analizeNupdateConstantsNvars(std::vector<LuaLexFrame> *Keys);

//BEGIN EVAL


enum _LUA_XMM_REGISTERS: uint8_t {
	xmmU = UINT8_MAX,
	xmm0 = 0,
	xmm1 = 1,
	xmm2 = 2,
	xmm3 = 3,
};
#include <sys/ucontext.h>

struct _REGISTER_;
struct _XREGISTER_;
typedef void(*_regCallback)(asmjit::x86::Assembler *a, _REGISTER_ *reg);
typedef void(*_regCallbackXMM)(asmjit::x86::Assembler *a, _XREGISTER_ *reg);

enum _R_CONTENTS: uint8_t {
	_R_TRASHDATA = 0,
	_R_FUNC_RESULT = 1,
	_R_AR_RESULT = 4,
	_R_CMP_RESULT = 5,
	_R_FUNC_ARGS = 6,
	_R_FUNC_ARGS_ENTRY = 7,
	_R_CLUATYPE_TAGGED = 2,
	_R_CLUATYPE_UNTAGGED = 3,
	_R_TABLE_POINTER = 8,
};
struct _REGISTER_ {
	greg_t rID = REG_ERR;
	Values regVal = 0x0;
	uint64_t rData0;
	int32_t stackPtrBase = 0;
	LuaType valType; // Untagged value should be in regVal [On practical zone, theorical=0]
	_R_CONTENTS cntId = _R_TRASHDATA;
	_regCallback onModified;
};
struct _XREGISTER_ {
	Values regDataType = 0x0;
	_LUA_XMM_REGISTERS rID = xmmU;
	_R_CONTENTS cntId = _R_TRASHDATA;
	int32_t stackPtrBase = 0;
	void *rData0 = nullptr;
	_regCallbackXMM onModified;
};
typedef std::unordered_map<greg_t, _REGISTER_> RegistersDataGP;
typedef std::unordered_map<_LUA_XMM_REGISTERS, _XREGISTER_> RegistersDataXMM;
extern RegistersDataGP lua_Registers;
extern RegistersDataXMM lua_RegistersXMM;
extern void initializeRegistersData(void *asmPtr);

//END EVAL

#include <cstring>

// Lua <Values> manipulator

// I did extract some parts of LuaJIT for our use, so. Thank you LuaJIT developers!!!
// No need to be mad or angry at me, i just want some multithreading in my game.
static constexpr uint64_t NAN_MASK = 0x7FF0000000000000ULL;
static constexpr uint64_t TAG_MASK = 0x000F000000000000ULL;
static constexpr uint64_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;
static constexpr uint64_t NAN_BASE = 0x7FF0000000000000ULL;

// WRITERS

inline Values lua_makeVar(void *ptr, LuaType T) {
	return NAN_BASE | (T << 48) | (uintptr_t)ptr;
}

inline Values lua_aMakeVarDouble(double k) {
	uint64_t _0;
	memcpy(&_0, &k, 8);
	return _0;
}

inline LuaType lua_getVarType(Values o) {
	return LuaType((o >> 48) & 0xF);
}

inline void *lua_getPtr(Values k) {
	return (void*)(k & PTR_MASK);
}

inline bool lua_isNum(Values k) {
	return (k & NAN_MASK) != NAN_MASK;
}

extern LuaType __LEX_KEY_TO_LuaType(_Lua_Lex_Keys a, uint8_t ATTR);

void lua_initializeRuntime();
//Environment on blocks
/*
 * New proposal for values management on the internal
 */

// Some global variables, this ensures NO multithreading in loading scripts.
// but thats not the reason, the main reason is i do not want to flood the compiler's function arguments
extern bool _0_0_0_CMPTIME_ASM_isScript;
extern void *_0_0_0_CMPTIME_ASM_scriptMem;
extern int32_t _0_0_0_CMPTIME_ASM_localStackFrameBytes;

asmjit::x86::Gp CLUA_EvalExprNReturn(std::vector<LuaLexFrame> *k, lua_Scope *scope, std::pair<bool, asmjit::x86::Gp> saveSpecificallyTo, bool getPointerInsteadofRawD = false, bool noTag = false, std::pair<uint32_t*, _Lua_Lex_Keys> middleCheck = {0, _L_NONE});
asmjit::x86::Gp _ASM__getPathToSelGp(std::vector<LuaLexFrame> *vct, asmjit::x86::Gp ret, lua_Scope *aSCP, bool pointer = false, bool preservRegister = false);
asmjit::x86::Gp _ASM__keyInstRestoreVar(asmjit::x86::Gp toVar);
void _F_ASM_MAKEFUNCTIONARGUMENTS(lua_Expression *Args, asmjit::x86::Assembler *a, lua_Scope *AS, bool give_stackptr, uint32_t stackptrsiz);
void _ASM__movToReg(asmjit::x86::Gp cR, asmjit::x86::Gp b);
extern asmjit::StringLogger qlog0;
extern int32_t stackRegCounter; // Starts from -40.
greg_t _CPP_getRegisterFromASM(asmjit::x86::Gp reg);
void _ASM_DEBUGGER_STOP();
void _lua_Table__initializeAssembler(asmjit::x86::Assembler *ptr);
LuaLexFrame getExprValue(std::vector<LuaLexFrame> *k, uint32_t *pos, lua_Scope *scope = nullptr, bool _returnEmptyIfVariables = false);
void _ASMH__rs_searchInTable(asmjit::x86::Gp tblPTR, std::pair<bool, std::pair<asmjit::x86::Gp, TString*>> key, asmjit::x86::Gp toGp, bool pointer = false);
void _HELPER__runHooksFor(asmjit::Reg rId_, _R_CONTENTS id);
inline void __ASM_callback_nothing_(asmjit::x86::Assembler *a, _REGISTER_ *reg) {}
inline void __ASM_callback_nothingX_(asmjit::x86::Assembler *a, _XREGISTER_ *reg) {}
asmjit::x86::Gp _ASMH__parseVarCacheRef(uint8_t r);
lua_localSymbol *searchSavedGeneralVars(const std::string id);
_LUA_XMM_REGISTERS _CPP_getXMMfromASM(asmjit::Reg rId);





















