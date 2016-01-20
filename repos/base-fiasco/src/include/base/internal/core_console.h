/*
 * \brief  Console backend using the Fiasco kernel debugger
 * \author Norman Feske
 * \date   2006-04-08
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CORE_CONSOLE_H_
#define _INCLUDE__BASE__INTERNAL__CORE_CONSOLE_H_

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/kdebug.h>
}

/* Genode includes */
#include <base/console.h>

namespace Genode {

	class Core_console : public Console
	{
		protected:

			void _out_char(char c) { Fiasco::outchar(c); }
	};
}

#endif /* _INCLUDE__BASE__INTERNAL__CORE_CONSOLE_H_ */
