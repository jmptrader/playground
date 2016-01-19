
extern "C"
{
	#include "lua/lua.h"
	#include "lua/lauxlib.h"
}

int main()
{
	auto x = luaL_newstate();
	return x != nullptr ? 0 : 1;
}