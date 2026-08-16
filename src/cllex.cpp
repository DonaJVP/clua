#include "lua.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

static bool _is_numb(uint8_t p) {
	switch (p) {
		case '1': return true;
		case '2': return true;
		case '3': return true;
		case '4': return true;
		case '5': return true;
		case '6': return true;
		case '7': return true;
		case '8': return true;
		case '9': return true;
		case '0': return true;
	}
	return false;
}

static std::pair<bool, bool> _chars_is_numb(std::string numb) {
	bool _done = false;
	bool _dot = false;
	for (const char &i: numb) {
		switch (i) {
			case '1': {_done = true; break;};
			case '2': {_done = true; break;};
			case '3': {_done = true; break;};
			case '4': {_done = true; break;};
			case '5': {_done = true; break;};
			case '6': {_done = true; break;};
			case '7': {_done = true; break;};
			case '8': {_done = true; break;};
			case '9': {_done = true; break;};
			case '0': {_done = true; break;};
			case '.': { if (!_dot) _dot = true; else { _done = false; break; } break; };
			default: {
				if (!_done) {
					return {_done, _dot};
				}
				_done = false; 
				break;
			};
		}
	}
	return {_done, _dot};
}

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

// Reference: ASCII chart 'hexadecimal'
bool _isOperatorByte(uint8_t byte) {
	if (byte == 0xA) {
		return true; // New line
	}
	if (byte == 0x20 || byte >= 0x22 && byte < 0x30) {
		// First sector
		return true;
	}
	if (byte >= 0x3A && byte < 0x3F) {
		// Second sector
		return true;
	}
	if (byte >= 0x5B && byte < 0x5F) {
		// Third sector
		return true;
	}
	if (byte >= 0x7B && byte <= 0x7F) {
		// Fourth sector
		return true;
	}
	return false;
}

bool _compatibleToSumBytes(uint8_t fOp, uint8_t sOp) {
	switch (fOp) {
		case 0x3D: {
			switch (sOp) {
				case 0x3C: {
					return true;
				}
				case 0x3E: {
					return true;
				}
				case 0x7E: {
					return true;
				}
				case 0x3D: {
					return true;
				}
				default: {
					return false;
				}
			}
			break;
		}
		case 0x2E: {
			if (sOp != fOp) {
				return false;
			} else {
				return true;
			}
			break;
		}
		case 0x2B: {
			if (sOp != fOp) {
				return false;
			} else {
				return true;
			}
			break;
		}
		case 0x2D: {
			if (sOp != fOp) {
				return false;
			} else {
				return true;
			}
			break;
		}
		default: {
			// Do not let two bytes smash together.
			return false;
		}
	}
}

uint8_t _seekNextByte(std::vector<uint8_t> &data, uint64_t *pos) {
	uint8_t BT = 0;
	try {
		BT = data.at(*pos + 1);
	} catch(std::out_of_range &e) {
		return 0;
	}
	return BT;
}

