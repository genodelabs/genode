/*
 * \brief  MMIO and IRQ definitions for RISC-V (1.9.1)
 * \author Sebastian Sumpf
 * \date   2017-05-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__RISCV_H_
#define _INCLUDE__DRIVERS__DEFS__RISCV_H_

namespace Riscv {
	enum {
		RAM_0_BASE = 0x81000000,
		RAM_0_SIZE = 0x6e00000,
	};
}

#endif /* _INCLUDE__DRIVERS__DEFS__RISCV_H_ */

