/*
 * \brief   Riscv spike specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__RISCV__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__RISCV__BOARD_H_

#include <hw/spec/riscv/board.h>

namespace Board { using namespace Hw::Riscv_board; }

template <typename E, unsigned B, unsigned S>
void Sv39::Level_x_translation_table<E, B, S>::_translation_added(addr_t, size_t)
{ }

#endif /* _SRC__BOOTSTRAP__SPEC__RISCV__BOARD_H_ */
