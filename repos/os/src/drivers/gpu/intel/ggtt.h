/*
 * \brief  Intel global graphics translation table
 * \author Josef Soentgen
 * \date   2017-03-15
 */

/*
 * Copyright (C) 2017-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GLOBAL_GTT_H_
#define _GLOBAL_GTT_H_

/* Genode includes */
#include <util/bit_array.h>

/* local includes */
#include <mmio.h>
#include <platform_session.h>


namespace Igd { struct Ggtt; }

/*
 * Global Graphics Translation Table
 */
class Igd::Ggtt
{
	public:

		struct Could_not_find_free    : Genode::Exception { };
		struct Offset_out_of_range    : Genode::Exception { };
		struct Wrong_graphics_address : Genode::Exception { };

		using Offset = Igd::uint32_t;

		struct Mapping
		{
			Genode::Dataspace_capability cap { };
			Offset                       offset;
			Igd::addr_t                  vaddr = 0;

			enum { INVALID_OFFSET = ~0u - 1, };

			Mapping() : offset(INVALID_OFFSET) { }

			Mapping(Genode::Dataspace_capability cap, Offset offset)
			: cap(cap), offset(offset) { }

			virtual ~Mapping() { }
		};

	private:

		/*
		 * Noncopyable
		 */
		Ggtt(Ggtt const &);
		Ggtt &operator = (Ggtt const &);

		/*
		 * IHD-OS-BDW-Vol 5-11.15 p. 44
		 */
		struct Page_table_entry : Genode::Register<64>
		{
			struct Physical_adress : Bitfield<12, 36> { };
			struct Present         : Bitfield< 0,  1> { };
		};

		struct Space_allocator
		{
			Genode::Bit_array<1024*1024> _array { };
			size_t                       _used  { 0 };

			bool allocated(addr_t const index) const
			{
				return _array.get(index, 1);
			}

			void set(addr_t const index)
			{
				_used++;
				_array.set(index, 1);
			}

			void clear(addr_t const index)
			{
				_used--;
				_array.clear(index, 1);
			}

			unsigned free_index(unsigned start_index, unsigned end_index,
			                    size_t const num) const
			{
				for (unsigned index = start_index; index < end_index; index += num) {
					if (index + num >= end_index)
						throw Could_not_find_free();
					if (!_array.get(index, num))
						return index;
				}

				throw Could_not_find_free();
			}

			size_t used() const { return _used; }

		} _space { };

		addr_t const _base;
		size_t const _size;
		size_t const _num_entries;

		uint64_t *_entries;

		Platform::Dma_buffer _scratch_page;

		size_t const _aperture_size;
		size_t const _aperture_entries;

