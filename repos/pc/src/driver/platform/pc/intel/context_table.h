/*
 * \brief  Intel IOMMU Context Table implementation
 * \author Johannes Schlatow
 * \date   2023-08-31
 *
 * The context table is a page-aligned 4KB size structure. It is indexed by the
 * lower 8bit of the resource id (see 9.3).
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__CONTEXT_TABLE_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__CONTEXT_TABLE_H_

/* Genode includes */
#include <base/env.h>
#include <util/register.h>
#include <util/xml_generator.h>
#include <pci/types.h>
#include <cpu/clflush.h>

namespace Intel {
	using namespace Genode;

	class Context_table;

	/* forward declaration */
	class Report_helper;
}


class Intel::Context_table
{
	private:

		struct Hi : Genode::Register<64>
		{
			/* set according to SAGAW of Capability register */
			struct Address_width  : Bitfield< 0, 3>
			{
				enum {
					AGAW_3_LEVEL = 0x1,
					AGAW_4_LEVEL = 0x2,
					AGAW_5_LEVEL = 0x3
				};
			};
			struct Domain         : Bitfield< 8,16> { };
		};

		struct Lo : Genode::Register<64>
		{
			struct Present        : Bitfield< 0, 1> { };
			struct Ignore_faults  : Bitfield< 1, 1> { };

			/* should be 0 */
			struct Type           : Bitfield< 2, 2> { };
			struct Stage2_pointer : Bitfield<12,52> { };
		};

		typename Lo::access_t _entries[512];

		static size_t _lo_index(Pci::rid_t rid) { return 2*(rid & 0xff); }
		static size_t _hi_index(Pci::rid_t rid) { return 2*(rid & 0xff) + 1; }

	public:

		template <typename FN>
		static void for_each(Pci::rid_t start, FN && fn)
		{
			Pci::rid_t rid { start };
			do {
				fn(rid);
				rid++;
			} while (rid != start + 0xFF);
		}

		Lo::access_t lo(Pci::rid_t rid) {
			return _entries[_lo_index(rid)]; }

		Hi::access_t hi(Pci::rid_t rid) {
			return _entries[_hi_index(rid)]; }

		bool present(Pci::rid_t rid) {
			return Lo::Present::get(lo(rid)); }

		uint16_t domain(Pci::rid_t rid) {
			return Hi::Domain::get(hi(rid)); }

		uint8_t agaw(Pci::rid_t rid) {
			return Hi::Address_width::get(hi(rid)); }

		uint8_t translation_type(Pci::rid_t rid) {
			return Lo::Type::get(lo(rid)); }

		bool fault_processing_disabled(Pci::rid_t rid) {
			return Lo::Ignore_faults::get(lo(rid)); }

		addr_t stage2_pointer(Pci::rid_t rid) {
			return Lo::Stage2_pointer::masked(lo(rid)); }

		template <unsigned ADDRESS_WIDTH>
		void insert(Pci::rid_t rid, addr_t phys_addr, uint16_t domain_id,
		            bool flush)
		{
			static_assert(ADDRESS_WIDTH == 39 ||
			              ADDRESS_WIDTH == 48 ||
			              ADDRESS_WIDTH == 57, "unsupported address width");

			unsigned agaw;
			switch (ADDRESS_WIDTH) {
				case 39:
					agaw = Hi::Address_width::AGAW_3_LEVEL;
					break;
				case 48:
					agaw = Hi::Address_width::AGAW_4_LEVEL;
					break;
				case 57:
					agaw = Hi::Address_width::AGAW_5_LEVEL;
					break;
				default:
					error("unsupported address width");
					return;
			}

			Hi::access_t hi_val = Hi::Address_width::bits(agaw) |
			                      Hi::Domain::bits(domain_id);
			_entries[_hi_index(rid)] = hi_val;

			Lo::access_t lo_val = Lo::Present::bits(1) |
			                      Lo::Stage2_pointer::masked(phys_addr);
			_entries[_lo_index(rid)] = lo_val;

			if (flush)
				clflush(&_entries[_lo_index(rid)]);
		}

		void remove(Pci::rid_t rid, bool flush)
		{
			Lo::access_t val = lo(rid);
			Lo::Present::clear(val);
			_entries[_lo_index(rid)] = val;

			if (flush)
				clflush(&_entries[_lo_index(rid)]);
		}

		void generate(Xml_generator &, Env &, Intel::Report_helper &);

		void flush_all()
		{
			for (Genode::size_t i=0; i < 512; i+=8)
				clflush(&_entries[i]);
		}

		Context_table()
		{
			for (Genode::size_t i=0; i < 512; i++)
				_entries[i] = 0;
		}

} __attribute__((aligned(4096)));


#endif /* _SRC__DRIVERS__PLATFORM__INTEL__CONTEXT_TABLE_H_ */
