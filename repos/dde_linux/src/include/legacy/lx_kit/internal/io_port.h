/*
 * \brief  I/O port access helper
 * \author Sebastian Sumpf
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__INTERNAL__IO_PORT_H_
#define _LX_KIT__INTERNAL__IO_PORT_H_

/* Genode includes */
#include <util/reconstructible.h>
#include <io_port_session/client.h>

namespace Lx { class Io_port; }

class Lx::Io_port
{
	private:

		unsigned                                              _base = 0;
		unsigned                                              _size = 0;
		Genode::Io_port_session_capability                    _cap;
		Genode::Constructible<Genode::Io_port_session_client> _port;

		bool _valid(unsigned port) {
			return _cap.valid() && port >= _base && port < _base + _size; }

	public:

		~Io_port()
		{
			if (_cap.valid())
				_port.destruct();
		}

		void session(unsigned base, unsigned size, Genode::Io_port_session_capability cap)
		{
			_base = base;
			_size = size;
			_cap  = cap;
			_port.construct(_cap);
		}

		template<typename POD>
		bool out(unsigned port, POD val)
		{
			if (!_valid(port))
				return false;

			switch (sizeof(POD)) {
				case 1: _port->outb(port, val); break;
				case 2: _port->outw(port, val); break;
				case 4: _port->outl(port, val); break;
				default:
					return false;
			}

			return true;
		}

		template<typename POD>
		bool in(unsigned port, POD *val)
		{
			if (!_valid(port))
				return false;

			switch (sizeof(POD)) {
				case 1: *val = _port->inb(port); break;
				case 2: *val = _port->inw(port); break;
				case 4: *val = _port->inl(port); break;
				default:
					return false;
			}

			return true;
		}
};

#endif /* _LX_KIT__INTERNAL__IO_PORT_H_ */
