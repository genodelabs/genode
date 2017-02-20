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

#ifndef _SERIAL_H_
#define _SERIAL_H_

/* Genode includes */
#include <base/stdint.h>
#include <util/register.h>

/* core includes */
#include <machine_call.h>

namespace Genode { class Serial; }

/**
 * Serial output driver for core
 */
class Genode::Serial
{
	public:

		/**
		 * Constructor
		 */
		Serial(unsigned) { }

		void put_char(char const c)
		{
			struct Arg : Register<64>
			{
				struct Char      : Bitfield<0,  8> { };
				struct Write_cmd : Bitfield<48, 1> { };
				struct Stdout    : Bitfield<56, 1> { };
			};

			Machine::put_char(Arg::Char::bits(c) |
			                  Arg::Stdout::bits(1) |
			                  Arg::Write_cmd::bits(1));
		}
};

#endif /* _SERIAL_H_ */
