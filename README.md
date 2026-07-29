# CLua
A little library and program which compiles lua code to dynamic asm code.
Made specifically for functions. This can export lua functions to executable CPP/C functions.
To: <code>casted_lua_func(RDI(uint64_t), RSI(uint64_t), uint64_t*);</code>
from: <code>lua_func(string1, string2, bool1, bool2, integer1, integer2)</code>
This is on a developing state, so expect bugs or crashes in final functions nor unions.
## API
This is like the PUC Lua api, with some adds of LuaJIT.
Essential:
`Lua*`
`Values (uint64_t :: 48bit pointer, 16bit pointer info)`
`lua_Table*`
`FunctionPointer`
LuaType:
```cpp
enum LuaType: uint64_t {
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
```
String Manipulation:
```cpp
struct TString {
	uint64_t IDX;
	uint64_t len;
	char *data;
	uint64_t _unocc = 0;
};
```
Helpers:
`lua_makeVar(void*, LuaType) -> Values`
`lua_aMakeVarDouble(double) -> Values`
`lua_getVarType(Values) -> LuaType`
`lua_getPtr(Values) -> void*`
`lua_isNum(Values) -> bool`
`getLuaTypeString(LuaType) -> std::string`
``

### Basic example for CLua JIT
```lua
-- Let's suppose this is on luatest.lua
function _sayHelloWorld()
	print("Hello World!");
end
```
```cpp
// clua_runtest.cpp
#include <lua.hpp> // Only one library...
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <math.h>
#include <sstream>

// A pretty basic print function.
static Values PRINT(Values RDI, Values RSI, FuncArgs *ARGS) { // Classic start.
	TString *ptr = (TString*)lua_getPtr(RDI); // Transform first arg to TString*
	std::cout << std::string(ptr->data, ptr->len) << '\n'; // Print
	return 0;
}
int main() {
	// CLua obj
	Lua *obj = new Lua();
	// Seed
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> dist(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
	luaSeed = dist(gen);
	// Errors FIFO
	lua_ErrHandler fifo = lua_ErrHandler();
	fifo.nomutex = true; // Disable mutex, we arent running on multi threaded env.
	LuaErrorHandler *leh = new LuaErrorHandler(&fifo); // Bind fifo
	// Lexer
	LuaLex *lex = new LuaLex(obj, leh); // Start lexer
	obj->m_LuaErrorHandler = leh; // Bind ErrorHandler to CLua
	// Tables
	lua_Table *TBL = new lua_Table(); // For _GEN
	TBL->nodes = (Node*)malloc(0xFFFF*sizeof(Node)); // Allocate 65535 Node spaces
	TBL->hmask = 0xFFFF;
	obj->m_General = TBL; // Bind _GEN
	// Bind "print()" function.
	Values print = lua_makeVar(((void*)PRINT), LuaFunction); // Initialize print
	TString *_PRINTFUNC = returnCompiledString("print"); // Make a name for print
	_F_ASM_NOTGUARANTEED_SETVALUE(TBL, _PRINTFUNC, print); // Bind print func to _GEN
	// LEXER running. [Without ErrorHandler.]
	std::ifstream inputFile("luatest.lua");
	std::string content((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
	std::vector<std::string> _strV = lex->ParserFirstStage(std::vector<uint8_t>(content.begin(), content.end())); // Divide this in multiple words
	std::vector<LuaLexFrame> _lFrmV = lex->ParserSecondStage(_strV); // Convert it to keywords
	std::vector<LuaLexFrame> _lSmpV = lex->ParserThirdStage(_lFrmV); // Simplify and recompile keywords
	std::vector<lua_biOpCode> k = lua_B_F_OP(&j, &pos, n, false, false); // Convert them to opcodes
	void *_lEnvFunc = luaBundleFunction(&k, n, false, nullptr, nullptr, true); // Compile
	// _lEnvFunc is the entire script (Includes local variables)
	((FunctionPointer)_lEnvFunc)(0x0,0x0,nullptr); // Run generated script.
	// Notice the "Hello World!" ins't printed yet. Their function is generated but not executed.
	// So let's proceed to execute it.
	Values node = *(Values*)_F_ASM_NOTGUARANTEED_GETPTR(TBL, _PRINTFUNC);
	// Let's get the function pointer.
	FunctionPointer func = (FunctionPointer)lua_getPtr(node);
	func(0,0,nullptr); // There does print Hello World!
}
```

#### TODO
- Add 'static' keyword for static table, means no modifications at table elements but their values.
- Add values types after local nor variable name declaration for better performance at runtime.
