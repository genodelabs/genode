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
#include <util/register.h>
#include <util/xml_generator.h>

#include <page_table/page_table_base.h>
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

		static access_t create(Page_flags const &flags)
		{
			return R::bits(1)
				| W::bits(flags.writeable);
		}

		/**
		 * Return descriptor value with cleared accessed and dirty flags. These
		 * flags can be set by the MMU.
		 */
		static access_t clear_mmu_flags(access_t value)
		{
			A::clear(value);
			D::clear(value);
			return value;
		}
	};

	/**
	 * Level 1 descriptor
	 */
	template <unsigned _PAGE_SIZE_LOG2>
	struct Level_1_descriptor;

	/**
	 * Base descriptor for page directories (intermediate level)
	 */
	struct Page_directory_base_descriptor : Common_descriptor
	{
		using Common = Common_descriptor;

		struct Ps : Common::template Bitfield<7, 1> { };  /* page size */

		static bool maps_page(access_t const v) { return Ps::get(v); }
	};

	/**
	 * Intermediate level descriptor
	 *
	 * Wraps descriptors for page tables and pages.
	 */
	template <unsigned _PAGE_SIZE_LOG2>
	struct Page_directory_descriptor : Page_directory_base_descriptor
	{
		static constexpr size_t PAGE_SIZE_LOG2 = _PAGE_SIZE_LOG2;

		struct Page;
		struct Table;
	};

	/**
	 * Level 4 descriptor
	 */
	template <unsigned _PAGE_SIZE_LOG2, unsigned _SIZE_LOG2>
	struct Level_4_descriptor;
}


template <unsigned _PAGE_SIZE_LOG2>
struct Intel::Level_1_descriptor : Common_descriptor
{
	using Common = Common_descriptor;

	static constexpr size_t PAGE_SIZE_LOG2 = _PAGE_SIZE_LOG2;

	struct Pa  : Bitfield<12, 36> { };        /* physical address     */

	static access_t create(Page_flags const &flags, addr_t const pa)
	{
		/* Ipat and Emt are ignored in legacy mode */

		return Common::create(flags)
			| Pa::masked(pa);
	}

	static void generate_page(unsigned long           index,
	                          access_t                entry,
	                          Genode::Xml_generator & xml)
	{
		using Genode::Hex;
		using Hex_str = Genode::String<20>;

		xml.node("page", [&] () {
			addr_t addr = Pa::masked(entry);
			xml.attribute("index",   Hex_str(Hex(index << PAGE_SIZE_LOG2)));
			xml.attribute("value",   Hex_str(Hex(entry)));
			xml.attribute("address", Hex_str(Hex(addr)));
			xml.attribute("accessed",(bool)A::get(entry));
			xml.attribute("dirty",   (bool)D::get(entry));
			xml.attribute("write",   (bool)W::get(entry));
			xml.attribute("read",    (bool)R::get(entry));
		});
	}
};


template <unsigned _PAGE_SIZE_LOG2>
struct Intel::Page_directory_descriptor<_PAGE_SIZE_LOG2>::Table
	: Page_directory_base_descriptor
{
	using Base = Page_directory_base_descriptor;

	/**
	 * Physical address
	 */
	struct Pa : Base::template Bitfield<12, 36> { };

	static typename Base::access_t create(addr_t const pa)
	{
		static Page_flags flags { RW, NO_EXEC, USER, NO_GLOBAL,
		                          RAM, Genode::UNCACHED };
		return Base::create(flags) | Pa::masked(pa);
	}

	template <typename ENTRY>
	static void generate(unsigned long           index,
	                     access_t                entry,
	                     Genode::Xml_generator & xml,
	                     Genode::Env           & env,
	                     Report_helper         & report_helper)
	{
		using Genode::Hex;
		using Hex_str = Genode::String<20>;

		xml.node("page_directory", [&] () {
			addr_t pd_addr = Pa::masked(entry);
			xml.attribute("index",   Hex_str(Hex(index << PAGE_SIZE_LOG2)));
			xml.attribute("value",   Hex_str(Hex(entry)));
			xml.attribute("address", Hex_str(Hex(pd_addr)));

			report_helper.with_table<ENTRY>(pd_addr, [&] (ENTRY & pd) {
				pd.generate(xml, env, report_helper); });
		});
	}
};


