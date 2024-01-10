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
#include <exception.h>

class Ram {

	private:

		using Byte_range_ptr = Genode::Byte_range_ptr;
		using addr_t = Genode::addr_t;
		using size_t = Genode::size_t;

		addr_t const _guest_base;
		Byte_range_ptr const _local_range;

	public:

		Ram(addr_t guest_base, Byte_range_ptr const &local_range)
		: _guest_base(guest_base), _local_range(local_range.start, local_range.num_bytes) { }

		size_t size() const { return _local_range.num_bytes;  }
		addr_t guest_base() const { return _guest_base;  }
		addr_t local_base() const { return (addr_t)_local_range.start; }

		Byte_range_ptr to_local_range(Byte_range_ptr const &guest_range)
		{
			addr_t guest_base = (addr_t)guest_range.start;
			if (guest_base <  _guest_base ||
			    guest_base >= _guest_base + _local_range.num_bytes ||
			    guest_range.num_bytes == 0 ||
			    guest_base + guest_range.num_bytes >= _guest_base + _local_range.num_bytes)
				throw Vmm::Exception("Invalid guest physical address: ",
				                     Genode::Hex(guest_base), " size: ", Genode::Hex(guest_range.num_bytes));

			Genode::off_t offset = guest_base - _guest_base;
			return {_local_range.start + offset, guest_range.num_bytes};
		}
};

#endif /* _SRC__SERVER__VMM__RAM_H_ */
