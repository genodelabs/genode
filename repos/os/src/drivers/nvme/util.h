/*
 * \brief  Utilitize used by the NVMe driver
 * \author Josef Soentgen
 * \date   2018-03-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NVME_UTIL_H_
#define _NVME_UTIL_H_

/* Genode includes */
#include <base/fixed_stdint.h>

namespace Util {

	using namespace Genode;

	/*
	 * DMA allocator helper
	 */
	struct Dma_allocator : Genode::Interface
	{
		virtual Genode::Ram_dataspace_capability alloc(size_t) = 0;
		virtual void free(Genode::Ram_dataspace_capability) = 0;
	};

	/*
	 * Wrap Bit_array into a convinient Bitmap allocator
	 */
	template <unsigned BITS>
	struct Bitmap
	{
		struct Full : Genode::Exception { };

		static constexpr addr_t INVALID { BITS - 1 };
		Genode::Bit_array<BITS> _array { };
		size_t                  _used  { 0 };

		addr_t _find_free(size_t const bits)
		{
			for (size_t i = 0; i < BITS; i += bits) {
				if (_array.get(i, bits)) { continue; }
				return i;
			}
			throw Full();
		}

		/**
		 * Return index from where given number of bits was allocated
		 *
		 * \param bits  number of bits to allocate
		 *
		 * \return index of start bit
		 */
		addr_t alloc(size_t const bits)
		{
			addr_t const start = _find_free(bits);
			_array.set(start, bits);
			_used += bits;
			return start;
		}

		/**
		 * Free given number of bits from start index
		 *
		 * \param start  index of the start bit
		 * \param bits   number of bits to free
		 */
		void free(addr_t const start, size_t const bits)
		{
			_used -= bits;
			_array.clear(start, bits);
		}
	};

	/*
	 * Wrap array into convinient interface
	 *
	 * The used datatype T must implement the following methods:
	 *
	 *    bool valid() const   returns true if the object is valid
	 *    void invalidate()    adjusts the object so that valid() returns false
	 */
	template <typename T, size_t CAP>
	struct Slots
	{
		T _entries[CAP] { };

		/**
		 * Lookup slot
		 */
		template <typename FUNC>
		T *lookup(FUNC const &func)
		{
			for (size_t i = 0; i < CAP; i++) {
				if (!_entries[i].valid()) { continue; }
				if ( func(_entries[i])) { return &_entries[i]; }
			}
			return nullptr;
		}

		/**
		 * Get free slot
		 */
		T *get()
		{
			for (size_t i = 0; i < CAP; i++) {
				if (!_entries[i].valid()) { return &_entries[i]; }
			}
			return nullptr;
		}

		/**
		 * Iterate over all slots until FUNC returns true
		 */
		template <typename FUNC>
		bool for_each(FUNC const &func)
		{
			for (size_t i = 0; i < CAP; i++) {
				if (!_entries[i].valid()) { continue; }
				if ( func(_entries[i])) { return true; }
			}
			return false;
		}
	};

	/**
	 * Extract string from memory
	 *
	 * This function is used to extract the information strings from the
	 * identify structure.
	 */
	char const *extract_string(char const *base, size_t offset, size_t len)
	{
		static char tmp[64] = { };
		if (len > sizeof(tmp)) { return nullptr; }

		Genode::strncpy(tmp, base + offset, len);

		len--; /* skip NUL */
		while (len > 0 && tmp[--len] == ' ') { tmp[len] = 0; }
		return tmp;
	}
}

#endif /* _NVME_UTIL_H_ */
