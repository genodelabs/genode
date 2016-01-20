/*
 * \brief  Printf backend using Linux stdout
 * \author Norman Feske
 * \date   2006-04-08
 *
 * This console back-end should only be used by core.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/console.h>

/* Linux syscall bindings */
#include <linux_syscalls.h>

namespace Genode {

	class Core_console : public Console
	{
		protected:

			void _out_char(char c) { lx_write(1, &c, sizeof(c)); }
	};
}

