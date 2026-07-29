/*
** $Id: lapi.c,v 2.55.1.5 2008/07/04 18:41:18 roberto Exp $
** Lua API
** See Copyright Notice in lua.h
*/


#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <math.h>
#include <vector>
#include <stdarg.h>
#include <string.h>
#include "lua.hpp"
#include <stdexcept>
#include <memory>
#include <iostream>

lua_Scope::lua_Scope() {
	std::cout << "SCOPE CONSTRUCT" << std::endl;
}


std::vector<std::vector<std::string>> __LINES = std::vector<std::vector<std::string>>();
LuaErrorHandler *m_LuaErrorHandler = nullptr;
uint32_t luaCurrentFileId = 0;
lua_Table *m_General = nullptr;
std::unordered_map<std::string, TString> stringTable = std::unordered_map<std::string, TString>();
uint32_t luaSeed;

const char lua_ident[] =
  "$Lua: " LUA_RELEASE " " LUA_COPYRIGHT " $\n"
  "$Authors: " LUA_AUTHORS " $\n"
  "$URL: www.lua.org $\n";

// NOTE: The entire script that we load "*.lua" files must be loaded and then compiled to AsmJIT for faster work.

#include <string_view>

/*
*	Basic functions for VM management
*/

void luaErr(lua_ErrSignals sgn, uint32_t file, uint32_t line) {
	m_LuaErrorHandler->reportError(sgn, 9898986555, "Lua error at: " + getLine(line, file));
	m_LuaErrorHandler->setFatal(true);
}

#include <sstream>
std::string luaVarInfoToString(lua_VarOnMemInfo data) {
	std::stringstream a;
	a << std::hex << data.Addr;
	std::string typo = "null";
	switch (data.type) {
		case (LuaInteger): typo="int";
		case (LuaString): typo="string";
		case (LuaFunction): typo="function";
		case (LuaObject): typo="object";
		case (LuaTable): typo="table";
		case (LuaNil): typo="nil";
		case (LuaBoolean): typo="boolean";
	}
	return std::string(data.varname + "; MemPos: " + std::string(a.str()) + "; LuaType: " + typo);
}

std::string getLuaTypeString(LuaType TYP) {
	std::string typo = "null";
	switch (TYP) {
		case (LuaInteger): typo="int";
		case (LuaString): typo="string";
		case (LuaFunction): typo="function";
		case (LuaObject): typo="object";
		case (LuaTable): typo="table";
		case (LuaNil): typo="nil";
		case (LuaBoolean): typo="boolean";
	}
	return typo;
}

//BEGIN TSTRING
// All strings, everything, should be collapsed into one big system. No hashes, no comparations, just numbers. Index.
//Compile string
std::vector<TString> _L_L_C_S;
std::vector<uint32_t> _L_L_C_S_GC;
TString *returnCompiledString(std::string N) {
	// stringTable
	uint32_t i;
	TString *OBJ;
	try {
		OBJ = &stringTable.at(N);
		if (OBJ->_unocc) {
			goto _L_R_C_S_C;
		}
	} catch (std::out_of_range &e) {
		// Not found, go to creation line.
		goto _L_R_C_S_C;
	}
	// Found.
	return OBJ;
	_L_R_C_S_C:
	i = stringTable.size()+1;
	
	if (!_L_L_C_S_GC.empty()) {
		i = _L_L_C_S_GC.back();
		_L_L_C_S_GC.pop_back();
	}
	
	OBJ = new TString();
	OBJ->IDX = murmur3_32(N.data(), N.size(), luaSeed);
	OBJ->data = new char[N.size()+1];
	memcpy(OBJ->data, N.data(), N.size());
	OBJ->len = N.size();
	OBJ->_unocc = false;
	stringTable[N] = *OBJ; // Compiled.
	/// ALLOCATE TO _L_L_C_S
	_L_L_C_S.resize(i);
	_L_L_C_S[i-1] = *OBJ;
	return OBJ;
}
TString returnIndexOfString(uint32_t i) {
	TString obj;
	try {
		obj = _L_L_C_S.at(i);
	} catch (std::out_of_range &e) {
		return obj;
	}
	return obj;
}
TString *returnIndexOfStringPTR(uint32_t i) {
	TString obj;
	try {
		obj = _L_L_C_S.at(i);
	} catch (std::out_of_range &e) {
		return &_L_L_C_S.at(i);
	}
	return &_L_L_C_S.at(i);
}
void removeStringId(uint32_t I) { // For cache datas
	try {
		TString *a = &_L_L_C_S.at(I);
		a->_unocc = true;
		_L_L_C_S_GC.push_back(I);
	} catch (std::out_of_range &e) {
		// HUH
	}
}
//END TSTRING

