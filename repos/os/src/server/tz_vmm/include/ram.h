/*
 * \brief  Virtual Machine Monitor RAM definition
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__INCLUDE__RAM_H_
#define _SRC__SERVER__VMM__INCLUDE__RAM_H_

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/stdint.h>
#include <base/exception.h>

class Ram {

	private:

		Genode::Attached_io_mem_dataspace _ds;
		Genode::addr_t const              _base;
		Genode::size_t const              _size;
		Genode::addr_t const              _local;

	public:

		class Invalid_addr : Genode::Exception {};

		Ram(Genode::Env &env, Genode::addr_t base, Genode::size_t size)
		:
			_ds(env, base, size), _base(base), _size(size),
			_local((Genode::addr_t)_ds.local_addr<void>()) { }

		Genode::addr_t base()  const { return _base;  }
		Genode::size_t size()  const { return _size;  }
		Genode::addr_t local() const { return _local; }

		Genode::addr_t va(Genode::addr_t phys) const
		{
			if ((phys < _base) || (phys > (_base + _size)))
				throw Invalid_addr();
			return _local + (phys - _base);
		}
};

#endif /* _SRC__SERVER__VMM__INCLUDE__RAM_H_ */
