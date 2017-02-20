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

class Serial_interface
{
	public:

		virtual unsigned char read() = 0;
		virtual void write(unsigned char) = 0;
		virtual bool data_read_ready() = 0;

		/**
		 * (Re-)enable device interrupt
		 */
		virtual void enable_irq() { }

		virtual ~Serial_interface() { }
};

#endif /* _SERIAL_INTERFACE_H_ */