std::vector<std::string> LuaLex::ParserFirstStage(std::vector<uint8_t> data) {
	// First stage, just separate by <operators: = ! ( ) { } [ ] / * + - < > " ' | %>
	std::vector<std::string> blocks;
	std::string cache;
	uint8_t _last_char = 0;
	// This has no brain, yet.
	uint8_t byte = 0;
	uint8_t lbyte = 0;
	uint64_t pos = 0;
	uint8_t pointer_COUNT = 0;
	uint8_t occupied = 0;
	std::pair<uint8_t, uint8_t> opBytes;
	for (;;) {
		try {
			byte = data.at(pos);
		} catch (std::out_of_range &e) {
			break;
		}
		if (byte == 0x9) {
			byte = 0x20;
		}
		// First, identify it as a number.
		// lbyte is optional, but next number.
		if (byte == 0x2E) {
			if (_is_numb(_seekNextByte(data, &pos))) {
				// A floating value
				cache.push_back(byte);
				pos++;
				continue;
			}
		}
		if (_isOperatorByte(byte)) {
			if (occupied == 0) {
				// If theres data at cache then push it.
				if (!cache.empty()) {
					blocks.push_back(cache);
					cache.clear();
				}
				opBytes.first = byte;
				occupied++;
			} else {
				// The audacity of putting two plus one operator byte must be studied.
				if (occupied == 2) {
					std::string op;
					op.push_back(opBytes.first);
					if (occupied > 1) {
						op.push_back(opBytes.second);
					}
					blocks.push_back(op);
					occupied = false;
					std::string opNew;
					opNew.push_back(byte);
					// Insert cache if possible.
					if (!cache.empty()) {
						blocks.push_back(cache);
						cache.clear();
					}
					blocks.push_back(opNew);
					pos++;
					continue;
				}
				if (_compatibleToSumBytes(byte, opBytes.first)) {
					opBytes.second = byte;
					occupied++;
				} else {
					// Push two keywords.
					std::string s1;
					std::string s2;
					s1.push_back(opBytes.first);
					s2.push_back(byte);
					// do
					blocks.push_back(s1);
					if (!cache.empty()) {
						blocks.push_back(cache);
						cache.clear();
					}
					blocks.push_back(s2);
					occupied = 0;
					opBytes.first = 0;
					opBytes.second = 0;
				}
			}
			pos++;
			continue;
		} else {
			if (occupied) {
				// Push those bytes first.
				std::string op;
				op.push_back((char)opBytes.first);
				if (occupied > 1) {
					op.push_back((char)opBytes.second);
				}
				blocks.push_back(op);
				occupied = false;
			}
			cache.push_back(byte);
		}
		pos++;
	}
	if (!cache.empty()) {
		blocks.push_back(cache); //Termination
	}

	/*std::cout << "BUNDLED STRING INSTRUCTIONS: ";
	for (const std::string &i: blocks) {
		if (i == "\n")
			std::cout << "<\\n> "; 
		else
			std::cout << "<" << i << "> ";
	}
	std::cout << std::endl;*/
	
	return blocks;
}

// A array type thing.
// This must equal the order of _Lua_Lex_Keys
static std::vector<std::string> KEYWORDS = {
	"function",
	"local",
	"while",
	"until",
	"for",
	"repeat",
	"bool",
	"integer",
	"stringT",
	"do", // Must be "then" too.
	"end",
	"and",
	"else",
	"elseif",
	"or",
	"if",
	"then", // skipped.
	"break",
	"in",
	"nil",
	"not",
	"return",
	"true",
	"false",
	"\0", // Number should be controlled.
	"\0", // String should return own key.
	"\0", // Varname should return own key.
	"\0",
	"..",
	"==", // Until second key
	">=",
	"<=",
	"~=",
	"...",
	"\0", // Obsolete key.
	"=",
	".",
	"++",
	"--",
	"+",
	"-",
	"*",
	"/",
	"^",
	"%",
	"{",
	"}",
	";", // Separator2
	"[",
	"\1", // Not coverable
	"\1", // Not coverable
	",", // Separator
	"\1", // Not coverable
	"\1", // Not coverable
	"is",
	"(",
	")",
	"\1", // Not coverable [58]
	"thread",
	"\n",
	"\1", // Not coverable
	">",
	"<",
	" ",
	"nop",
	"\1", // Not coverable
	"]",
	"\1", // Not coverable
	"\1", // Not coverable
	"\1", // Not coverable
	"\1", // Not coverable
	"\1", // Not coverable
	"double",
	"\0", // obsolete
	"\1", // Not coverable
	"\1", // Not coverable
	"\1", // Not coverable
	"\1", // Not coverable
	"\1", // 79
	"_CLUA@IGVARCHECK", // flag
	"\1", // Not coverable
	"_CLUA@CONSTTABLE", // flag
	"_CLUA@OBJECTNAME", // flag
	":",
	"\"",
};

_Lua_Lex_Keys getKey(const std::string parameter) {
	uint64_t pos = 1;
	if (parameter == "then")
		return _L_BlockStart;
	for (const std::string &i: KEYWORDS) {
		if (i == parameter) {
			return static_cast<_Lua_Lex_Keys>(pos);
		}
		pos++;
	}
	return _L_UNKNOWN;
}

LuaLexFrame *_seekBackByteFromPos(std::vector<LuaLexFrame> &vct, uint64_t pos) {
	LuaLexFrame *L = nullptr;
	try {
		L = &vct.at(pos);
	} catch (std::out_of_range &e) {
		return nullptr;
	}
	return L;
}

