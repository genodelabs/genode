/*
 * \brief  Helper for allocating domain IDs
 * \author Johannes Schlatow
 * \date   2023-08-31
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__DOMAIN_ALLOCATOR_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__DOMAIN_ALLOCATOR_H_

/* Genode includes */
#include <util/bit_allocator.h>
#include <util/misc_math.h>

namespace Intel {
	using namespace Genode;

	struct Domain_id;
	class  Domain_allocator;

	struct Out_of_domains { };
}


struct Intel::Domain_id
{
	uint16_t value;

	enum {
		INVALID = 0,
		MAX = (1 << 16)-1
	};

	bool valid() {
		return value != INVALID; }

	Domain_id() : value(INVALID) { }

	Domain_id(size_t v)
	: value(static_cast<uint16_t>(min((size_t)MAX, v)))
	{
		if (v > MAX)
			warning("Clipping domain id: ", v, " -> ", value);
	}
};


class Intel::Domain_allocator
{
	private:

		using Bit_allocator = Genode::Bit_allocator<Domain_id::MAX+1>;

		Domain_id      _max_id;
		Bit_allocator  _allocator { };

	public:

		Domain_allocator(size_t max_id)
		: _max_id(max_id)
		{ }

		Domain_id alloc()
		{
			try {
				addr_t new_id = _allocator.alloc() + 1;

				if (new_id > _max_id.value) {
					_allocator.free(new_id - 1);
					throw Out_of_domains();
				}

				return Domain_id { new_id };
			} catch (typename Bit_allocator::Out_of_indices) { }

			throw Out_of_domains();
		}

		void free(Domain_id domain)
		{
			if (domain.valid())
				_allocator.free(domain.value - 1);
		}

};

#endif /* _SRC__DRIVERS__PLATFORM__INTEL__DOMAIN_ALLOCATOR_H_ */
