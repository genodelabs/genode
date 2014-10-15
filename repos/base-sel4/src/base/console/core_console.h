/*
 * \brief  Console backend for seL4
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/console.h>


namespace Genode
{
	class Core_console : public Console
	{
		protected:

			void _out_char(char c) { }
	};
}


