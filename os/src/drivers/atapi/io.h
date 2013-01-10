/*
 * \brief  I/O back-end of the mindrvr driver
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2010-07-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ATA_IO_H_
#define _ATA_IO_H_

#include <io_port_session/connection.h>

namespace Ata {

	class Io_port : public Genode::Io_port_session
	{
		private:

			Genode::Io_port_connection _io_cmd;
			Genode::Io_port_connection _io_ctrl;
			unsigned _base_cmd;  /* command base */
			unsigned _base_ctrl; /* control base */

			unsigned short translate_address(unsigned short address)
			{
				return address < 8 ? _base_cmd + address : _base_ctrl + 6;
			}

			Genode::Io_port_connection * io(unsigned short address)
			{
				return (address < 8) ? &_io_cmd : &_io_ctrl;
			}

		public:

			Io_port(unsigned base, unsigned base_ctrl)
			  : _io_cmd(base, 8), _io_ctrl(base_ctrl, 8),
			    _base_cmd(base), _base_ctrl(base_ctrl) {}

			unsigned char inb(unsigned short address)
			{
				return io(address)->inb(translate_address(address));
			}

			unsigned short inw(unsigned short address)
			{
				return io(address)->inw(translate_address(address));
			}

			unsigned inl(unsigned short address)
			{
				return io(address)->inl(translate_address(address));
			}

			void outb(unsigned short address, unsigned char value)
			{
				io(address)->outb(translate_address(address), value);
			}

			void outw(unsigned short address, unsigned short value)
			{
				io(address)->outw(translate_address(address), value);
			}

			void outl(unsigned short address, unsigned value)
			{
				io(address)->outl(translate_address(address), value);
			}
	};
}

#endif /* _ATA_IO_H_ */
