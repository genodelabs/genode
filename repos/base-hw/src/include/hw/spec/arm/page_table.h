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
	struct Page_table_descriptor_level2;
	struct Page_table_descriptor_level1;

	using Page_table_level_2 = Page_table_leaf<Page_table_descriptor_level2,
	                                           SIZE_LOG2_4KB, SIZE_LOG2_1MB>;

	struct Page_table;
}


struct Hw::Page_table_descriptor : Genode::Register<32>
{
	struct Type : Bitfield<0, 2> { };

	static Page_table_entry type(access_t const v)
	{
		switch (Type::get(v)) {
		case 0:  return Page_table_entry::INVALID;
		case 1:  return Page_table_entry::TABLE;
		default: break;
		};
		return Page_table_entry::BLOCK;
	}

	static bool present(access_t const v) {
		return type(v) != Page_table_entry::INVALID; }


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

	static bool conflicts(access_t const old, access_t const desc) {
		return present(old) && (old != desc); }

};


struct Hw::Page_table_descriptor_level2 : Hw::Page_table_descriptor
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
		access_t v = Page_table_descriptor::create<Page_table_descriptor_level2>(flags, pa);
		Small::set(v, 1);
		return v;
	}

	static bool writeable(access_t const desc) {
		return Ap::get(desc) == 1 || Ap::get(desc) == 3; }

	static addr_t address(access_t const desc) { return Pa::masked(desc); }
};


struct Hw::Page_table_descriptor_level1 : Hw::Page_table_descriptor
{
	struct Table_pa : Bitfield<10, 22> { }; /* physical base */

	/**
	 * Return descriptor value for page table 'pt
	 */
	static access_t create(addr_t pt)
	{
		access_t v = Table_pa::masked(pt);
		Page_table_descriptor::Type::set(v, 1);
		return v;
	}

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
		access_t v = Page_table_descriptor::create<Page_table_descriptor_level1>(flags, pa);
		Page_table_descriptor::Type::set(v, 2);
		return v;
	}

	static bool writeable(access_t const desc) {
		return Ap::get(desc) == 1 || Ap::get(desc) == 3; }

	static addr_t address(access_t const desc) {
		return (type(desc) == Page_table_entry::TABLE) ? Table_pa::masked(desc)
		                                               : Pa::masked(desc); }
};


struct Hw::Page_table
: Hw::Page_table_node<Hw::Page_table_level_2, Hw::Page_table_descriptor_level1,
                      SIZE_LOG2_1MB, SIZE_LOG2_4GB>
{
	using Base = Page_table_node<Page_table_level_2,
	                             Page_table_descriptor_level1,
	                             SIZE_LOG2_1MB, SIZE_LOG2_4GB>;
	using Array = Hw::Page_table_array<sizeof(Page_table_level_2),
	                                   _table_count(_core_vm_size(),
	                                                SIZE_LOG2_1MB)>;

	static constexpr size_t ALIGNM_LOG2 = SIZE_LOG2_16KB;

	static inline void table_changed(addr_t, size_t);

	using Base::Base;

	/**
	 * On ARM we do not need to copy top-level kernel entries
	 * because the virtual-memory kernel part is hold in a separate table
	 */
	explicit Page_table(Page_table&) : Page_table() { }

	template <typename ALLOCATOR>
	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &flags, ALLOCATOR &alloc)
	{
		return Base::insert(vo, pa, size, flags, alloc,
		                    [&] (addr_t addr, size_t size) {
			table_changed(addr, size); });
	}

	template <typename ALLOCATOR>
	void remove(addr_t vo, size_t size, ALLOCATOR &alloc)
	{
		Base::remove(vo, size, alloc, [&] (addr_t addr, size_t size) {
			table_changed(addr, size); });
	}
} __attribute__((aligned(1 << Page_table::ALIGNM_LOG2)));

#endif /* _SRC__LIB__HW__SPEC__ARM__PAGE_TABLE_H_ */
