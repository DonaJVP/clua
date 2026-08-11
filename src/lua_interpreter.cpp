#include "lua.hpp"
#include <cstdint>
#include <iostream>
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

std::vector<std::string> LuaLex::ParserFirstStage(std::vector<uint8_t> data) {
	// First stage, just separate by <operators: = ! ( ) { } [ ] / * + - < > " ' | %>
	std::vector<std::string> blocks;
	std::string cache;
	uint8_t _last_char = 0;
	// This has no brain, yet.
	uint8_t byte = 0;
	uint32_t pos = 0;
	uint8_t pointer_COUNT = 0;
	for (;;) {
		try {
			byte = data.at(pos);
		} catch (std::out_of_range &e) {
			break;
		}
		/*if ((byte == '=') || (byte == '!') || (byte == '(') || (byte == ')')
		 || (byte == '{') || (byte == '}') || (byte == '[') || (byte == ']')
		 || (byte == '/') || (byte == '*') || (byte == '+') || (byte == '-')
		 || (byte == '<') || (byte == '>') || (byte == '"') ||(byte == '\'')
		 || (byte == '|') || (byte == '%') || (byte == '\\')|| (byte == ' ')
		 || (byte == '.') || (byte == ',') // $ is skipped, because it is needed for $00 commands
		) {
			if (byte == '\n') {
				blocks.push_back(std::string("\\n"));
				continue;
			}
			if (!_is_numb(_last_char) && byte != '.') {
				if (!cache.empty())
					blocks.push_back(cache);
				cache.clear();
				blocks.push_back(std::string(sizeof(char), static_cast<char>(byte)));
			} else {
				blocks.push_back(std::string(sizeof(char), static_cast<char>(byte)));
			}
		} else {
			cache.resize(cache.size());
			cache.append(std::string(sizeof(char), static_cast<char>(byte)));
		}*/

		if ((byte == '=') || (byte == '!') || (byte == '(') || (byte == ')')
			|| (byte == '{') || (byte == '}') || (byte == '[') || (byte == ']')
			|| (byte == '/') || (byte == '*') || (byte == '+') || (byte == '-')
			|| (byte == '<') || (byte == '>') || (byte == '"') ||(byte == '\'')
			|| (byte == '|') || (byte == '%') || (byte == '\\')|| (byte == ' ')
			|| (byte == ';') || (byte == ',') || (byte == ':') || (byte == '.')
			|| (byte == 0x9) || (byte == 0xA) // $ is skipped, because it is needed for $00 commands
		) {
			if (byte == 0x9) {
				//blocks.push_back(std::to_string('\n'));
				if (!cache.empty())
					blocks.push_back(cache);
				cache.clear();
				pos++;
				continue; // Skip tab
			}
			if (byte == 0xA) {
				if (!cache.empty())
					blocks.push_back(cache);
				cache.clear();
				blocks.push_back("\n");
				pos++;
				continue;
			}
			
			if (byte == '.') {
				if (!cache.empty())
					blocks.push_back(cache);
				cache.clear();
				pointer_COUNT++;
				pos++;
				continue;
			} else {
				if (pointer_COUNT > 0) {
					std::string k;
					for (int i = 0; i < pointer_COUNT; i++) {
						k.push_back('.');
					}
					blocks.push_back(k);
					pointer_COUNT = 0;
				}
				if (!cache.empty())
					blocks.push_back(cache);
				cache.clear();
			}
			
			blocks.push_back(std::string(sizeof(char), static_cast<char>(byte)));
		} else {
			/*if (byte == '.') {
				uint8_t p1 = 0;
				uint8_t p2 = 0;
				std::string c;
				try {
					p1 = data.at(pos+1);
					pos++;
					if (p1 == '.')
						c.push_back(p1);
					else
						pos--;
				} catch (std::out_of_range &e) {
					goto _CONT_C_C_;
				}
				try {
					p2 = data.at(pos+2);
					pos++;
					if (p2 == '.')
						c.push_back(p2);
					else
						pos--;
				} catch (std::out_of_range &e) {
					goto _CONT_C_C_;
				}
				if (!c.empty()) {
					if (!cache.empty()) 
						blocks.push_back(cache);
					cache.clear();
					c.push_back(byte);
					blocks.push_back(c);
					continue;
				}
			}*/
			cache.resize(cache.size());
			cache.push_back(byte);
		}
		_last_char = byte;
		pos++;
	}
	if (!cache.empty()) {
		blocks.push_back(cache); //Termination
	}

	// Only process for each lines in the file.
	std::string _CACHE;
	__LINES.push_back(std::vector<std::string>());
	luaCurrentFileId = __LINES.size()-1;
	for (uint8_t &k: data) {
		if (k == '\n') {
			__LINES[luaCurrentFileId].push_back(_CACHE);
			_CACHE.clear();
		} else {
			_CACHE.resize(_CACHE.size() + sizeof(char));
			_CACHE.append(std::string(sizeof(char), static_cast<char>(k)));
		}
	}
	// Notice!
	// Let's see if it are single line
	if (__LINES[luaCurrentFileId].empty()) {
		__LINES[luaCurrentFileId].push_back(_CACHE);
	}
	_CACHE.clear();
	
	return blocks;
}




