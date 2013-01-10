/*
 * \brief  Board driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* core includes */
#include <drivers/board_base.h>

namespace Genode
{
	class Board : public Board_base
	{
		public:

			static void prepare_kernel() { }
	};
}

#endif /* _BOARD_H_ */