#include <deque>
enum _parserClosureType {
	COMMENT,
	TABLE,
	//Block,
	PARENTHESES,
	STRING,
	STRING2,
	INDEX,
	OBJCONTROLNAME,
	FPARENTHESES,
	NOTHING,
};

_parserClosureType getLatestClosure(std::deque<_parserClosureType> &c) {
	if (c.size() == 0) {
		return NOTHING;
	}
	return c.back();
}

bool _capableOfEndingAClosure(_Lua_Lex_Keys KEY, _parserClosureType CLOSURE) {
	switch (KEY) {
		case _L_NEWLINE: {
			switch (CLOSURE) {
				case COMMENT: {
					return true;
				}
				default: {
					return false;
				}
			}
		}
		default: {
			return false;
		}
	}
}

void pushNewLLF(std::vector<LuaLexFrame> &vct, _Lua_Lex_Keys key) {
	vct.push_back(LuaLexFrame(key));
}

void modifyNextKey(LuaLexFrame *key, LuaLexFrame &data) {
	key->local = data.local;
	key->ATTRIB = data.ATTRIB;
	data = LuaLexFrame(_L_NONE);
}

// Debug info must be included.
// So any data must be put like an array.
std::string *getDebugLineNmakeNewOne(std::vector<std::string> data, uint64_t pos) {
	std::string *str = new std::string();
	while (true) {
		std::string it;
		try {
			it = data.at(pos);
		} catch (std::out_of_range &e) {
			str->append(" <EOF>");
			return str;
		}
		if (it == "\n") {
			str->append(" <_L_NEWLINE>");
			// Cut out.
			return str;
		} else {
			str->append(it);
		}
	}
}

bool shouldntSaveKey(_Lua_Lex_Keys k) {
	switch (k) {
		case _L_STRING_CTRL: {return true;}
		case _L_UNKNOWN: {return true;}
		case _L_OBJECTCODENAME: {return true;}
		case _L_BlockStart: {return true;}
		case _L_SEPARATOR: {return true;}
		default: {return false;}
	}
}

