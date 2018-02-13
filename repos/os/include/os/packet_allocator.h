/*
 * \brief  Fast-bitmap allocator for packet streams
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2012-07-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__PACKET_ALLOCATOR__
#define _INCLUDE__OS__PACKET_ALLOCATOR__

#include <base/allocator.h>
#include <util/bit_array.h>

namespace Genode { class Packet_allocator; }


/**
 * This allocator is designed to be used as packet allocator for the
 * packet stream interface. It uses a minimal block size, which is the
 * granularity packets will be allocated with. As backend, it uses a
 * simple bit array to manage free, and allocated blocks.
 */
class Genode::Packet_allocator : public Genode::Range_allocator
{
	private:

		/*
		 * Noncopyable
		 */
		Packet_allocator(Packet_allocator const &);
		Packet_allocator &operator = (Packet_allocator const &);

		Allocator      *_md_alloc;         /* meta-data allocator                 */
		size_t          _block_size;       /* granularity of packet allocations   */
		void           *_bits  = nullptr;  /* memory chunk containing the bits    */
		Bit_array_base *_array = nullptr;  /* bit array managing available blocks */
		addr_t          _base = 0;         /* allocation base                     */
		addr_t          _next = 0;         /* next free bit index                 */

		/*
		 * Returns the count of blocks fitting the given size
		 *
		 * The block count returned is aligned to the bit count
		 * of a machine word to fit the needs of the used bit array.
		 */
		inline size_t _block_cnt(size_t bytes)
		{
			bytes /= _block_size;
			return bytes - (bytes % (sizeof(addr_t)*8));
		}

	public:

		/**
		 * Constructor
		 *
		 * \param md_alloc       Meta-data allocator
		 * \param block_size     Granularity of packets in stream
		 */
		Packet_allocator(Allocator *md_alloc, size_t block_size)
		: _md_alloc(md_alloc), _block_size(block_size) { }


		/*******************************
		 ** Range-allocator interface **
		 *******************************/

		int add_range(addr_t base, size_t size) override
		{
			if (_base || _array) return -1;

			_base  = base;
			_bits  = _md_alloc->alloc(_block_cnt(size)/8);
			_array = new (_md_alloc) Bit_array_base(_block_cnt(size),
			                                        (addr_t*)_bits,
			                                        true);
			return 0;
		}

		int remove_range(addr_t base, size_t size) override
		{
			if (_base != base) return -1;

			_base = _next = 0;

			if (_array) {
				destroy(_md_alloc, _array);
				_array = nullptr;
			}

			if (_bits)  {
				_md_alloc->free(_bits, _block_cnt(size)/8);
				_bits = nullptr;
			}

			return 0;
		}

		Alloc_return alloc_aligned(size_t size, void **out_addr, int, addr_t,
			                       addr_t) override
		{
			return alloc(size, out_addr) ? Alloc_return::OK
			                             : Alloc_return::RANGE_CONFLICT;
		}

		bool alloc(size_t size, void **out_addr) override
		{
			addr_t const cnt = (size % _block_size) ? size / _block_size + 1
			                                        : size / _block_size;
			addr_t max = ~0UL;

			do {
				try {
					/* throws exception if array is accessed outside bounds */
					for (addr_t i = _next & ~(cnt - 1); i < max; i += cnt) {
						if (_array->get(i, cnt))
							continue;

						_array->set(i, cnt);
						_next = i + cnt;
						*out_addr = reinterpret_cast<void *>(i * _block_size
						                                     + _base);
						return true;
					}
				} catch (typename Bit_array_base::Invalid_index_access) { }

				max = _next;
				_next = 0;

			} while (max != 0);

			return false;
		}

		void free(void *addr, size_t size) override
		{
			addr_t i   = (((addr_t)addr) - _base) / _block_size;
			size_t cnt = (size % _block_size) ? size / _block_size + 1
			                                  : size / _block_size;
			try { _array->clear(i, cnt); } catch(...) { }
			_next = i;
		}


		/*************
		 ** Dummies **
		 *************/

		bool need_size_for_free() const override { return false; }
		void free(void *) override { }
		size_t overhead(size_t) const override {  return 0;}
		size_t avail() const override { return 0; }
		bool valid_addr(addr_t) const override { return 0; }
		Alloc_return alloc_addr(size_t, addr_t) override {
			return Alloc_return(Alloc_return::OUT_OF_METADATA); }
};

#endif /* _INCLUDE__OS__PACKET_ALLOCATOR__ */