//BEGIN TABLE_MANIPULATOR
// Build table from LuaLexFrame*, should start from _L_TABLE_START, each index or object inside the table should be online builded.
lua_Table *buildTableFromKeys(std::vector<LuaLexFrame> *Keys, uint32_t &pos) {
	lua_Table *TBL = new lua_Table();
	return TBL;
}
//END TABLE_MANIPULATOR

/*
*	Functions Management
* um, might there must be a raw string with a fixed size which should allow store the function name
*/

// Welp. I thinked this would get even bigger than normal lua, but it ins't.
// That's sad.
/*
	THIS LUA COPY WILL BE CALLED cLua as it works like a compiler. (I just made this for my luanti game)
	The syntax will be the common, just with few advantages as multithread support
	mutex will be available too.
	
	This don't work with stack base, this works with tree-walk engine based, so, enjoy using lua_Table* as lua_Stack*
*/

// Table/State manipulation

// LuaTableBase //
/*
LuaTableBase::LuaTableBase(Lua *obj) {
	m_LuaErrorHandler = obj->m_LuaErrorHandler;
	m_Lua = obj;
}

// Returns a index-INT value [Copy]
Values LuaTableBase::getValueIDX(uint32_t val) {
	// val are a address
	// This returns a copy.
	return *_Base2.at(val);
}

// Returns a index-INT value
Values *LuaTableBase::getRawAddrValueIDX(uint32_t val) {
	if (_Base2.size() <= val) {
		return _Base2[val];
	}
	return nullptr;
}


// Returns a index-defined value
Values *LuaTableBase::getValueAddr(size_t val) {
	Values* data = new Values();
	data->TYPO = LuaNil;
	try {
		data = _Base.at(val);
	} catch (std::out_of_range &e) {
		// Oops!
		std::string err_info = "";
		try {
			lua_VarOnMemInfo addr_inf = *AddrToInfo.at(val);
			err_info = luaVarInfoToString(addr_inf);
		} catch (std::out_of_range &e) {
			err_info = "<Unable to get info> [Var: " + std::to_string(val) + "]";
			// Ignore. It is sad to find a unknown value somewhere
		}
		m_LuaErrorHandler->reportWarning(_lua_es_UnknownDataIdx, 0, std::string("Value are: " + err_info));
		// Save the value to an table (Link)
		_Base[val] = data;
		return data;
	}
	return data;
}
// Returns a index-defined value
Values *LuaTableBase::getValueAddrIgnWarn(size_t val) {
	Values* data = new Values();
	data->TYPO = LuaNil;
	try {
		data = _Base.at(val);
	} catch (std::out_of_range &e) {
		return data;
	}
	return data;
}

// It's the true, its the true, it's the trie
// kinda love.........

//For direct asm
void setValueHashed(LuaTableBase *base, const size_t hash, FuncArgs *Val) {
	Values *var = new Values();
	var->TYPO = Val->data[0].TYPO;
	var->val0 = Val->data[0].val0;
	var->val1 = Val->data[0].val1;
	var->val2 = Val->data[0].val2;
	var->val3 = Val->data[0].val3;
	var->val4 = Val->data[0].val4;
	var->val5 = Val->data[0].val5;
	base->setValue(lua_AddrSpec(_last_Hashed, hash), var);
}
void setValueInteger(LuaTableBase *base, const size_t int_, FuncArgs *Val) {
	// New var
	Values *var = new Values();
	var->TYPO = Val->data[0].TYPO;
	var->val0 = Val->data[0].val0;
	var->val1 = Val->data[0].val1;
	var->val2 = Val->data[0].val2;
	var->val3 = Val->data[0].val3;
	var->val4 = Val->data[0].val4;
	var->val5 = Val->data[0].val5;
	base->setValue(lua_AddrSpec(_last_Integer, int_), var);
}

void LuaTableBase::setValue(lua_AddrSpec pos, Values *val) {
	if (pos.Type == _last_Hashed) { // It might look like this on lua: __n1["duh"] = nil; && __n1.duh = nil;
		// TO OBJ: _Base
		_Base.insert(std::pair<size_t, Values*>(pos.Addr, val));
		if (!pos.Info._name.empty()) {
			// Debug
			lua_VarOnMemInfo *vomi = new lua_VarOnMemInfo(pos.Info._name, pos.Addr, val->TYPO);
			linkAddrToInfo(pos.Addr, vomi);
		}
	} else if (pos.Type == _last_Integer) {
		_Base2[pos.Addr2] = val;
		if (!pos.Info._name.empty()) {
			// Debug
			lua_VarOnMemInfo *vomi = new lua_VarOnMemInfo(pos.Info._name, pos.Addr, val->TYPO);
			linkAddrToInfo(pos.Addr, vomi);
		}
	}
}

// Table clearing

void LuaTableBase::clearTable() {
	uint32_t idx = 0;
	for (Values *OBJ : _Base2) {
		// Clear the vector
		m_Lua->setNil(this, lua_IndexMethod(_lua_idxm_integer, -1, idx));
		idx++;
	}
	// Clear the raw table
	for (auto it = _Base.begin(); it != _Base.end(); it++) {
		m_Lua->setNil(this, lua_IndexMethod(_lua_idxm_string, it->first, 0));
	}
}

// Special for debug

void LuaTableBase::linkAddrToInfo(size_t addr, lua_VarOnMemInfo *L_vomi) {
	AddrToInfo.insert(std::pair<size_t, lua_VarOnMemInfo*>(addr, L_vomi));
}
*/