// _ParseSecondStage(std::vector<std::string> data)
std::vector<LuaLexFrame> _ParseSecondStage(std::vector<std::string> data) {
	// Check for data.
	if (data.size() < 2) {
		m_LuaErrorHandler->reportError(_lua_es_TooSmallEntry, 0, "");
		m_LuaErrorHandler->setFatal(true);
		return std::vector<LuaLexFrame>{ _L_NOP };
	}
	// Debug attribution.
	std::string *debugAttrib = nullptr;
	// Pos
	uint64_t pos = 0;
	// To return
	std::vector<LuaLexFrame> vct;
	// Cache
	std::string cache0;
	std::string cache1;
	_Lua_Lex_Keys cache2;
	// Attrib
	LuaLexFrame data0;
	// Closures
	std::deque<_parserClosureType> closure;
	// Continue
	for (const std::string &key: data) {
		// Get key version.
		_Lua_Lex_Keys k = getKey(key);
		if (_L_NONE == k) {
			if (getLatestClosure(closure) == STRING) {
				goto hyProc;
			} else if (getLatestClosure(closure) == OBJCONTROLNAME) {
				if (cache1.empty()) {
					pos++;
					continue;
				}
				LuaLexFrame OBJC(_L_OBJECTCODENAME);
				OBJC._data = std::vector<uint8_t>(cache1.begin(), cache1.end());
				vct.push_back(OBJC);
				cache1.clear();
				closure.pop_back();
			}
			pos++;
			continue;
		}
		if (cache2 == _L_NEWLINE) {
			// Read this new line until \n nor EOF
			debugAttrib = getDebugLineNmakeNewOne(data, pos);
		}
		if (closure.size() > 0) {
			if (_capableOfEndingAClosure(k, closure.back())) {
				closure.pop_back();
				vct.push_back(LuaLexFrame(k));
				pos++;
				continue;
			}
		}
		if (k == _L_DECREMENT_VARNUM) {
			// Either a comment nor a [plusValue]-- (C syntax)
			LuaLexFrame *v = _seekBackByteFromPos(vct, pos-1);
			if (v == nullptr) {
				// A comment <Starting of a script>
				closure.push_back(COMMENT);
			} else {
				if (v->key == _L_VARNAME || v->key == _L_ON_TO_GO_END) {
					// Strike that value.
					v->ATTRIB = 0xFE; // minus, FF is incrementd.
					pos++;
					continue;
				} else {
					closure.push_back(COMMENT);
					pos++;
					continue;
				}
			}
		}
		if (k == _L_INCREMENT_VARNUM) {
			LuaLexFrame *v = _seekBackByteFromPos(vct, pos-1);
			if (v) {
				if (v->key == _L_VARNAME || v->key == _L_ON_TO_GO_END) {
					v->ATTRIB = 0xFF; // minus, FF is incrementd.
					pos++;
					continue;
				}
			} else {
				m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "Usage of Increment<++>");
				return std::vector<LuaLexFrame>{_L_NOP};
			}
		}
		if (k == _L_UNKNOWN) {
			if (closure.size() > 0) {
				hyProc:
				switch (closure.back()) {
					case STRING: {
						cache1.append(key);
						break;
					}
					case STRING2: {
						cache1.append(key);
						break;
					}
					case OBJCONTROLNAME: {
						cache1.append(key);
						break;
					}
					default: {
						goto _upperPair;
					}
				}
				pos++;
				continue;
			} else {
				_upperPair:
				// Should be a varname nor a number
				std::pair<bool, bool> _P = _chars_is_numb(key);
				if (_P.first) {
					LuaLexFrame _N(_L_NUMBER);
					_N._data = std::vector<uint8_t>(key.begin(), key.end());
					_N.ATTRIB = _P.second;
					_N.debugSymbolLine = debugAttrib;
					vct.push_back(_N);
				} else {
					LuaLexFrame buff(_L_VARNAME);
					buff._data = std::vector<uint8_t>(key.begin(), key.end());
					buff.debugSymbolLine = debugAttrib;
					vct.push_back(buff);
				}
				pos++;
				continue;
			}
		} else {
			if (k == _L_ON_TO_GO_OBJECT) { // Only lexer should read this key. Compiler shouldn't.
				k = _L_ON_TO_GO;
				data0.ATTRIB = 0xFF;
			}
			if (k == _L_ON_TO_GO_P) {
				k = _L_ON_TO_GO;
				data0.ATTRIB = 1;
				closure.push_back(INDEX);
			}
			// Either string or comment, should not push special keys.
			if (getLatestClosure(closure) == STRING || getLatestClosure(closure) == COMMENT) {
				if (k != _L_STRING_CTRL) {
					cache1.append(key);
					pos++;
					continue;
				} else {
					if (getLatestClosure(closure) == COMMENT) {
						pos++;
						continue;
					}
				}
			}
			if (!shouldntSaveKey(k)) {
				pushNewLLF(vct, k);
				modifyNextKey(&vct.back(), data0);
				vct.back().debugSymbolLine = debugAttrib;
			}
			// Update closures and many more.
			switch (k) {
				case _L_BlockStart: {
					if (getLatestClosure(closure) == FPARENTHESES) {
						pushNewLLF(vct, _L_F_ARGS_END);
						pushNewLLF(vct, k);
						closure.pop_back();
					} else {
						pushNewLLF(vct, k);
					}
					break;
				}
				case _L_FOR: {
					// Insert _L_F_ARGS_START
					pushNewLLF(vct, _L_F_ARGS_START);
					closure.push_back(FPARENTHESES);
					break;
				}
				case _L_OBJECTCODENAME: {
					//OBJCONTROLNAME
					closure.push_back(OBJCONTROLNAME);
					cache1.clear();
					break;
				}
				case _L_ON_TO_GO_END: {
					if (getLatestClosure(closure) == INDEX) {
						closure.pop_back();
					} else {
						m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "After ] closure theres other non closed closure.");
					}
					break;
				}
				case _L_STRING_CTRL: {
					if (getLatestClosure(closure) == STRING) {
						closure.pop_back();
						// Save string.
						LuaLexFrame k(_L_STRING);
						k._data = std::vector<uint8_t>(cache1.begin(), cache1.end());
						cache1.clear();
						vct.push_back(k);
					} else {
						closure.push_back(STRING);
					}
					break;
				}
				case _L_RETURN: {
					pushNewLLF(vct, _L_F_ARGS_START);
					closure.push_back(PARENTHESES);
					break;
				}
				case _L_LOCAL: {
					data0.local = true;
					break;
				}
				case _L_TABLE_START: {
					closure.push_back(TABLE);
					break;
				}
				case _L_DECLR: {
					if (getLatestClosure(closure) == FPARENTHESES) {
						break;
					}
					// Push extra.
					pushNewLLF(vct, _L_F_ARGS_START);
					closure.push_back(PARENTHESES);
					break;
				}
				case _L_TABLE_END: {
					_AGAIN__:
					if (getLatestClosure(closure) == TABLE) {
						closure.pop_back();
					} else if (getLatestClosure(closure) == PARENTHESES) {
						pushNewLLF(vct, _L_F_ARGS_END);
						closure.pop_back();
						goto _AGAIN__;
					} else {
						m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "Unordered closures detected!");
						return std::vector<LuaLexFrame>{_L_NOP};
					}
					break;
				}
				case _L_SEPARATOR: { //","
					if (closure.size() == 0) {
						m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "Unexpected ','");
						return std::vector<LuaLexFrame>{_L_NOP};
					}
						
					switch (closure.back()) {
						case PARENTHESES: {
							pushNewLLF(vct, _L_F_ARGS_END);
							pushNewLLF(vct, k);
							closure.pop_back();
							break;
						}
						case TABLE: {
							pushNewLLF(vct, k);
							break;
						}
						case FPARENTHESES: {
							pushNewLLF(vct, k);
							break;
						}
						default: {
							m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "Unexpected ','");
							return std::vector<LuaLexFrame>{_L_NOP};
						}
					}
					break;
				}
				case _L_SEPARATOR2: { //";"
					if (getLatestClosure(closure) == PARENTHESES) {
						pushNewLLF(vct, _L_F_ARGS_END);
						closure.pop_back();
					}
					break;
				}
				case _L_NEWLINE: {
					if (getLatestClosure(closure) == PARENTHESES) {
						pushNewLLF(vct, _L_F_ARGS_END);
						closure.pop_back();
					}
					break;
				}
				default: {}
			}
			
		}
		cache0 = key;
		pos++;
	}
	// Resolve closures.
	while (true) {
		if (closure.size() > 0) {
			const _parserClosureType i = closure.back();
			switch (i) {
				case TABLE: {
					m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "Need to close { for table!");
					return std::vector<LuaLexFrame>{_L_NOP};
				}
				case PARENTHESES: {
					pushNewLLF(vct, _L_F_ARGS_END);
					closure.pop_back();
					break;
				}
				case COMMENT: {
					// Nothing.
					closure.pop_back();
					break;
				}
				case FPARENTHESES: {
					m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "'do' expected somewhere!");
					return std::vector<LuaLexFrame>{_L_NOP};
				}
				case STRING: {
					m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "Need to close \" for string!");
					return std::vector<LuaLexFrame>{_L_NOP};
				}
				case STRING2: {
					m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "Need to close [[ for string!");
					return std::vector<LuaLexFrame>{_L_NOP};
				}
				case OBJCONTROLNAME: {
					m_LuaErrorHandler->reportError(_lua_es_BadSyntax, (uint64_t)debugAttrib, "Internal error handling object control name");
					return std::vector<LuaLexFrame>{_L_NOP};
				}
				case NOTHING: {
					// Why there's nothing?
					closure.pop_back();
					break;
				}
			}
		} else {
			break;
		}
	}
	return vct;
}


std::vector<LuaLexFrame> LuaLex::ParserSecondStage(std::vector<std::string> data) {
	if (data.size() < 2) { //Not usable.
		m_LuaErrorHandler->reportError(_lua_es_TooSmallEntry, 0, "");
		m_LuaErrorHandler->setFatal(true);
		return std::vector<LuaLexFrame>{ _L_NOP };
	}
	// Obsolete code out.
	return _ParseSecondStage(data);
}

std::string LuaLex::dumpInfo(std::vector<LuaLexFrame> S) {
	std::string _s;
	_s.append(" ");
	for (LuaLexFrame &i: S) {
		if (i.key != _L_NEWLINE) {
			_s.append("$");
			_s.append(std::to_string(static_cast<int>(i.key)));
			_s.append("[" + i.keystring + ";" + i.header + "]");
			_s.append(" ");
		} else {
			_s.append("\n");
		}
	}
	return _s;
}

















