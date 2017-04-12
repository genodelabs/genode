/*
 * \brief  Serial output driver for core
 * \author Sebastian Sumpf
 * \date   2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__RISCV__SERIAL_H_
#define _CORE__SPEC__RISCV__SERIAL_H_

#include <hw/spec/riscv/uart.h>

namespace Genode { struct Serial; }

/**
 * Serial output driver for core
 */
struct Genode::Serial : Hw::Riscv_uart
{
	Serial(unsigned) { }
};

#endif /* _CORE__SPEC__RISCV__SERIAL_H_ */