std::vector<LuaLexFrame> LuaLex::ParserSecondStage(std::vector<std::string> data) {
	if (data.size() < 2) { //Not usable.
		m_LuaErrorHandler->reportError(_lua_es_TooSmallEntry, 0, "");
		m_LuaErrorHandler->setFatal(true);
		return std::vector<LuaLexFrame>{ _L_NOP };
	}
	
	std::vector<LuaLexFrame> blocks;
	LuaLexFrame FRM_;
	FRM_.key = _L_START;
	blocks.push_back(FRM_);
	//Just to complete keyword
	auto search = [this] (std::vector<std::string> *blks, std::string key, bool &found, uint16_t jump) {
		uint16_t jumps = 0;
		for (std::string &str: *blks) {
			if (str == key) {
				jumps++;
				if (jumps > jump) {
					found = true;
				}
			}
		}
	};
	// S T R I N G S
	bool _on_string = false;
	std::string cache;

	
	
	// Statements
	bool _on_comment = false;     // cmt
	bool _on_comment_i = false;
	bool _on_comment_st = false;  // \n
	bool _on_comment_st2 = false; // --
	bool _on_comment_st3 = false; // [[
	bool _on_bidir_001	= false; // [] neither [[]] <variable dir/big string>
	bool _on_bidir_002 = false; // Op to 001
	bool _big_string = false;
	bool _variable_decl_001 = false; // IF <name> + [.] = EXAMPLE = NameVar.[ACC]
	bool _function_decl = false;
	bool _local_decl = false;
	bool _bool_decl = false;
	bool _int_decl = false;
	bool _str_decl = false;
	bool _thr_decl = false;
	bool _then_decl = false;
	bool _decl_decl = false;
	bool _decl_min = false;
	bool _decl_max = false;
	bool _equal_frm = false;
	bool _minusthan = false;
	bool _morethan = false;
	bool _local_toclose = false; // Added multiple declarations in one line
	bool _if = false;
	bool _mat = false;
	bool _else = false;
	bool _elseif = false;
	bool _end = false;
	bool _LAST_Str = false;
	uint32_t _declr_p0 = 0;
	bool _for = false;
	uint8_t _key = false;
	LuaLexFrame CACHE_LEX;
	LuaLexFrame *ptrToLastLexF = nullptr;
	_Lua_Lex_Keys KY = _L_NONE;

	uint32_t words_count = 0;
	uint32_t wordpos = 0;

	//BEGIN DEBUG_METHODS

	auto show_code_ = [this, &data] (uint32_t wordscount) {
		std::string _C;
		// Start on the pos -7 (Enough to find a bug huh?) to 7, likely 14 positions revealed.
		if (wordscount < 7)
			wordscount = 7;
		uint32_t _max = data.size();
		if (_max < 7)
			_max = data.size();
		else
			_max = 7;
		uint16_t spaces_count = 0;
		for (int i = wordscount-7; i <= _max; i++) {
			if (data.size() == i)
				break;
			if (data[i] == " ") {
				spaces_count++;
				continue;
			}
			_C.append(std::string("["));
			_C.append(std::to_string(i-spaces_count));
			_C.append(std::string("]"));
			_C.append(data[i]);
			_C.append(" ");
		}
		return _C;
	};

	//END DEBUG_METHODS

	_Lua_Lex_Keys _last_frame = _L_NIL;
	uint32_t pos__ = 0;
	std::string key;
	LuaLexFrame _VAR(_L_VARNAME);
	for (;;) { // [[ T H I S   I S   T H E    F U T U R E ]]
		try {
			key = data.at(pos__);
		} catch (std::out_of_range &e) {
			return blocks;
		}
		pos__++;
		
		std::pair<bool, bool> A_T_R = _chars_is_numb(key);
		
		
		words_count++;
		if ((key == " ") && !_big_string && !_on_string)
			continue; // Ignore the space char, it's not used.
		if (key == "\n") {
			if (_declr_p0) {
				LuaLexFrame _L(_L_F_ARGS_END);
				blocks.push_back(_L);
				_declr_p0 = false;
			}
			if (_local_toclose) {
				LuaLexFrame _L(_L_F_ARGS_END);
				blocks.push_back(_L);
				_local_toclose = false;
			}
			_on_comment = false;
			LuaLexFrame _NEWLINE(_L_NEWLINE);
			blocks.push_back(_NEWLINE);
			continue;
		}
			
		// B I G   S T R I N G   && [[B I G]] C O M M E N T

		if (key == "]") {
			// Close
			if (!_on_bidir_002) {
				_on_bidir_002 = true;
			} else {
				// Double
				if (_big_string) {
					// A big string
					// Close
					_big_string = false;
					LuaLexFrame _STR(_L_STRING);
					_STR._data = std::vector<uint8_t>(cache.begin(), cache.end());
					_STR.keystring = "]";
					blocks.push_back(_STR);
					cache.clear();
				} else if (_on_comment_st3) {
					_on_comment_st3 = false;
				}
			}
			continue;
		}
		if (_on_bidir_002) {
			goto _insertOnToGoEnd;
			_JMP0:
			_on_bidir_002 = false;
		}
		
		if (_big_string) {
			cache.append(key);
			continue;
		}
		if (key == "[") {
			if (_on_comment_i) {
				if (!_on_comment_st3) { // Third mode comment, "[[ ]]"
					if (_on_comment) {
						_on_comment_st3 = true;
						continue;
					}
				} else {
					continue; //Skip this, as a comment
				}
			} else { // Bidir
				if (!_on_comment) {
					if (!_on_bidir_001) {
						// It is a string neither a table dir, with hope; lol
						_on_bidir_001 = true;
						//_on_bidir_002 = true;
						continue;
					} else {
						//if (_on_bidir_001) { //Big string
							_on_bidir_001 = false;
							_big_string = true;
						//}
						// Nothing.
						continue;
					}
				}
			}
		} else { // Make sure if not _on_bidir_001 and last char was "[" then it should be an variable dir
			// _on_bidir_001 == true so other char, then variable dir, insert an variable
			if (_on_bidir_001) {
				LuaLexFrame _ACC(_L_ON_TO_GO);
				_ACC.keystring = "->";
				_ACC.ATTRIB = 1;
				blocks.push_back(_ACC);
				_on_bidir_001 = false;
			}
		}
		//_on_bidir_002 = false;
		_on_comment_i = false; // Remove comment iterator
		if (_on_comment && !_on_string) {
			if (key == "\\") {
				_on_comment_st = true;
			} else if (key == "n") {
				if (_on_comment_st && !_on_comment_st3) {
					_on_comment = false;
				}
			}
			continue;
		} else {
			if (key == "-" && !_on_string) {
				try {
					if (data.at(pos__) == "-") {
						_on_comment_i = true; //Only one iteration
						_on_comment = true;
						_on_comment_st2 = false;
						_on_comment_st = false;
						continue;
					}
				} catch (std::out_of_range &e) {
					return blocks;
				}
				
			}
		}
		
		// S T R I N G
		if (_on_string) {
			if (key == "\"" || key == "'") {
				_on_string = false;
				LuaLexFrame _STR(_L_STRING);
				_STR._data = std::vector<uint8_t>(cache.begin(), cache.end());
				_STR.keystring.append("\"");
				_STR.keystring.append(cache);
				_STR.keystring.append("\"");
				blocks.push_back(_STR);
				cache.clear();
			} else {
				cache.append(key);
			}
			continue;
		} else {
			if (key == "\"" || key == "'") {
				_on_string = true;
				_mat = true;
				_LAST_Str = true;
				goto _EQ_TERM;
			}
		}

		// Flags
		
		if (key == "_CLUA@IGNOREVARIABLESCHECK_") {
			LuaLexFrame _K(_L_FLAG_IGNORE_VARCHECK);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		
		if (key == "_CLUA@ConstTable") {
			LuaLexFrame _K(_L_CONSTTABLE);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		
		// L O C A L

		if (key == "local") {
			if (_local_decl) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad naming found for word 'local': " + std::to_string(words_count) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			_local_decl = true;
			LuaLexFrame _LCL(_L_LOCAL);
			blocks.push_back(_LCL);
			LuaLexFrame _L(_L_F_ARGS_START);
			_L.local = true;
			_local_toclose = true;
			blocks.push_back(_L);
			goto _FLUSH;
		}

		// F U N C T I O N

		if (key == "function") {
			if (_function_decl) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad naming found for word 'function': " + std::to_string(words_count) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			if (_local_toclose) {
				// Pop that block!
				_local_toclose = false;
				blocks.pop_back();
			}
			LuaLexFrame _F(_L_FUNCTION);
			blocks.push_back(_F);
			_function_decl = true;
			goto _FLUSH;
		}

		// IF/ELSE/ELSEIF/END

		if (key == "if") { // Can be ran inside script or func
			if (_elseif) {
				m_LuaErrorHandler->reportWarning(_lua_es_BadSyntax, 0, std::string("elseif statement already present, no need for 'if': " + std::to_string(words_count) + "> " + show_code_(words_count)));
				_elseif = false;
				goto _FLUSH; // Skip this.
			}
			if (_if) {
				m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad naming found for word 'if': " + std::to_string(words_count) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			_if = true;
			LuaLexFrame _IF(_L_IF);
			blocks.push_back(_IF);
			LuaLexFrame _CL(_L_F_ARGS_START);
			blocks.push_back(_CL);
			goto _FLUSH;
		}
		if (key == "for") { // Can be ran inside script or func
			_for = true;
			LuaLexFrame _F(_L_FOR);
			blocks.push_back(_F);
			LuaLexFrame _CL(_L_F_ARGS_START);
			blocks.push_back(_CL);
			goto _FLUSH;
		}
		if (key == "do") { // Can be ran inside script or func
			LuaLexFrame _CL(_L_F_ARGS_END);
			blocks.push_back(_CL);
			if (!_for) {
				LuaLexFrame _F(_L_BlockStart);
				blocks.push_back(_F);
			}
			_for = false;
			goto _FLUSH;
		}
		if (key == "else") {
			if (_else || _elseif) {
				m_LuaErrorHandler->reportError(_lua_es_BadSyntax, 0, std::string("Bad naming found for word 'else': " + std::to_string(words_count) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			_else = true;
			LuaLexFrame _ELSE(_L_ELSE);
			blocks.push_back(_ELSE);
			goto _FLUSH;
		}
		if (key == "elseif") {
			if (_elseif || _else) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad naming found for word 'elseif': " + std::to_string(words_count) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			_elseif = true;
			LuaLexFrame _ELSEIF(_L_ELSEIF);
			blocks.push_back(_ELSEIF);
			goto _FLUSH;
		}
		if (key == "end") {
			/*if (_end || _if || _elseif || _else) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad naming found for word 'end': " + std::to_string(words_count) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}*/
			_end = true;
			LuaLexFrame _END(_L_END);
			blocks.push_back(_END);
			goto _FLUSH;
		}
		if (key == "then") {
			if (_then_decl) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad naming found for word 'end': " + std::to_string(words_count) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			_if = false;
			_then_decl = true;
			LuaLexFrame _CL(_L_F_ARGS_END);
			blocks.push_back(_CL);
			//LuaLexFrame _THEN(_L_THEN);
			//blocks.push_back(_THEN);
			goto _FLUSH;
		}

		//BEGIN VAR TYPES
		// Some addons
		// B O O L
		if ((key == "bool") || (key == "$10")) {
			if (_bool_decl || _int_decl || _str_decl) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad value formatting for 'bool'" + std::string(reinterpret_cast<const char*>(words_count)) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			// We define variable as an bool value
			//LuaLexFrame _X(_L_VARTYPE);
			//_X.subkey = _L_BOOL;
			//_X.keystring = "bool";
			//blocks.push_back(_X); // Not this time.
			//CACHE_LEX = _X;
			//_bool_decl = true;
			//blocks.push_back(_X);
			KY = _L_BOOL;
			goto _FLUSH;
		}

		// I N T
		if (key == "double") {
			if (_bool_decl || _int_decl || _str_decl) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad value formatting for 'double'" + std::string(reinterpret_cast<const char*>(words_count)) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			// We define variable as an bool value
			KY = _L_DOUBLE;
			//blocks.push_back(_X); // Not this time.
			//CACHE_LEX = _X;
			//_int_decl = true;
			//blocks.push_back(_X);
			goto _FLUSH;
		}
		
		// I N T
		if (key == "int") {
			if (_bool_decl || _int_decl || _str_decl) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad value formatting for 'int'" + std::string(reinterpret_cast<const char*>(words_count)) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			// We define variable as an bool value
			KY = _L_INT;
			goto _FLUSH;
		}

		// S T R I N G
		if (key == "string") {
			if (_bool_decl || _int_decl || _str_decl) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad value formatting for 'string'" + std::string(reinterpret_cast<const char*>(words_count)) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			// We define variable as an bool value
			KY = _L_STRING2;
			goto _FLUSH;
		}
		// T H R E A D
		if (key == "thread") {
			if (_bool_decl || _int_decl || _str_decl) {
				m_LuaErrorHandler->reportError(_lua_es_BadVariableNamingMethod, 0, std::string("Bad value formatting for 'thread'" + std::string(reinterpret_cast<const char*>(words_count)) + "> " + show_code_(words_count)));
				m_LuaErrorHandler->setFatal(true);
				return blocks;
			}
			// We define variable as an bool value
			KY = _L_THREAD;
			goto _FLUSH;
		}
		//END VAR TYPES

		//

		
_EQ_TERM:
		//std::cout << key.size() << " = " << key << std::endl;
		if (key == "=") {
			//LuaLexFrame _K(_L_DECLR);
			//blocks.push_back(_K);
			if (_decl_min) {
				LuaLexFrame _K(_L_EQUALS_OR_MINUS);
				blocks.push_back(_K);
				_decl_min = false;
			}
			if (_decl_max) {
				LuaLexFrame _K(_L_EQUALS_OR_MORE);
				blocks.push_back(_K);
				_decl_max = false;
			}
			if (!_decl_decl) {
				_decl_decl = true;
			} else {
				LuaLexFrame _K(_L_EQUALS);
				blocks.push_back(_K);
				_decl_decl = false;
			}
			goto _FLUSH;
		} else if (_decl_decl) {
			_decl_decl = false;
			ptrToLastLexF->declr = true;
			if (_local_toclose) {
				LuaLexFrame _L(_L_F_ARGS_END);
				blocks.push_back(_L);
			}
			LuaLexFrame _K(_L_DECLR);
			blocks.push_back(_K);
			_local_decl = false;
			if (!_for) {
				_declr_p0++;
				LuaLexFrame _L(_L_F_ARGS_START);
				blocks.push_back(_L);
			}
			if (!_mat)
				goto _GARGABE;
		}
		if (key == "<") {
			if (!_decl_min)
				_decl_min = true;
			_minusthan = true;
			goto _FLUSH;
		} else {
			if (key != "=" && _minusthan) {
				_minusthan = false;
				LuaLexFrame _K(_L_MINUSTHAN);
				blocks.push_back(_K);
				goto _FLUSH;
			}
		}
		if (key == ">") {
			if (!_decl_max)
				_decl_max = true;
			_morethan = true;
			goto _FLUSH;
		} else {
			if (key != "=" && _morethan) {
				_morethan = false;
				LuaLexFrame _K(_L_MORETHAN);
				blocks.push_back(_K);
				goto _FLUSH;
			}
		}
		if (_LAST_Str)
			goto _END;

_GARGABE:
		// Some logic, always not checked bcuz i'm lazy.
		if (key == "not") {
			LuaLexFrame _K(_L_NOT);
			_K.keystring = key;
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "and") {
			LuaLexFrame _K(_L_AND);
			_K.keystring = key;
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "or") {
			LuaLexFrame _K(_L_OR);
			_K.keystring = key;
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "return") {
			LuaLexFrame _K(_L_RETURN);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		
		// Bools
		if (key == "true") {
			LuaLexFrame _K(_L_TRUE);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "false") {
			LuaLexFrame _K(_L_FALSE);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		
		// Syntax
		if (key == "+") {
			LuaLexFrame _K(_L_SYNTAX_SUM);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "-") {
			LuaLexFrame _K(_L_SYNTAX_DEC);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "/") {
			LuaLexFrame _K(_L_SYNTAX_DIV);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "*") {
			LuaLexFrame _K(_L_SYNTAX_MUL);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "^") {
			LuaLexFrame _K(_L_SYNTAX_EXP);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		
		// F l u s h
		
		if (key == "(") {
			LuaLexFrame _K(_L_F_ARGS_START);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == ")") {
			//LuaLexFrame _E(_L_SEPARATOR);
			//blocks.push_back(_E);
			LuaLexFrame _K(_L_F_ARGS_END);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == ";") {
			if (_declr_p0) {
				LuaLexFrame _L(_L_F_ARGS_END);
				blocks.push_back(_L);
				_declr_p0--;
			}
			LuaLexFrame _K(_L_SEPARATOR);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "{") {
			LuaLexFrame _K(_L_TABLE_START);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "}") {
			if (_declr_p0) {
				LuaLexFrame _L(_L_F_ARGS_END);
				blocks.push_back(_L);
				_declr_p0--;
			}
			LuaLexFrame _K(_L_TABLE_END);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == ",") {
			if (_declr_p0) {
				LuaLexFrame _L(_L_F_ARGS_END);
				blocks.push_back(_L);
				_declr_p0--;
			}
			LuaLexFrame _K(_L_SEPARATOR);
			_K.ATTRIB = 1;
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "]") {
			_insertOnToGoEnd:
			LuaLexFrame _K(_L_ON_TO_GO_END);
			blocks.push_back(_K);
			if (_on_bidir_002)
				goto _JMP0;
			goto _FLUSH;
		}
		if (key == "..") {
			LuaLexFrame _K(_L_CONCAT);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		if (key == "...") {
			LuaLexFrame _K(_L_MULTIPLEARGS);
			blocks.push_back(_K);
			goto _FLUSH;
		}
		/*if (key == "." && data.size() <= (pos__+1) && data.at(pos__+1) == ".") {
			LuaLexFrame _K(_L_CONCAT);
			blocks.push_back(_K);
			pos__++;
			goto _FLUSH;
		}*/
		
		

		if (!A_T_R.first && key.find('.') != std::string::npos) { //Access variable. [Table]
			// Get the keywords
			std::string _CACHE;
			std::vector<std::string> _C_CACHE;
			char buff = 0;
			uint8_t pos= 0;
			for (char &c: key) {
				if (buff != '.') {
					_CACHE.push_back(c);
				} else {
					if (!_CACHE.empty())
						_C_CACHE.push_back(_CACHE);
					_CACHE.clear();
					//Put the . in chain
					_C_CACHE.push_back(std::to_string(c));
					
				}
			}
			if (!_CACHE.empty())
				_C_CACHE.push_back(_CACHE);
			_CACHE.clear();
			// Pair.
			for (std::string &d :_C_CACHE) {
				if (d == ".") {
					_VAR.key = _L_ON_TO_GO;
					_VAR.keystring = "->";
					_VAR._data = std::vector<uint8_t>();
					_VAR.ATTRIB = 0;
					blocks.push_back(_VAR);
				} else {
					_VAR.header = "";
					_VAR.key = _L_VARNAME;
					_VAR.keystring = d;
					_VAR._data = std::vector<uint8_t>(d.begin(), d.end());
					blocks.push_back(_VAR);
				}
			}
			goto _FLUSH;
		}
		if (A_T_R.first) {
			LuaLexFrame _STR(_L_NUMBER);
			_STR.ATTRIB = A_T_R.second;
			_STR._data = std::vector<uint8_t>(key.begin(), key.end());
			_STR.keystring = key;
			blocks.push_back(_STR);
			goto _FLUSH;
		}
		_VAR.header = "Varname";
		_VAR.key = _L_VARNAME;
		_VAR._data = std::vector<uint8_t>(key.begin(), key.end());
		_VAR.keystring = key;
		_VAR.subkey = KY;
		_VAR.local = _local_decl;
		
		KY = _L_NONE;
		blocks.push_back(_VAR);
		ptrToLastLexF = &blocks.at(blocks.size()-1);
		//std::cout << data[words_count] << std::endl; //After sum
_FLUSH:
//		if (key != "local")
//			_local_decl = false;
		if (key != "if")
			_if = false;
		if (key != "else")
			_else = false;
		if (key != "function")
			_function_decl = false;
		_mat = false;
		_on_comment_st2 = false;


_END:
		//Nothing.
		
		_VAR = LuaLexFrame();
		_LAST_Str = false;
		
		continue;
	}
	LuaLexFrame _QUOTE;
	if (_declr_p0)
		_QUOTE.key = _L_F_ARGS_END;
	LuaLexFrame _EOF;
	_EOF.key = _L_EOF;
	blocks.push_back(_QUOTE);
	blocks.push_back(_EOF);
	return blocks;
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
















