/*
 * \brief  RAM session guard
 * \author Stefan Kalkowski
 * \date   2016-06-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__RAM_SESSION_GUARD_H_
#define _INCLUDE__OS__RAM_SESSION_GUARD_H_

#include <base/ram_allocator.h>
#include <pd_session/capability.h>

namespace Genode { struct Ram_session_guard; }


class Genode::Ram_session_guard : public Genode::Ram_allocator
{
	private:

		Ram_allocator &_ram_alloc;

		size_t       _quota;
		size_t       _used = 0;
		size_t       _withdraw = 0;

	public:

		Ram_session_guard(Ram_allocator &ram_alloc, size_t quota)
		: _ram_alloc(ram_alloc), _quota(quota) { }

		/**
		 * Extend allocation limit
		 */
		void upgrade(size_t additional_amount) {
			_quota += additional_amount; }

		/**
		 * Consume bytes without actually allocating them
		 */
		bool withdraw(size_t size)
		{
			if ((_quota - _used) < size)
				return false;

			_used     += size;
			_withdraw += size;
			return true;
		}

		/**
		 * Revert withdraw
		 */
		bool revert_withdraw(size_t size)
		{
			if (size > _withdraw)
				return false;

			_used     -= size;
			_withdraw -= size;

			return true;
		}


		/*****************************
		 ** Ram_allocator interface **
		 *****************************/

		Ram_dataspace_capability alloc(size_t size,
		                               Cache_attribute cached = CACHED) override
		{
			if (_used + size <= _used || _used + size > _quota)
				throw Out_of_ram();

			Ram_dataspace_capability cap = _ram_alloc.alloc(size, cached);

			if (cap.valid())
				_used += size;

			return cap;
		}

		void free(Ram_dataspace_capability ds) override
		{
			size_t size = Dataspace_client(ds).size();
			_ram_alloc.free(ds);
			_used -= size;
		}

		size_t dataspace_size(Ram_dataspace_capability ds) const override
		{
			return _ram_alloc.dataspace_size(ds);
		}
};

#endif /* _INCLUDE__OS__RAM_SESSION_GUARD_H_ */
