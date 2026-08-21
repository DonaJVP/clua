# CLua
A little library and program which compiles lua code to dynamic asm code.
Made specifically for functions. This can export lua functions to executable CPP/C functions.
To: <code>casted_lua_func(RDI(uint64_t), RSI(uint64_t), uint64_t*);</code>
from: <code>lua_func(string1, string2, bool1, bool2, integer1, integer2)</code>
This is on a developing state, so expect bugs or crashes in final functions nor unions.
## API
This is like the PUC Lua api, with some adds of LuaJIT.
Helpers:
`CLUA::makeV(uint64_t, CLUA::TYPE) -> CLUA::VALUE`
`CLUA::makeV_D(double) -> CLUA::VALUE`
`CLUA::getVtype(CLUA::VALUE) -> CLUA::TYPE`
`CLUA::getVdata(CLUA::VALUE) -> uint64_t`
`CLUA::isVdouble(CLUA::VALUE) -> bool`
~~`getLuaTypeString(LuaType) -> std::string`~~


### Basic example for CLua JIT
```lua
-- Let's suppose this is on luatest.lua
function _sayHelloWorld()
	print("Hello World!");
end
```
```cpp
// clua_runtest.cpp
#include <clua.hpp> // Only one library...
int main() {
	// Initialize essential
	lua_ErrHandler *LEH = new lua_ErrHandler();
	Lua *m_lua = CLUA::create(LEH);
	// Get file "luatest.lua"
	std::ifstream inputFile("luatest.lua");
	std::string content((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
	// Generate
	CLUA::function script = CLUA::gen(content);
	// Run the lua script
	script(0, 0, nullptr);
	// Now, search for _sayHelloWorld and run it.
	const char *name = "_sayHelloWorld"; // Name
	CLUA::STRING *tname = CLUA::doString(name, strlen(name));
	// Get it..
	CLUA::VALUE val = CLUA::table::getData_String(CLUA::world::m_general, tname, 0);
	if (!val) {
		// Failed somehow..
		return 1;
	} else {
		// Run it.
		CLUA::function fun = (CLUA::function)CLUA::getVdata(val);
		fun(0, 0, nullptr); // There's Hello World
	}
}
```
