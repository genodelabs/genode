/*
 * \brief   Standard ARM v7 2-level page table format
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__PAGE_TABLE_H_
#define _SRC__LIB__HW__SPEC__ARM__PAGE_TABLE_H_

#include <cpu/page_flags.h>

#include <hw/page_table.h>

namespace Hw {
	struct Page_table_descriptor;
	struct Page_table_level_2;
	struct Page_table;
}


struct Hw::Page_table_descriptor : Register<32>
{
	enum { FAULT, TABLE, SECTION };

	struct Type : Bitfield<0, 2> { };

	static bool present(access_t &v) {
		return Type::get(v) != FAULT; }

	static bool table(access_t &v) {
		return Type::get(v) == TABLE; }

	/**
	 * Return TEX value for device-memory
	 */
	static constexpr unsigned device_tex();

	/**
	 * Return whether this is a SMP system
	 */
	static constexpr bool smp();

	/**
	 * Return descriptor value according to physical address 'pa'
	 */
	template <typename T>
	static typename T::access_t create(Page_flags const &f, addr_t pa)
	{
		typename T::access_t v = T::Pa::masked(pa);
		T::S::set(v, smp());
		T::Ng::set(v, !f.global);
		T::Xn::set(v, !f.executable);
		if (f.type == DEVICE) {
			T::Tex::set(v, device_tex());
		} else {
			switch (f.cacheable) {
				case         CACHED: T::Tex::set(v, 5); [[fallthrough]];
				case WRITE_COMBINED: T::B::set(v, 1);   break;
				case       UNCACHED: T::Tex::set(v, 1); break;
			}
		}
		if (f.writeable) if (f.privileged) T::Ap::set(v, 1);
		else              T::Ap::set(v, 3);
		else             if (f.privileged) T::Ap::set(v, 5);
		else              T::Ap::set(v, 2);
		return v;
	}
};


struct Hw::Page_table_level_2
: Hw::Page_table_tpl<Hw::Page_table_descriptor, SIZE_LOG2_4KB, SIZE_LOG2_1MB>
{
	using Base = Page_table_tpl<Page_table_descriptor, SIZE_LOG2_4KB,
	                            SIZE_LOG2_1MB>;
	using desc_t = typename Page_table_descriptor::access_t;

	struct Descriptor : Page_table_descriptor
	{
		struct Xn    : Bitfield<0, 1> { };       /* execute never */
		struct Small : Bitfield<1, 1> { };
		struct B     : Bitfield<2, 1> { };       /* mem region attr. */
		struct Ap_0  : Bitfield<4, 2> { };       /* access permission */
		struct Tex   : Bitfield<6, 3> { };       /* mem region attr. */
		struct Ap_1  : Bitfield<9, 1> { };       /* access permission */
		struct S     : Bitfield<10, 1> { };      /* shareable bit */
		struct Ng    : Bitfield<11, 1> { };      /* not global bit */
		struct Pa    : Bitfield<12, 20> { };     /* physical base */
		struct Ap    : Bitset_2<Ap_0, Ap_1> { }; /* access permission */

		/**
		 * Return page descriptor for physical address 'pa' and 'flags'
		 */
		static access_t create(Page_flags const &flags, addr_t const pa)
		{
			access_t v = Page_table_descriptor::create<Descriptor>(flags, pa);
			Small::set(v, 1);
			return v;
		}
	};

	/**
	 * Insert one atomic translation into this table
	 *
	 * \param vo    offset of the virtual region represented
	 *              by the translation within the virtual
	 *              region represented by this table
	 * \param pa    base of the physical backing store
	 * \param size  size of the translated region
	 * \param flags mapping flags
	 */
	Result insert(addr_t vo, addr_t pa, size_t size, Page_flags const &flags)
	{
		return Base::_for_range(vo, pa, size,
			[&] (addr_t const vo, addr_t const pa,
			     size_t const size, desc_t &desc) -> Result
			{
				if (!Base::_aligned_and_fits(vo, pa, size))
					return Page_table_error::INVALID_RANGE;

				desc_t table_entry = Descriptor::create(flags, pa);

				if (Descriptor::present(desc) && desc != table_entry)
					return Page_table_error::INVALID_RANGE;

				desc = table_entry;
				return Ok();
			});
	}

	/**
	 * Remove translations that overlap with a given virtual region
	 *
	 * \param vo    region offset within the tables virtual region
	 * \param size  region size
	 */
	void remove(addr_t vo, size_t size)
	{
		Base::_for_range(vo, size, [] (addr_t, size_t, desc_t &d) {
			d = 0; });
	}

	/**
	 * Lookup writeable translation
	 *
	 * \param virt  virtual address offset to look for
	 * \param phys  physical address to return
	 * \returns true if lookup was successful, otherwise false
	 */
	Result lookup(addr_t const virt, addr_t &phys)
	{
		return Base::_for_range(virt, 0, 1,
			[&] (addr_t, addr_t, size_t, desc_t &desc) -> Result
			{
				if (Descriptor::present(desc)) {
					desc_t ap = Descriptor::Ap::get(desc);

					phys = Descriptor::Pa::masked(desc);

					if (ap == 1 || ap == 3)
						return Ok();
				}
				return Page_table_error::INVALID_RANGE;
			});
	}
};


