/*
 * \brief  x86_64 DMAR page table definitions
 * \author Johannes Schlatow
 * \date   2023-11-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__PC__INTEL__PAGE_TABLE_H_
#define _SRC__DRIVERS__PLATFORM__PC__INTEL__PAGE_TABLE_H_

#include <base/env.h>
#include <base/node.h>
#include <cpu/page_table.h>
#include <util/register.h>

#include <intel/report_helper.h>

namespace Intel {

	/**
	 * Common descriptor.
	 *
	 * Table entry containing descriptor fields common to all levels.
	 */
	struct Common_descriptor : Genode::Register<64>
	{
		struct R   : Bitfield<0, 1> { };   /* read            */
		struct W   : Bitfield<1, 1> { };   /* write           */
		struct A   : Bitfield<8, 1> { };   /* accessed        */
		struct D   : Bitfield<9, 1> { };   /* dirty           */

		static bool present(access_t const v) { return R::get(v) || W::get(v); }

		static access_t create(Page_flags const &flags) {
			return R::bits(1) | W::bits(flags.writeable); }

		/**
		 * Return descriptor value with cleared accessed and dirty flags. These
		 * flags can be set by the MMU.
		 */
		static access_t clear_mmu_flags(access_t const value)
		{
			access_t ret = value;
			A::clear(ret);
			D::clear(ret);
			return ret;
		}

		static bool conflicts(access_t const old, access_t const desc) {
			return (present(old) && clear_mmu_flags(old) != desc); }

		static bool writeable(access_t const desc) {
			return present(desc) && W::get(desc); }
	};

	struct Level_1_descriptor;


	/**
	 * Intermediate level descriptor
	 *
	 * Base descriptor for page directories (intermediate level)
	 */
	template <unsigned _PAGE_SIZE_LOG2>
	struct Page_directory_descriptor : Common_descriptor
	{
		static constexpr size_t PAGE_SIZE_LOG2 = _PAGE_SIZE_LOG2;

		using Base = Common_descriptor;

		struct Ps : Base::template Bitfield<7, 1> { };  /* page size */
		struct Table_address : Base::template Bitfield<12, 36> { };
		struct Pa : Base::template Bitfield<PAGE_SIZE_LOG2,
		                                    48 - PAGE_SIZE_LOG2> { };

		static Page_table_entry type(access_t const desc)
		{
			if (!present(desc)) return Page_table_entry::INVALID;
			return (Ps::get(desc)) ? Page_table_entry::BLOCK
			                       : Page_table_entry::TABLE;
		}

		template <typename ENTRY>
		static void generate(unsigned long        index,
		                     access_t             entry,
		                     Genode::Generator   &g,
		                     Report_helper const &report_helper)
		{
			using Genode::Hex;
			using Hex_str = Genode::String<20>;

			if (type(entry) == Page_table_entry::TABLE) {
				g.node("page_directory", [&] () {
					addr_t pd_addr = Table_address::masked(entry);
					g.attribute("index",   Hex_str(Hex(index << PAGE_SIZE_LOG2)));
					g.attribute("value",   Hex_str(Hex(entry)));
					g.attribute("address", Hex_str(Hex(pd_addr)));

					report_helper.with_table<ENTRY>(pd_addr, [&] (ENTRY &pd) {
						pd.generate(g, report_helper); });
				});
			}
		}

		static void generate_page(unsigned long      index,
		                          access_t           entry,
		                          Genode::Generator &g)
		{
			using Genode::Hex;
			using Hex_str = Genode::String<20>;

			g.node("page", [&] () {
				addr_t addr = Pa::masked(entry);
				g.attribute("index",   Hex_str(Hex(index << PAGE_SIZE_LOG2)));
				g.attribute("value",   Hex_str(Hex(entry)));
				g.attribute("address", Hex_str(Hex(addr)));
				g.attribute("accessed",(bool)A::get(entry));
				g.attribute("dirty",   (bool)D::get(entry));
				g.attribute("write",   (bool)W::get(entry));
				g.attribute("read",    (bool)R::get(entry));
			});
		}

		static typename Base::access_t create(addr_t const pa)
		{
			static Page_flags flags { RW, NO_EXEC, USER, NO_GLOBAL,
			                          RAM, Genode::UNCACHED };
			return Base::create(flags) | Table_address::masked(pa);
		}

		static typename Base::access_t create(Page_flags const &flags,
		                                      addr_t const pa)
		{
			/* Ipat and Emt are ignored in legacy mode */

			return Base::create(flags)
			     | Ps::bits(1)
			     | Pa::masked(pa);
		}

		static addr_t address(typename Base::access_t desc)
		{
			switch (type(desc)) {
			case Page_table_entry::INVALID: break;
			case Page_table_entry::TABLE: return Table_address::masked(desc);
			case Page_table_entry::BLOCK: return Pa::masked(desc);
			};
			return 0;
		}
	};

	struct Level_4_descriptor;
}


