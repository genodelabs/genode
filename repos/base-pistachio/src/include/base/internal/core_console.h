/*
 * \brief  Console backend for Pistachio
 * \author Julian Stecklina
 * \date   2008-08-20
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CORE_CONSOLE_H_
#define _INCLUDE__BASE__INTERNAL__CORE_CONSOLE_H_

/* Pistachio includes */
namespace Pistachio {
#include <l4/kdebug.h>
}

/* Genode includes */
#include <base/console.h>

namespace Genode
{
	class Core_console : public Console
	{
		protected:

			void _out_char(char c) { Pistachio::L4_KDB_PrintChar(c); }
	};
}

#endif /* _INCLUDE__BASE__INTERNAL__CORE_CONSOLE_H_ */
