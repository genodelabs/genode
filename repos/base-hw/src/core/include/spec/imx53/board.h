/*
 * \brief  Board driver
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__IMX53__BOARD_H_
#define _CORE__INCLUDE__SPEC__IMX53__BOARD_H_

/* core includes */
#include <spec/imx/board_support.h>

namespace Genode
{
	/**
	 * Board driver
	 */
	class Board : public Imx::Board
	{
		public:

			bool is_smp() { return false; }
   	};
}

#endif /* _CORE__INCLUDE__SPEC__IMX53__BOARD_H_ */
