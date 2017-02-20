/*
 * \brief  Readline supplement to resolve symbols missing from the libc
 * \author Norman Feske
 * \date   2009-10-16
 *
 * In the future, the content of this file should go to the libc repository.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <util/string.h>

using namespace Genode;


/********************************************
 ** External references declared in tcap.h **
 ********************************************/

char PC;
char *BC = 0;
char *UP = 0;


extern "C" int tgetnum(char *s);
int tgetnum(char *s)
{
	Genode::log(__func__, ": tgetnum called with s=\"", Genode::Cstring(s), "\"");

	enum { SCREEN_WIDTH = 80, SCREEN_HEIGHT = 25 };
	if (!strcmp(s, "co")) return SCREEN_WIDTH;
	if (!strcmp(s, "li")) return SCREEN_HEIGHT;
	return 1;
}


extern "C" char *tgetstr(char *id, char **area);
char *tgetstr(char *id, char **area)
{
	Genode::log(__func__, " not yet implemented");
	return 0;
}


extern "C" int tputs(const char *str, int affcnt, int (*putc)(int));
int tputs(const char *str, int affcnt, int (*putc)(int))
{
	Genode::log(__func__, " not yet implemented");
	return -1;
}


extern "C" int tgetent(char *bp, const char *name);
int tgetent(char *bp, const char *name)
{
	Genode::log(__func__, " not yet implemented");
	return -1;
}


extern "C" char *tgoto(const char *cap, int col, int row);
char *tgoto(const char *cap, int col, int row)
{
	Genode::log(__func__, " not yet implemented");
	return 0;
}


extern "C" int tgetflag(char *id);
int tgetflag(char *id)
{
	Genode::log(__func__, " not yet implemented");
	return -1;
}