template <unsigned _PAGE_SIZE_LOG2>
struct Intel::Page_directory_descriptor<_PAGE_SIZE_LOG2>::Page
	: Page_directory_base_descriptor
{
	using Base = Page_directory_base_descriptor;

	/**
	 * Physical address
	 */
	struct Pa : Base::template Bitfield<PAGE_SIZE_LOG2,
	                                     48 - PAGE_SIZE_LOG2> { };


	static typename Base::access_t create(Page_flags const &flags,
	                                      addr_t const pa)
	{
		/* Ipat and Emt are ignored in legacy mode */

		return Base::create(flags)
		     | Base::Ps::bits(1)
		     | Pa::masked(pa);
	}

	static void generate_page(unsigned long           index,
	                          access_t                entry,
	                          Genode::Xml_generator & xml)
	{
		using Genode::Hex;
		using Hex_str = Genode::String<20>;

		xml.node("page", [&] () {
			addr_t addr = Pa::masked(entry);
			xml.attribute("index",   Hex_str(Hex(index << PAGE_SIZE_LOG2)));
			xml.attribute("value",   Hex_str(Hex(entry)));
			xml.attribute("address", Hex_str(Hex(addr)));
			xml.attribute("accessed",(bool)A::get(entry));
			xml.attribute("dirty",   (bool)D::get(entry));
			xml.attribute("write",   (bool)W::get(entry));
			xml.attribute("read",    (bool)R::get(entry));
		});
	}
};


template <unsigned _PAGE_SIZE_LOG2, unsigned _SIZE_LOG2>
struct Intel::Level_4_descriptor : Common_descriptor
{
	static constexpr size_t PAGE_SIZE_LOG2 = _PAGE_SIZE_LOG2;
	static constexpr size_t SIZE_LOG2      = _SIZE_LOG2;

	struct Pa  : Bitfield<12, SIZE_LOG2> { };    /* physical address */

	static access_t create(addr_t const pa)
	{
		static Page_flags flags { RW, NO_EXEC, USER, NO_GLOBAL,
		                          RAM, Genode::UNCACHED };
		return Common_descriptor::create(flags) | Pa::masked(pa);
	}

	template <typename ENTRY>
	static void generate(unsigned long           index,
	                     access_t                entry,
	                     Genode::Xml_generator & xml,
	                     Genode::Env           & env,
	                     Report_helper         & report_helper)
	{
		using Genode::Hex;
		using Hex_str = Genode::String<20>;

		xml.node("level4_entry", [&] () {
			addr_t level3_addr = Pa::masked(entry);
			xml.attribute("index",   Hex_str(Hex(index << PAGE_SIZE_LOG2)));
			xml.attribute("value",   Hex_str(Hex(entry)));
			xml.attribute("address", Hex_str(Hex(level3_addr)));

			report_helper.with_table<ENTRY>(level3_addr, [&] (ENTRY & level3_table) {
				level3_table.generate(xml, env, report_helper); });
		});
	}
};


namespace Intel {

	struct Level_1_translation_table
	:
		Final_table<Level_1_descriptor<SIZE_LOG2_4KB>>
	{
		static constexpr unsigned address_width() { return SIZE_LOG2_2MB; }

		void generate(Genode::Xml_generator &, Genode::Env & env, Report_helper &);
	} __attribute__((aligned(1 << ALIGNM_LOG2)));

	struct Level_2_translation_table
	:
		Page_directory<Level_1_translation_table,
		               Page_directory_descriptor<SIZE_LOG2_2MB>>
	{
		static constexpr unsigned address_width() { return SIZE_LOG2_1GB; }

		void generate(Genode::Xml_generator &, Genode::Env & env, Report_helper &);
	} __attribute__((aligned(1 << ALIGNM_LOG2)));

	struct Level_3_translation_table
	:
		Page_directory<Level_2_translation_table,
		               Page_directory_descriptor<SIZE_LOG2_1GB>>
	{
		static constexpr unsigned address_width() { return SIZE_LOG2_512GB; }

		void generate(Genode::Xml_generator &, Genode::Env & env, Report_helper &);
	} __attribute__((aligned(1 << ALIGNM_LOG2)));

	struct Level_4_translation_table
	:
		Pml4_table<Level_3_translation_table,
		           Level_4_descriptor<SIZE_LOG2_512GB, SIZE_LOG2_256TB>>
	{
		static constexpr unsigned address_width() { return SIZE_LOG2_256TB; }

		void generate(Genode::Xml_generator &, Genode::Env & env, Report_helper &);
	} __attribute__((aligned(1 << ALIGNM_LOG2)));

}

#endif /* _SRC__DRIVERS__PLATFORM__PC__INTEL__PAGE_TABLE_H_ */
