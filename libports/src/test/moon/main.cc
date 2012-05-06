/*
 * \brief  Lua C++ library test
 * \author Christian Helmuth
 * \date   2012-05-06
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* Lua includes */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


static int log(lua_State *L)
{
	int n = lua_gettop(L);

	for (int i = 1; i <= n; ++i) {
		if (lua_isstring(L, i))
			PLOG("%s", lua_tostring(L, i));
		else if (lua_isnil(L, i))
			PLOG("%s", "nil");
		else if (lua_isboolean(L, i))
			PLOG("%s", lua_toboolean(L, i) ? "true" : "false");
		else
			PLOG("%s: %p", luaL_typename(L, i), lua_topointer(L, i));
	}
	return 0;
}


static char const *exec_string =
	"i = 10000000000000000 + 1\n"
	"log(\"your result is: \"..i)\n"
	"a = { }\n"
	"log(a)\n"
	"log(type(a))\n"
	"a.foo = \"foo\"\n"
	"a.bar = \"bar\"\n"
	"log(a.foo .. \" \" .. a.bar)\n"
	;


int main()
{
	lua_State *L = lua_open();

	/* initialize libs */
	luaopen_base(L);

	/* register simple log function */
	lua_register(L, "log", log);

	if (luaL_dostring(L, exec_string) != 0)
		PLOG("%s\n", lua_tostring(L, -1));

	lua_close(L);
}