struct Intel::Level_1_descriptor : Common_descriptor
{
	using Common = Common_descriptor;

	static constexpr size_t PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;

	struct Pa  : Bitfield<12, 36> { };        /* physical address     */

	static access_t create(Page_flags const &flags, addr_t const pa)
	{
		/* Ipat and Emt are ignored in legacy mode */

		return Common::create(flags)
			| Pa::masked(pa);
	}

	static void generate_page(unsigned long      index,
	                          access_t           entry,
	                          Genode::Generator &g)
	{
		using Genode::Hex;
		using Hex_str = Genode::String<20>;

		g.node("page", [&] () {
			addr_t addr = Pa::masked(entry);
			g.attribute("index",   Hex_str(Hex(index << PAGE_SIZE_LOG2)));
			g.attribute("value",   Hex_str(Hex(entry)));
			g.attribute("address", Hex_str(Hex(addr)));
			g.attribute("accessed",(bool)A::get(entry));
			g.attribute("dirty",   (bool)D::get(entry));
			g.attribute("write",   (bool)W::get(entry));
			g.attribute("read",    (bool)R::get(entry));
		});
	}
};


struct Intel::Level_4_descriptor : Common_descriptor
{
	static constexpr size_t PAGE_SIZE_LOG2 = SIZE_LOG2_512GB;
	static constexpr size_t SIZE_LOG2      = SIZE_LOG2_256TB;

	struct Pa  : Bitfield<12, SIZE_LOG2> { };    /* physical address */

	static Page_table_entry type(access_t const desc)
	{
		return (!present(desc)) ? Page_table_entry::INVALID
		                        : Page_table_entry::TABLE;
	}

	static access_t create(Page_flags flags, addr_t const pa)
	{
		return Common_descriptor::create(flags) | Pa::masked(pa);
	}

	static access_t create(addr_t const pa)
	{
		static Page_flags flags { RW, NO_EXEC, USER, NO_GLOBAL,
		                          RAM, Genode::UNCACHED };
		return Common_descriptor::create(flags) | Pa::masked(pa);
	}

	template <typename ENTRY>
	static void generate(unsigned long        index,
	                     access_t             entry,
	                     Genode::Generator   &g,
	                     Report_helper const &report_helper)
	{
		using Genode::Hex;
		using Hex_str = Genode::String<20>;

		g.node("level4_entry", [&] () {
			addr_t level3_addr = Pa::masked(entry);
			g.attribute("index",   Hex_str(Hex(index << PAGE_SIZE_LOG2)));
			g.attribute("value",   Hex_str(Hex(entry)));
			g.attribute("address", Hex_str(Hex(level3_addr)));

			report_helper.with_table<ENTRY>(level3_addr, [&] (ENTRY &level3_table) {
				level3_table.generate(g, report_helper); });
		});
	}

	static addr_t address(typename Common_descriptor::access_t desc) {
		return Pa::masked(desc); }
};


namespace Intel {

	enum {
		L1_TABLE_SIZE_LOG2 = SIZE_LOG2_2MB,
		L2_TABLE_SIZE_LOG2 = SIZE_LOG2_1GB,
		L3_TABLE_SIZE_LOG2 = SIZE_LOG2_512GB,
		L4_TABLE_SIZE_LOG2 = SIZE_LOG2_256TB,
	};

	struct Level_1_translation_table
	:
		Page_table_leaf<Level_1_descriptor, SIZE_LOG2_4KB, L1_TABLE_SIZE_LOG2>
	{
		void generate(Genode::Generator &, Report_helper const &) const;
	};

	struct Level_2_translation_table
	:
		Page_table_node<Level_1_translation_table,
		                Page_directory_descriptor<L1_TABLE_SIZE_LOG2>,
		                L1_TABLE_SIZE_LOG2, L2_TABLE_SIZE_LOG2>
	{
		void generate(Genode::Generator &, Report_helper const &) const;
	};

	struct Level_3_translation_table
	:
		Page_table_node<Level_2_translation_table,
		                Page_directory_descriptor<L2_TABLE_SIZE_LOG2>,
		                L2_TABLE_SIZE_LOG2, L3_TABLE_SIZE_LOG2>
	{
		void generate(Genode::Generator &, Report_helper const &) const;
	};

	struct Level_4_translation_table
	:
		Page_table_node<Level_3_translation_table, Level_4_descriptor,
		                L3_TABLE_SIZE_LOG2, L4_TABLE_SIZE_LOG2>
	{
		void generate(Genode::Generator &, Report_helper const &) const;
	};

}

#endif /* _SRC__DRIVERS__PLATFORM__PC__INTEL__PAGE_TABLE_H_ */
