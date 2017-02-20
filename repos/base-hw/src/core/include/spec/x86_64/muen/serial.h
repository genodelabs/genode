/*
 * \brief  Serial output driver for core
 * \author Stefan Kalkowski
 * \author Adrian-Ken Rueegsegger
 * \date   2015-08-20
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__X86_64__MUEN__SERIAL_H_
#define _CORE__INCLUDE__SPEC__X86_64__MUEN__SERIAL_H_

/* Genode includes */
#include <drivers/uart_base.h>

namespace Genode { class Serial; }

/**
 * Serial output driver for core
 */
class Genode::Serial : public X86_uart_base
{
	private:

		enum {
			CLOCK_UNUSED = 0,
			COM1_PORT    = 0x3f8
		};

	public:

		Serial(unsigned baud_rate)
		:
			X86_uart_base(COM1_PORT, CLOCK_UNUSED, baud_rate)
		{ }
};

#endif /* _CORE__INCLUDE__SPEC__X86_64__MUEN__SERIAL_H_ */
