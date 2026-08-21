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

#include <vector>

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

typedef FuncArgs*(*FunctionPointer)(Values, Values, FuncArgs*); // rdi, rsi, rdx

struct TString {
	uint64_t IDX;
	uint64_t len;
	char *data;
	uint64_t _unocc = 0;
};

struct Node {
	TString *key = nullptr;
	Values val = 0;
	Node *next = 0;
	uint64_t f;
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

enum _lua_IndexMethod {
	_lua_idxm_nil, // Should throw exception :)
	_lua_idxm_string,
	_lua_idxm_integer,
};

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
	_lua_es_oversizedSave = 19,
	_lua_es_unknownFile = 20,
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

class Lua {
public:
	Lua() = default;
	LuaErrorHandler *m_LuaErrorHandler = nullptr;
	lua_Table *m_General = nullptr; // Might be _G
};
// Lexical reader

extern const std::string _LuaKeysString[85];

struct lua_biOpCode;
struct lua_AddrPath;

#include <asmjit/core.h>
#include <asmjit/x86.h>
struct lua_localSymbol {
	int64_t slot = 0;
	uint8_t qID = 0;
	uint8_t cacheReg = 0;
	asmjit::x86::Gp register_ = asmjit::x86::rax;
	std::string id = "";
	LuaType type = LuaUnknown;
	uint64_t rawdata = 0;
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



using tStringTable = std::unordered_map<std::string, TString>;

extern LuaErrorHandler *m_LuaErrorHandler;
extern lua_Table *m_General;
extern uint32_t luaCurrentFileId;
extern tStringTable stringTable;

//bundleFunction()
//FunctionPointer luaBundleFunction(std::vector<LuaLexFrame> *Keys, lua_Table *ENV, uint32_t _LINES, uint32_t &pos);
//Tstring manipulation
extern TString *returnCompiledString(std::string N);
extern TString returnIndexOfString(uint32_t i);
extern TString *returnIndexOfStringPTR(uint32_t i);
extern void removeStringId(uint32_t I);
extern std::vector<TString> _L_L_C_S;
extern std::vector<uint32_t> _L_L_C_S_GC;

extern uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

//BEGIN EVAL


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
