struct Hw::Page_table
: Hw::Page_table_tpl<Hw::Page_table_descriptor, SIZE_LOG2_1MB, SIZE_LOG2_4GB>
{
	using Base = Page_table_tpl<Page_table_descriptor, SIZE_LOG2_1MB,
	                            SIZE_LOG2_4GB>;
	using desc_t = typename Page_table_descriptor::access_t;
	using Array = Hw::Page_table_array<sizeof(Page_table_level_2),
	                                   _table_count(_core_vm_size(),
	                                                SIZE_LOG2_1MB)>;

	static constexpr size_t ALIGNM_LOG2 = SIZE_LOG2_16KB;

	/**
	 * Link to a second level translation table
	 */
	struct Table_descriptor : Page_table_descriptor
	{
		struct Pa : Bitfield<10, 22> { }; /* physical base */

		/**
		 * Return descriptor value for page table 'pt
		 */
		static access_t create(addr_t pt)
		{
			access_t v = Pa::masked(pt);
			Page_table_descriptor::Type::set(v, TABLE);
			return v;
		}
	};

	/**
	 * Section translation descriptor
	 */
	struct Section : Page_table_descriptor
	{
		struct B    : Bitfield<2, 1> { };       /* mem. region attr. */
		struct Xn   : Bitfield<4, 1> { };       /* execute never bit */
		struct Ap_0 : Bitfield<10, 2> { };      /* access permission */
		struct Tex  : Bitfield<12, 3> { };      /* mem. region attr. */
		struct Ap_1 : Bitfield<15, 1> { };      /* access permission */
		struct S    : Bitfield<16, 1> { };      /* shared            */
		struct Ng   : Bitfield<17, 1> { };      /* not global        */
		struct Pa   : Bitfield<20, 12> { };     /* physical base     */
		struct Ap   : Bitset_2<Ap_0, Ap_1> { }; /* access permission */

		/**
		 * Return section descriptor for physical address 'pa' and 'flags'
		 */
		static access_t create(Page_flags const &flags, addr_t const pa)
		{
			access_t v = Page_table_descriptor::create<Section>(flags, pa);
			Page_table_descriptor::Type::set(v, SECTION);
			return v;
		}
	};

	static inline void table_changed(addr_t, size_t);

	using Base::Base;

	/**
	 * On ARM we do not need to copy top-level kernel entries
	 * because the virtual-memory kernel part is hold in a separate table
	 */
	explicit Page_table(Page_table&) : Page_table() { }

	/**
	 * Insert translations into this table
	 *
	 * \param vo    offset of virt. transl. region in virt. table region
	 * \param pa    base of physical backing store
	 * \param size  size of translated region
	 * \param f     mapping flags
	 * \param alloc second level translation table allocator
	 */
	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &f, Page_table_allocator &alloc)
	{
		using Pt  = Page_table_level_2;
		using Ptd = Table_descriptor;

		return Base::_for_range(vo, pa, size,
			[&] (addr_t const vo, addr_t const pa,
			     size_t const size, desc_t &desc) -> Result
			{
				/* can we insert a large page mapping? */
				if (Base::_aligned_and_fits(vo, pa, size)) {
					desc_t const blk_desc = Section::create(f, pa);

					if (Page_table_descriptor::present(desc) &&
					    desc != blk_desc)
						return Page_table_error::INVALID_RANGE;

					desc = blk_desc;

					/* some CPUs need to act on changed translations */
					table_changed((addr_t)&desc, sizeof(desc));
					return Ok();
				}

				/* do we need to create and link next level table? */
				if (!Page_table_descriptor::present(desc)) {
					Result r = alloc.create<Pt, Ptd>(desc);
					if (r.failed())
						return r;
					table_changed((addr_t)&desc, sizeof(desc));
				}

				return alloc.lookup<Pt>(Ptd::Pa::masked(desc),
				                           [&] (Pt &table) {
					Result r = table.insert(vo-Base::_page_mask_high(vo),
					                        pa, size, f);
					table_changed((addr_t)&table, sizeof(Pt));
					return r;
				});
			});
	}

	/**
	 * Remove translations that overlap with a given virtual region
	 *
	 * \param vo    region offset within the tables virtual region
	 * \param size  region size
	 * \param alloc second level translation table allocator
	 */
	void remove(addr_t vo, size_t size, Page_table_allocator &alloc)
	{
		using Pt  = Page_table_level_2;
		using Ptd = Table_descriptor;

		Base::_for_range(vo, size, [&] (addr_t const vo, size_t const size,
		                                desc_t &desc)
		{
			if (!Page_table_descriptor::present(desc))
				return;

			if (!Page_table_descriptor::table(desc)) {
				desc = 0;
				table_changed((addr_t)&desc, sizeof(desc));
				return;
			}

			alloc.lookup<Pt>(Ptd::Pa::masked(desc), [&] (Pt &table) {
				table.remove(vo-Base::_page_mask_high(vo), size);
				table_changed((addr_t)&table, sizeof(Pt));
				if (table.empty()) {
					alloc.destroy<Pt>(table);
					desc = 0;
					table_changed((addr_t)&desc, sizeof(desc));
				}
				return Ok();
			}).with_error([] (Page_table_error) {
				/* ignore non-mapped entries */ });
		});
	}

	/**
	 * Lookup writeable translation
	 *
	 * \param virt  virtual address to look at
	 * \param phys  physical address to return
	 * \param alloc second level translation table allocator
	 * \returns true if a translation was found, otherwise false
	 */
	Result lookup(addr_t const virt, addr_t &phys,
	              Page_table_allocator &alloc)
	{
		using Pt  = Page_table_level_2;
		using Ptd = Table_descriptor;

		return Base::_for_range(virt, 0, 1,
			[&] (addr_t vo, addr_t, size_t, desc_t &desc) -> Result
			{
				if (!Page_table_descriptor::table(desc)) {
					Section::access_t ap = Section::Ap::get(desc);
					phys = Section::Pa::masked(desc);

					if (ap == 1 || ap == 3)
						return Ok();
					return Page_table_error::INVALID_RANGE;
				} else {
					return alloc.lookup<Pt>(Ptd::Pa::masked(desc),
					                        [&] (Pt &pt) {
						return pt.lookup(vo-Base::_page_mask_high(vo), phys);
					});
				}
			});
	}
} __attribute__((aligned(1 << Page_table::ALIGNM_LOG2)));

#endif /* _SRC__LIB__HW__SPEC__ARM__PAGE_TABLE_H_ */