		void _insert_pte(Igd::Mmio &mmio, addr_t const pa, Offset const &offset)
		{
			Page_table_entry::access_t pte = 0;
			Page_table_entry::Physical_adress::set(pte, pa >> 12);
			Page_table_entry::Present::set(pte, 1);

			_entries[offset] = pte;
			mmio.flush_gfx_tlb();
			Igd::wmb();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param platform       reference to Platform::Connection object
		 * \param mmio           reference to Igd::Mmio object
		 * \param base           virtual base address of GGTT start
		 * \param size           size of GGTT in bytes
		 * \param aperture_size  size of the aperture in bytes
		 * \param scratch_page   physical address of the scratch page
		 * \param fb_size        size of the framebuffer region in the GTT in bytes
		 */
		Ggtt(Platform::Connection & platform, Igd::Mmio &mmio,
		     addr_t base, size_t size, size_t aperture_size, size_t fb_size)
		:
			_base(base),
			_size(size),
			/* make the last entry/page unavailable */
			_num_entries((_size / 8) - 1),
			_entries((uint64_t*)_base),
			_scratch_page(platform, PAGE_SIZE, Genode::CACHED),
			_aperture_size(aperture_size),
			_aperture_entries(_aperture_size / PAGE_SIZE)
		{
			/* reserve GGTT region occupied by the framebuffer */
			size_t const fb_entries = fb_size / PAGE_SIZE;
			for (size_t i = 0; i < fb_entries; i++) {
				_space.set(i);
			}
			for (size_t i = fb_entries; i < _num_entries; i++) {
				_insert_pte(mmio, _scratch_page.dma_addr(), i);
			}
		}

		/**
		 * Insert page into GGTT
		 *
		 * \param pa      physical address
		 * \param offset  offset of GTT entry
		 *
		 * \throw Offset_out_of_range
		 */
		void insert_pte(Igd::Mmio &mmio, addr_t const &pa, Offset const &offset)
		{
			if (offset > _num_entries) { throw Offset_out_of_range(); }

			if (_space.allocated(offset)) {
				Genode::warning(__func__, " offset:", offset, " already used");
			}

			_space.set(offset);
			_insert_pte(mmio, pa, offset);
		}

		/**
		 * Remove page from GTT
		 *
		 * \param offset  offset of GTT entry
		 */
		void remove_pte(Igd::Mmio &mmio, Offset const &offset)
		{
			if (!_space.allocated(offset)) {
				Genode::warning(__func__, " offset:", offset, " was not used");
				return;
			}

			_space.clear(offset);
			_insert_pte(mmio, _scratch_page.dma_addr(), offset);
		}

		/**
		 * Remove range of pages from GTT
		 *
		 * \param start  offset of the first page in the GGTT
		 * \param num    number of pages
		 */
		void remove_pte_range(Igd::Mmio &mmio, Offset const start, Offset const num)
		{
			Offset const end = start + num;
			for (Offset offset = start; offset < end; offset++) {
				remove_pte(mmio, offset);
			}
		}

		/**
		 * Find free GGTT entries
		 *
		 * \param num       number of continuous entries
		 * \param aperture  request entries mappable by CPU
		 *
		 * \return start offset of free entry range
		 *
		 * \throw Could_not_find_free
		 */
		Offset find_free(size_t num, bool aperture = false) const
		{
			size_t const start_index = aperture ?                 0 : _aperture_entries;
			size_t const end_index   = aperture ? _aperture_entries : _num_entries;

			return _space.free_index(start_index, end_index, num);
		}

		/**
		 * Get GGTT entry offset from GMADDR
		 *
		 * \param gmaddr  graphics memory address
		 *
		 * \return GGTT offset of GMADDR
		 *
		 * \throw Wrong_graphics_address
		 */
		Offset offset(addr_t const gmaddr) const
		{
			if (gmaddr & 0xfffu)      { throw Wrong_graphics_address(); }
			if (gmaddr > 0xffffffffu) { throw Wrong_graphics_address(); }
			return gmaddr / PAGE_SIZE;
		}

		/**
		 * Get GMADDR for GGTT entry offset
		 *
		 * \param offset  offset of GGTT entry
		 *
		 * \return graphics memory address
		 *
		 * \throw Offset_out_of_range
		 */
		addr_t addr(Offset offset) const
		{
			if (offset > _num_entries) { throw Offset_out_of_range(); }
			return offset * PAGE_SIZE;
		}

		/**
		 * Get total number of entries in GGTT
		 *
		 * \return number of entries
		 */
		size_t entries() const { return _num_entries; }

		/*********************
		 ** Debug interface **
		 *********************/

		void dump(bool const dump_entries = false,
		          size_t const limit = 0u,
		          size_t const start = 0u) const
		{
			using namespace Genode;

			log("GGTT");
			log("  vaddr:", Hex(_base), " size:", Hex(_size), " entries:", _num_entries,
			    " used:", _space.used(), " aperture_size:", Hex(_aperture_size));
			log("  scratch_page:", Hex(_scratch_page.dma_addr()), " (PA)");

			if (!dump_entries) { return; }

			log("  entries:");
			uint64_t *ggtt = (uint64_t*)_base;
			size_t const max = !limit ? _num_entries : limit > _num_entries ? _num_entries : limit;
			for (size_t i = start; i < start + max; i += 8) {
				log("  ", Hex(i, Hex::PREFIX, Hex::PAD), " ",
				    Hex(ggtt[i]),     " ", Hex(ggtt[i + 1]), " ",
				    Hex(ggtt[i + 2]), " ", Hex(ggtt[i + 3]), " ",
				    Hex(ggtt[i + 4]), " ", Hex(ggtt[i + 5]), " ",
				    Hex(ggtt[i + 6]), " ", Hex(ggtt[i + 7]));
			}
		}
};

#endif /* _GLOBAL_GTT_H_ */
