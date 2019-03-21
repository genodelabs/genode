/*
 * \brief  UART output driver for RISCV
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__RISCV__UART_H_
#define _SRC__LIB__HW__SPEC__RISCV__UART_H_

#include <hw/spec/riscv/machine_call.h>
#include <util/register.h>

namespace Hw { struct Riscv_uart; }

struct Hw::Riscv_uart
{
	void put_char(char const c)
	{
		Hw::put_char(c);
	}
};

#endif /* _SRC__LIB__HW__SPEC__RISCV__UART_H_ */