bool LuaLex::areNumber(uint8_t data) {
	switch (data) {
		case (0): {
			return true;
		}
		case (1): {
			return true;
		}
		case (2): {
			return true;
		}
		case (3): {
			return true;
		}
		case (4): {
			return true;
		}
		case (5): {
			return true;
		}
		case (6): {
			return true;
		}
		case (7): {
			return true;
		}
		case (8): {
			return true;
		}
		case (9): {
			return true;
		}
	}
	return false;
}

std::vector<uint8_t> LuaLex::EndOfLineOfNumberOrVar(std::vector<uint8_t> *data, uint32_t pos) {
	// Start checking
	bool cancel_ = false;
	uint32_t pos_ = pos;
	for (std::vector<uint8_t>::iterator it = data->begin()+pos; it != data->end(); it++) {
		//Skip the spaces, otherwise, if the number are already done, then cancel
		if ((*it == ' ') && !cancel_)
			goto EndPoint;
		if ((*it == '0') || (*it == '1') || (*it == '2') || (*it == '3') || (*it == '4') || (*it == '5') || (*it == '6') || (*it == '7') || (*it == '8') || (*it == '9')) {
			// This could be an decimal or hexadecimal value
			// So, let's see if theres an 'x'
			if (data->at(pos_+1) == 'x') {
				//Hexadecimal, idk how to make this

			}
		}
		EndPoint:
		pos_++;
	}
}

#define cstr __cache2.clear();
#include <sstream>


// This piece of shit fills you with determination

