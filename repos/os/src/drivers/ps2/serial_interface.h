/*
 * \brief  Driver-internal serial interface
 * \author Norman Feske
 * \date   2007-09-28
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SERIAL_INTERFACE_H_
#define _SERIAL_INTERFACE_H_

/* Genode includes */
#include <util/interface.h>

struct Serial_interface : Genode::Interface
{
	virtual unsigned char read() = 0;
	virtual void write(unsigned char) = 0;
	virtual bool data_read_ready() = 0;

	/**
	 * (Re-)enable device interrupt
	 */
	virtual void enable_irq() { }

	protected:

		virtual void _begin_commands() { }
		virtual void _end_commands() { }

	public:

		/**
		 * Don't interfere with incoming events on command sequence
		 */
		template <typename FN>
		void apply_commands(FN const &fn)
		{
			_begin_commands();
			fn();
			_end_commands();
		}
};

#endif /* _SERIAL_INTERFACE_H_ */
