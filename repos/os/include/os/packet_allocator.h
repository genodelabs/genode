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
		addr_t         *_bits  = nullptr;  /* memory chunk containing the bits    */
		Bit_array_base *_array = nullptr;  /* bit array managing available blocks */
		addr_t          _base = 0;         /* allocation base                     */
		addr_t          _next = 0;         /* next free bit index                 */

		/*
		 * Returns the count of bits required to use the internal bit
		 * array to track the allocations. The bit count is rounded up/aligned
		 * to the natural machine word bit size.
		 */
		unsigned _bits_cnt(size_t const size)
		{
			unsigned const bits = (unsigned)sizeof(addr_t)*8;
			unsigned const cnt  = (unsigned)(size / _block_size);

			unsigned bits_aligned = cnt / bits;
			if (cnt % bits)
				bits_aligned += 1;

			return bits_aligned * bits;
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

		Range_result add_range(addr_t const base, size_t const size) override
		{
			if (_base || _array)
				return Alloc_error::DENIED;

			unsigned const bits_cnt = _bits_cnt(size);

			_base = base;

			Alloc_error error = Alloc_error::DENIED;

			size_t const bits_bytes = bits_cnt / 8;

			try {
				_bits = (addr_t *)_md_alloc->alloc(bits_bytes);
				memset(_bits, 0, bits_cnt / 8);

				_array = new (_md_alloc) Bit_array_base(bits_cnt, _bits);

				/* reserve bits which are unavailable */
				size_t const max_cnt = size / _block_size;
				if (bits_cnt > max_cnt)
					_array->set(max_cnt, bits_cnt - max_cnt);

				return Range_ok();

			}
			catch (Out_of_ram)  { error = Alloc_error::OUT_OF_RAM; }
			catch (Out_of_caps) { error = Alloc_error::OUT_OF_CAPS; }
			catch (...)         { error = Alloc_error::DENIED; }

			if (_bits)
				_md_alloc->free(_bits, bits_bytes);

			if (_array)
				destroy(_md_alloc, _array);

			return error;
		}

		Range_result remove_range(addr_t base, size_t size) override
		{
			if (_base != base)
				return Alloc_error::DENIED;

			_base = _next = 0;

			if (_array) {
				destroy(_md_alloc, _array);
				_array = nullptr;
			}

			if (_bits)  {
				_md_alloc->free(_bits, _bits_cnt(size)/8);
				_bits = nullptr;
			}

			return Range_ok();
		}

		Alloc_result alloc_aligned(size_t size, unsigned, Range) override
		{
			return try_alloc(size);
		}

		Alloc_result try_alloc(size_t size) override
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
						return reinterpret_cast<void *>(i * _block_size
						                                + _base);
					}
				} catch (typename Bit_array_base::Invalid_index_access) { }

				max = _next;
				_next = 0;

			} while (max != 0);

			return Alloc_error::DENIED;
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

		bool need_size_for_free() const override { return true; }
		void free(void *) override { }
		size_t overhead(size_t) const override {  return 0;}
		size_t avail() const override { return 0; }
		bool valid_addr(addr_t) const override { return 0; }
		Alloc_result alloc_addr(size_t, addr_t) override {
			return Alloc_error::DENIED; }
};

#endif /* _INCLUDE__OS__PACKET_ALLOCATOR__ */
