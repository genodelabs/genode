/*
 * \brief  Lua C++ library test
 * \author Christian Helmuth
 * \date   2012-05-06
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <libc/component.h>
#include <timer_session/connection.h>

/* Lua includes */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


namespace Moon {

	struct Main;
	struct Env
	{
		Timer::Session     &timer;
		Genode::Pd_session &pd;
	};
	Env *env;
}


/**
 * Lua: Sleep for milliseconds
 */
static int l_msleep(lua_State *lua)
{
	if (lua_gettop(lua) != 1) {
		lua_pushstring(lua, "msleep: invalid number of arguments");
		lua_error(lua);
		return 0;
	}
	luaL_checknumber(lua, 1);

	Moon::env->timer.msleep(lua_tonumber(lua, 1));

	return 0;
}


/**
 * Lua: Return RAM quota
 */
static int l_quota(lua_State *lua)
{
	if ((lua_gettop(lua) != 0)) {
		lua_pushstring(lua, "quota: invalid number of arguments");
		lua_error(lua);
		return 0;
	}

	lua_pushnumber(lua, Moon::env->pd.ram_quota().value);

	return 1;
}


/**
 * Lua: Log arguments
 */
static int l_log(lua_State *lua)
{
	int n = lua_gettop(lua);

	for (int i = 1; i <= n; ++i) {
		if (lua_isstring(lua, i))
			Genode::log(lua_tostring(lua, i));
		else if (lua_isnil(lua, i))
			Genode::log("nil");
		else if (lua_isboolean(lua, i))
			Genode::log(lua_toboolean(lua, i) ? "true" : "false");
		else
			Genode::log(luaL_typename(lua, i), ": ", lua_topointer(lua, i));
	}

	return 0;
}


/**
 * Lua library for Genode functions
 */
static const struct luaL_reg l_genode[] = {
	{ "log",    l_log    },
	{ "msleep", l_msleep },
	{ "quota",  l_quota  },
	{ 0, 0 } /* end of list */
};


static char const *exec_string =
	"local a = { }\n"
	"Genode.log(a)\n"
	"a.foo = \"foo\"\n"
	"a.bar = \"bar\"\n"
	"Genode.log(a.foo .. \" \" .. a.bar)\n"
	"\n"
	"print(\"Our RAM quota is \"..Genode.quota()..\" bytes.\")\n"
	"\n"
	"print(\"Going to sleep...\")\n"
	"for i=1,4 do\n"
	"  Genode.msleep(i * 1000)\n"
	"  print(\"Slept well for \"..i..\" seconds.\")\n"
	"end\n"
	"print(\"Finished.\")\n"
	;


struct Moon::Main
{
	Libc::Env &_env;

	Timer::Connection _timer { _env };

	Moon::Env _moon_env { _timer, _env.pd() };

	Main(Libc::Env &env) : _env(env)
	{
		Libc::with_libc([&] () {
			Moon::env = &_moon_env;

			lua_State *lua = lua_open();

			/* initialize libs */
			luaopen_base(lua);

			/* register Genode Lua library */
			luaL_register(lua, "Genode", l_genode);

			if (luaL_dostring(lua, exec_string) != 0)
				Genode::log(lua_tostring(lua, -1));

			lua_close(lua);
		});
	}
};


void Libc::Component::construct(Libc::Env &env) { static Moon::Main instance(env); }
