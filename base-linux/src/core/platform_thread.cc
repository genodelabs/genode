/*
 * \brief  Linux-specific platform thread implementation
 * \author Norman Feske
 * \date   2007-10-15
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/token.h>
#include <util/misc_math.h>
#include <base/printf.h>

/* local includes */
#include "platform_thread.h"

/* Linux syscall helper */
#include <linux_syscalls.h>

using namespace Genode;


typedef Token<Scanner_policy_identifier_with_underline> Tid_token;


Platform_thread::Platform_thread(const char *name, unsigned, addr_t)
{
	/* search for thread-id portion of thread name */
	Tid_token tok(name);
	while (tok.type() != Tid_token::END && tok[0] != ':')
		tok = tok.next();

	/* tok points at the colon separator, next token is the id */
	tok = tok.next();

	if (tok.type() == Tid_token::END) {
		PWRN("Invalid format of thread name.");
		return;
	}

	/* convert string to thread id */
	ascii_to(tok.start(), &_tid);

	/* search for process-id portion of thread name */
	while (tok.type() != Tid_token::END && tok[0] != ':')
		tok = tok.next();

	/* tok points at the colon separator, next token is the id */
	tok = tok.next();

	if (tok.type() == Tid_token::END) {
		PWRN("Invalid format of thread name.");
		return;
	}

	/* convert string to process id */
	ascii_to(tok.start(), &_pid);

	/* initialize private members */
	size_t name_len = tok.start() - name;
	strncpy(_name, name, min(sizeof(_name), name_len));
}


void Platform_thread::cancel_blocking()
{
	PDBG("send cancel-blocking signal to %ld\n", _tid);
	lx_tgkill(_pid, _tid, LX_SIGUSR1);
}


void Platform_thread::pause()
{
	PDBG("not implemented");
}


void Platform_thread::resume()
{
	PDBG("not implemented");
}
