/*
 * \brief  Board driver for core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__BOARD_H_
#define _CORE__INCLUDE__BOARD_H_

/* core includes */
#include <drivers/board_base.h>

namespace Genode
{
	class Board : public Board_base
	{
		public:

			void init() { }

			static bool is_smp() { return false; }
	};
}

#endif /* _CORE__INCLUDE__BOARD_H_ */
