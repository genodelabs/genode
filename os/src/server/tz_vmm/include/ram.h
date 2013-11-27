/*
 * \brief  Virtual Machine Monitor RAM definition
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__SERVER__VMM__INCLUDE__RAM_H_
#define _SRC__SERVER__VMM__INCLUDE__RAM_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/exception.h>

class Ram {

	private:

		Genode::addr_t _base;
		Genode::size_t _size;
		Genode::addr_t _local;

	public:

		class Invalid_addr : Genode::Exception {};

		Ram(Genode::addr_t addr, Genode::size_t sz, Genode::addr_t local)
		: _base(addr), _size(sz), _local(local) { }

		Genode::addr_t base()  { return _base;  }
		Genode::size_t size()  { return _size;  }
		Genode::addr_t local() { return _local; }

		Genode::addr_t va(Genode::addr_t phys)
		{
			if ((phys < _base) || (phys > (_base + _size)))
				throw Invalid_addr();
			return _local + (phys - _base);
		}
};

#endif /* _SRC__SERVER__VMM__INCLUDE__RAM_H_ */
