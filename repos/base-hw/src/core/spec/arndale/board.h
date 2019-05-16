/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2017-04-27
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARNDALE__BOARD_H_
#define _CORE__SPEC__ARNDALE__BOARD_H_

#include <hw/spec/arm/arndale_board.h>

namespace Board {
	using namespace Hw::Arndale_board;

	static constexpr bool SMP = true;
}

#endif /* _CORE__SPEC__ARNDALE__BOARD_H_ */