// ERRORS REPORTER
void LuaErrorHandler::reportError(const lua_ErrSignals signal, const size_t funcid, std::string reason) {
	// Just errors.
	if (!m_pipe->nomutex) {
		m_pipe->mtx.lock();
	}
	if (funcid != 9898986555 || funcid != 0) { //Please be uniqueeee
		switch (signal) {
			case (_lua_es_NonFunction): {
				std::string err = "[ERROR] Not a function: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_ValueIsNil): {
				std::string err = "[ERROR] Value are nil: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_NotCorrect): {
				std::string err = "[ERROR] Not a correct way to do: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_ArgIsProblem): {
				std::string err = "[ERROR] Verify arguments: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_BadString): {
				std::string err = "[ERROR] Bad string: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_WhatTheHell): {
				std::string err = "[ERROR] ... what?: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_InvalidUsage): {
				std::string err = "[ERROR] Invalid usage: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_BadSyntax): {
				std::string err = "[ERROR] Syntax error!: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_BadVariableNamingMethod): {
				std::string err = "[ERROR] Bad naming: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			case (_lua_es_NotTable): {
				std::string err = "[ERROR] Not a table: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			}
			case (_lua_es_InvalidType): {
				std::string err = "[ERROR] Invalid type: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			}
			case (_lua_es_TooSmallEntry): {
				std::string err = "[ERROR] Invalid code.";
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			}
			//Warning. Ignore.
			case (_lua_es_UnknownDataIdx): {
				break;
			};
			default: {
				std::string err = "[ERROR] <No exec code>: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			}
		}
	}
	std::cout << "\033[3;31mCOMPILATION ERR: " << m_pipe->reason << "\033[0m" << std::endl;
	//__asm__("ud2");
	if (!m_pipe->nomutex) {
		m_pipe->mtx.unlock();
	}
}

void LuaErrorHandler::reportWarning(const lua_ErrSignals signal, const size_t funcid, std::string reason) {
	// Just errors.
	if (!m_pipe->nomutex) {
		m_pipe->mtx.lock();
	}
	if (funcid != 9898986555 || funcid != 0) { //Please be uniqueeee
		switch (signal) {
			case (_lua_es_NonFunction): {
				break;
			};
			case (_lua_es_ValueIsNil): {
				break;
			};
			case (_lua_es_NotCorrect): {
				break;
			};
			case (_lua_es_ArgIsProblem): {
				break;
			};
			case (_lua_es_BadString): {
				break;
			};
			case (_lua_es_WhatTheHell): {
				break;
			};
			case (_lua_es_InvalidUsage): {
				break;
			};
			case (_lua_es_BadSyntax): {
				break;
			};
			case (_lua_es_BadVariableNamingMethod): {
				break;
			};
			//Warning. Ignore.
			case (_lua_es_UnknownDataIdx): {
				std::string err = "[WARNING] Bad data, " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = true;
				break;
			};
			default: {
				std::string err = "[WARNING] ToDebug<NoReasonToPrintThis>: " + reason;
				m_pipe->fatal = false;
				m_pipe->reason = err;
				m_pipe->err = false;
				break;
			}
		}
	}
	std::cout << "\033[3;33m" << m_pipe->reason << "\033[0m" << std::endl;
	if (!m_pipe->nomutex) {
		m_pipe->mtx.unlock();
	}
}

std::string toHex(uint64_t bytes) {
	std::stringstream ss;
	ss << std::hex;
	ss << bytes;
	ss << std::dec;
	return ss.str();
}

//BEGIN LUAFUNC
static Values PRINT(Values RDI, Values RSI, FuncArgs *ARGS) { // Classic start.
	TString *ptr = (TString*)lua_getPtr(RDI);
	std::cout << std::string(ptr->data, ptr->len) << '\n';
	return 0;
}
int64_t clua_cCalcTimeMS() {
	struct timespec now;
	// Use TIME_UTC to measure time since the Epoch
	timespec_get(&now, TIME_UTC); 
	// Convert seconds to milliseconds and add milliseconds from nanoseconds
	return ((int64_t)now.tv_sec) * 1000 + ((int64_t)now.tv_nsec) / 1000000;
}
static int64_t start = 0;
static bool started = false;
static void calcTime() {
	if (!started) {
		start = clua_cCalcTimeMS();
		started = !started;
	} else  {
		std::cout << "Records are: " << std::to_string(clua_cCalcTimeMS() - start) << std::endl;
		started = false;
	}
}
static FuncArgs *RT(FuncArgs *_) {
	FuncArgs *a = new FuncArgs[2];
	a[0] = (uint64_t)0x1;
	a[1] = (uint64_t)lua_makeVar(returnCompiledString("PUTO"), LuaString);
	return a;
}
static Values TOSTRING(Values RDI, Values RSI, FuncArgs *ARGS) {
	std::stringstream a;
	a << std::hex;
	a << (uint64_t)RDI;
	//__asm__ ( "ud2" );
	return lua_makeVar(returnCompiledString(a.str()), LuaString);
}
static FuncArgs *EXIT(FuncArgs *_) {
	exit(0);
	free(_);
	return nullptr;
}
static Values ud2(Values RDI, Values RSI, FuncArgs *ARGS) {
	__asm__ ("ud2");
}
//END LUAFUNC

#ifndef DO_MAIN_FUNC

#include <random>
#include <fstream>
#include <iterator>

int main(int argc, char* argv[]) {
	// Main
	std::cout << "C L U A" << std::endl;
	std::cout << "Initializing cLua..." << std::endl;
	Lua *obj = new Lua();
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> dist(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
	luaSeed = dist(gen);
	std::cout << "Initializing fifo pipe for warnings and errors" << std::endl;
	lua_ErrHandler fifo = lua_ErrHandler();
	fifo.nomutex = true; // Don't lock if theres no other thread managing this.
	std::cout << "Initializing engine for warnings and errors" << std::endl;
	LuaErrorHandler *leh = new LuaErrorHandler(&fifo);
	std::cout << "Initializing cLua Lexer..." << std::endl;
	LuaLex *lex = new LuaLex(obj, leh);
	std::cout << "Linking pipes" << std::endl;
	obj->m_LuaErrorHandler = leh;
	lua_Table *TBL = new lua_Table();
	obj->m_General = TBL;
	m_General = TBL;
	m_General->nodes = (Node*)malloc(0xFFFF*sizeof(Node));//new Node[0xFFFF];
	memset((void*)m_General->nodes, 0, 0xFFFF*sizeof(Node));
	m_General->hmask = 0xFFFF;
	m_LuaErrorHandler = leh;
	TString *_PRINTFUNC = returnCompiledString("print");
	TString *_TOSTRINGFUNC = returnCompiledString("lel");
	TString *_TOSTRING = returnCompiledString("tostring");
	Values print;
	print = lua_makeVar(((void*)PRINT), LuaFunction);
	Values lel;
	lel = lua_makeVar(((void*)RT), LuaFunction);
	//std::cout << std::hex << ((uint64_t)PRINT) << std::dec<< std::endl;
	//std::cout << std::hex << print << std::dec << std::endl;
	//std::cout << (lua_getVarType(print)) << std::endl;
	//Values tostring;
	//tostring.type = LuaFunction;
	//tostring.func = (FunctionPointer)PRINT;
	//TBL->strMap[_TOSTRINGFUNC] = tostring;
	_F_ASM_NOTGUARANTEED_SETVALUE(m_General, _PRINTFUNC, print);
	_F_ASM_NOTGUARANTEED_SETVALUE(m_General, _TOSTRINGFUNC, lel);
	_F_ASM_NOTGUARANTEED_SETVALUE(m_General, returnCompiledString("tostring"), lua_makeVar(((void*)TOSTRING), LuaFunction));
	_F_ASM_NOTGUARANTEED_SETVALUE(m_General, returnCompiledString("test"), lua_makeVar((void*)returnCompiledString("nigger "), LuaString));
	_F_ASM_NOTGUARANTEED_SETVALUE(m_General, returnCompiledString("exit"), lua_makeVar(((void*)EXIT), LuaFunction));
	_F_ASM_NOTGUARANTEED_SETVALUE(m_General, returnCompiledString("CC"), lua_makeVar(((void*)calcTime), LuaFunction));
	_F_ASM_NOTGUARANTEED_SETVALUE(m_General, returnCompiledString("ud2"), lua_makeVar(((void*)ud2), LuaFunction));
	//_F_ASM_NOTGUARANTEED_SETVALUE(m_General, returnCompiledString("var0"), lua_makeVar(returnCompiledString("pingas"), LuaString));
	if (argc > 1) {
		// Proc files.
		std::ifstream inputFile(argv[1]);
		if (!inputFile.is_open()) { // The ! operator or .is_open() can be used for checking
			std::cout << "\033[1;33m" << "Error opening file: " << std::string(argv[1]) << "\033[0m" << std::endl;
			return 1; // Return an error code
		}
		std::string content((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
		//std::vector<char> buffer((std::istream_iterator<char>(inputFile)), std::istream_iterator<char>());
		std::vector<std::string> _strV = lex->ParserFirstStage(std::vector<uint8_t>(content.begin(), content.end()));
		if (fifo.err) {
			std::cout << "\033[1;33m" << fifo.reason << "\033[0m" << std::endl;
			fifo.err = false;
			return 1;
		}
		std::vector<LuaLexFrame> _lFrmV = lex->ParserSecondStage(_strV);
		if (fifo.err) {
			std::cout << "\033[1;31m" << fifo.reason << "\033[0m" << std::endl;
			fifo.err = false;
			return 1;
		}
		std::cout << "Keys: " << _lFrmV.size() << " ;;;";
		std::string _c = lex->dumpInfo(_lFrmV);
		std::cout << _c << std::endl;
		// Begin executed
		std::vector<LuaLexFrame> j = lex->ParserThirdStage(_lFrmV);
		if (fifo.err) {
			std::cout << "\033[3;31m" << fifo.reason << "\033[0m" << std::endl;
			fifo.err = false;
			return 1;
		}
		uint32_t pos = 0;
		lua_Scope *n = new lua_Scope();
		std::vector<lua_biOpCode> k = lua_B_F_OP(&j, &pos, n, false, false);
		if (fifo.err) {
			std::cout << "\033[3;31mCompilation 1st Lvl: " << fifo.reason << "\033[0m" << std::endl;
			fifo.err = false;
			return 1;
		}
		void *F = luaBundleFunction(&k, n, false, nullptr, nullptr, true);
		if (fifo.err) {
			std::cout << "\033[3;31mCompilation 2nd Lvl: " << fifo.reason << "\033[0m" << std::endl;
			fifo.err = false;
			return 1;
		}
		((FunctionPointer)F)(0x0,0x0,nullptr);
		if (fifo.err) {
			std::cout << "\033[1;31mRuntime Err: " << fifo.reason << "\033[0m" << std::endl;
			fifo.err = false;
			return 1;
		}
		return 0;
	}
	while (true) {
		std::string user_input;
		std::cout << "\033[1;37mcLua>\033[0m ";
		std::getline(std::cin, user_input);
		// If user inserted something, check if it aren't empty
		if (user_input != "" || user_input != " " || user_input != "\n") {
			std::vector<std::string> _a = lex->ParserFirstStage(std::vector<uint8_t>(user_input.begin(), user_input.end()));
			//Check errors
			if (fifo.err) {
				std::cout << "\033[1;33m" << fifo.reason << "\033[0m" << std::endl;
				fifo.err = false;
				continue;
			}
			std::vector<LuaLexFrame> _b = lex->ParserSecondStage(_a);
			if (fifo.err) {
				std::cout << "\033[1;31m" << fifo.reason << "\033[0m" << std::endl;
				fifo.err = false;
				continue;
			}
			std::cout << "Keys: " << _b.size() << " ;;;";
			std::string _c = lex->dumpInfo(_b);
			std::cout << _c << std::endl;
			// Begin executed
			std::vector<LuaLexFrame> j = lex->ParserThirdStage(_b);
			if (fifo.err) {
				std::cout << "\033[3;31m" << fifo.reason << "\033[0m" << std::endl;
				fifo.err = false;
				continue;
			}
			//Now, do smth
			uint32_t pos = 0;
			lua_Scope *n = new lua_Scope();
			std::vector<lua_biOpCode> k = lua_B_F_OP(&j, &pos, n, false, false);
			if (fifo.err) {
				std::cout << "\033[3;31mCompilation 1st Lvl: " << fifo.reason << "\033[0m" << std::endl;
				fifo.err = false;
				continue;
			}
			void *F = luaBundleFunction(&k, n, false, nullptr, nullptr, true);
			if (fifo.err) {
				std::cout << "\033[3;31mCompilation 2nd Lvl: " << fifo.reason << "\033[0m" << std::endl;
				fifo.err = false;
				continue;
			}
			((FunctionPointer)F)(0x0,0x0,nullptr);
			if (fifo.err) {
				std::cout << "\033[1;31mRuntime Err: " << fifo.reason << "\033[0m" << std::endl;
				fifo.err = false;
				continue;
			}
			continue;
		} else {
			std::cout << "Not valid data!" << std::endl;
			continue;
		}
	}
}

#endif


