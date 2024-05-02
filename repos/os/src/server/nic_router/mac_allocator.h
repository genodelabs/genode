/*
 * \brief  MAC-address allocator
 * \author Stefan Kalkowski
 * \date   2010-08-25
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MAC_ALLOCATOR_H_
#define _MAC_ALLOCATOR_H_

/* Genode includes */
#include <base/exception.h>
#include <net/mac_address.h>
#include <util/attempt.h>

namespace Net { class Mac_allocator; }


class Net::Mac_allocator
{
	private:

		Mac_address const _base;
		bool              _free[sizeof(_base.addr[0]) << 8];

	public:

		Mac_allocator(Mac_address base) : _base(base)
		{
			Genode::memset(&_free, true, sizeof(_free));
		}

		struct Alloc_error { };
		using Alloc_result = Genode::Attempt<Mac_address, Alloc_error>;

		Alloc_result alloc()
		{
			for (unsigned id = 0; id < sizeof(_free) / sizeof(_free[0]); id++) {
				if (!_free[id]) {
					continue; }

				_free[id] = false;
				Mac_address mac = _base;
				mac.addr[5] = id;
				return mac;
			}
			return Alloc_error();
		}

		void free(Mac_address mac) { _free[mac.addr[5]] = true; }
};

#endif /* _MAC_ALLOCATOR_H_ */
