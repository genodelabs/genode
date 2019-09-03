/*
 * \brief  VMM ram object
 * \author Stefan Kalkowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__RAM_H_
#define _SRC__SERVER__VMM__RAM_H_

#include <base/stdint.h>

class Ram {

	private:

		Genode::addr_t const _base;
		Genode::size_t const _size;
		Genode::addr_t const _local;

	public:

		Ram(Genode::addr_t const addr,
		    Genode::size_t const sz,
		    Genode::addr_t const local)
		: _base(addr), _size(sz), _local(local) { }

		Genode::addr_t base()  const { return _base;  }
		Genode::size_t size()  const { return _size;  }
		Genode::addr_t local() const { return _local; }
};

#endif /* _SRC__SERVER__VMM__RAM_H_ */
