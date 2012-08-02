/*
 * \brief  Fast-bitmap allocator for NIC-session packet streams
 * \author Sebastian Sumpf
 * \date   2012-07-30
 *
 * This allocator can be used with a nic session. It is *not* required though.
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC__PACKET_ALLOCATOR__
#define _INCLUDE__NIC__PACKET_ALLOCATOR__

#include <base/allocator.h>
#include <util/string.h>

namespace Nic {

	class Packet_allocator : public Genode::Range_allocator
	{
		private:

			Genode::Allocator *_md_alloc;   /* meta-data allocator */
			unsigned           _block_size; /* network packet size */
			Genode::addr_t     _base;       /* allocation base */
			unsigned          *_free;       /* free list */
			int                _curr_idx;   /* current index in free list */
			int                _count;      /* number of elements */

		public:

			enum { DEFAULT_PACKET_SIZE = 1600 };

			/**
			 * Constructor
			 *
			 * \param md_alloc       Meta-data allocator
			 * \param block_size     Size of network packet in stream
			 */
			Packet_allocator(Genode::Allocator *md_alloc,
			                 unsigned block_size = DEFAULT_PACKET_SIZE)
			: _md_alloc(md_alloc), _block_size(block_size), _base(0), _free(0),
			  _curr_idx(0), _count(0)
			{}

			~Packet_allocator()
			{
				if (_free)
					_md_alloc->free(_free, sizeof(unsigned) * (_count / 32));
			}


			/*******************************
			 ** Range-allocator interface **
			 *******************************/

			int add_range(Genode::addr_t base, Genode::size_t size)
			{
				if (_count)
					return -1;

				_base  = base;
				_count = size / _block_size;
				_free  = (unsigned *)_md_alloc->alloc(sizeof(unsigned) * (_count / 32));

				Genode::memset(_free, 0xff, sizeof(unsigned) * (_count / 32));
				return 0;
			}

			bool alloc_aligned(Genode::size_t size, void **out_addr, int) {
				return alloc(size, out_addr); }

			bool alloc(Genode::size_t size, void **out_addr)
			{ 
				int free_cnt = _count / 32;

				for (register int i = 0; i < free_cnt; i++) {

					if (_free[_curr_idx] != 0) {
						unsigned msb = Genode::log2(_free[_curr_idx]);
						int elem_idx = (_curr_idx * 32) + msb;

						if (elem_idx < _count) {
							_free[_curr_idx] ^= (1 << msb);
							*out_addr = reinterpret_cast<void *>(_base + (_block_size * elem_idx));
							return true;
						}
					}

					_curr_idx = (_curr_idx + 1) % free_cnt;
				}
				return false;
			}

			void free(void *addr)
			{
				Genode::addr_t a = reinterpret_cast<Genode::addr_t>(addr);

				int elem_idx = (a - _base) / _block_size;
				if (elem_idx >= _count)
					return;

				_curr_idx = elem_idx / 32;
				_free[_curr_idx] |= (1 << (elem_idx % 32));
			}

			void free(void *addr, Genode::size_t) { free(addr); }


			/*********************
			 ** Dummy functions **
			 *********************/

			Genode::size_t overhead(Genode::size_t) {  return 0;}
			int remove_range(Genode::addr_t, Genode::size_t) { return 0;}
			Genode::size_t avail() { return 0; }
			bool valid_addr(Genode::addr_t) { return 0; }
			Genode::Range_allocator::Alloc_return alloc_addr(Genode::size_t, Genode::addr_t) {
				return OUT_OF_METADATA; }
	};
};

#endif /* _INCLUDE__NIC__PACKET_ALLOCATOR__ */
