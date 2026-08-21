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
#include "cllex.hpp"
#include "cljit.hpp"

lua_Scope::lua_Scope() {
}

LuaErrorHandler *m_LuaErrorHandler = nullptr;
uint32_t luaCurrentFileId = 0;
lua_Table *m_General = nullptr;
std::unordered_map<std::string, TString> stringTable = std::unordered_map<std::string, TString>();
uint32_t luaSeed;

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

#define cstr __cache2.clear();

// Improve line errors/warnings
std::vector<std::string> _starter00 {
	"Not a function",
	"Value is nil",
	"Not correct",
	"Arguments",
	"Bad string",
	"Unknown",
		"Unknown data",
	"Invalid usage",
	"Bad syntax",
	"Bad variable name",
	"Not a table",
	"Invalid type",
	"Too few instructions set",
		"Future crash at runtime",
	"Bad typing",
	"Not a integer",
	"Not a string",
	"Unknown error",
	"Illegal instruction",
	"Call to a oversized value",
	"Unknown file to open",
};

const std::string getLineError(lua_ErrSignals sign) {
	try {
		return _starter00.at(sign);
	} catch (std::out_of_range &e) {
		return std::string("Internal Error");
	}
}

// ERRORS REPORTER
void LuaErrorHandler::reportError(const lua_ErrSignals signal, const size_t funcid, std::string reason) {
	// Just errors.
	if (!m_pipe->nomutex) {
		m_pipe->mtx.lock();
	}
	
	std::string ErrString = "[ERROR] ";
	
	ErrString.append(getLineError(signal));
	ErrString.append(": ");
	
	if (funcid != 0) {
		ErrString.append(" At line: ");
		ErrString.append(((std::string*)funcid)->c_str());
		ErrString.append(": ");
	}
	
	ErrString.append(reason);
	
	m_pipe->reason = ErrString;
	m_pipe->err = true;
	
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



#include <random>
#include <fstream>
#include <iterator>
#include "clibInit.hpp"
#include "clobject.hpp"
/*
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
	m_General->hsize = m_General->hmask;
	m_LuaErrorHandler = leh;
	std::cout << "Initializing libraries" << std::endl;
	LIBC__InitializeStreamLibrary(m_General);
	LIBC__InitializeStringLibrary(m_General);
	LIBC__InitializeDebugKit(m_General);
	_TEST__registerObject(m_General); //TEST
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
*/

