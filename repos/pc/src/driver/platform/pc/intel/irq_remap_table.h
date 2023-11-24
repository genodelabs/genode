/*
 * \brief  Intel IOMMU Interrupt Remapping Table implementation
 * \author Johannes Schlatow
 * \date   2023-11-09
 *
 * The interrupt remapping table is a page-aligned table structure of up to 64K
 * 128bit entries (see section 9.9 [1]). Each entries maps a virtual interrupt
 * index to a destination ID and vector.
 *
 * [1] "Intel® Virtualization Technology for Directed I/O"
 *     Revision 4.1, March 2023
 */

/*
 * Copyright (C) 2023-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__IRQ_REMAP_TABLE_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__IRQ_REMAP_TABLE_H_

/* Genode includes */
#include <util/register.h>
#include <util/bit_allocator.h>
#include <irq_session/irq_session.h>
#include <util/xml_generator.h>
#include <pci/types.h>

/* platform-driver includes */
#include <io_mmu.h>

/* local includes */
#include <cpu/clflush.h>

namespace Intel {
	using namespace Genode;

	class Irq_remap;

	template <unsigned SIZE_LOG2>
	class Irq_remap_table;
}

struct Intel::Irq_remap
{
	struct Hi : Genode::Register<64>
	{
		struct Source_id   : Bitfield<0, 16> { };

		/* Source-id qualifier */
		struct Sq          : Bitfield<16, 2> {
			enum {
				ALL_BITS          = 0,
				IGNORE_BITS_2     = 1,
				IGNORE_BITS_2_1   = 2,
				IGNORE_BITS_2_1_0 = 3
			};
		};

		/* Source validation type */
		struct Svt         : Bitfield<18, 2> {
			enum {
				DISABLE     = 0,
				SOURCE_ID   = 1,
				BUS_ID_ONLY = 2
			};
		};
	};

	struct Lo : Genode::Register<64>
	{
		struct Present          : Bitfield< 0, 1> { };
		struct Ignore_faults    : Bitfield< 1, 1> { };
		struct Destination_mode : Bitfield< 2, 1> { };
		struct Redirection_hint : Bitfield< 3, 1> { };
		struct Trigger_mode     : Bitfield< 4, 1> { };
		struct Delivery_mode    : Bitfield< 5, 3> { };
		struct Vector           : Bitfield<16, 8> { };
		struct Destination_id   : Bitfield<40, 8> { };
	};

	struct Irq_address : Register<64>
	{
		struct Destination_mode : Bitfield<2,1> { };
		struct Redirection_hint : Bitfield<3,1> { };

		struct Format : Bitfield<4,1> {
			enum {
				COMPATIBILITY = 0,
				REMAPPABLE    = 1
			};
		};

		struct Destination_id  : Bitfield<12,8> { };
		struct Handle          : Bitfield<5,15> { };
	};

	struct Irq_data : Register<64>
	{
		struct Vector        : Bitfield< 0,8> { };
		struct Delivery_mode : Bitfield< 8,3> { };
		struct Trigger_mode  : Bitfield<15,1> { };
	};

	static Hi::access_t hi_val(Pci::Bdf const &);
	static Lo::access_t lo_val(Irq_session::Info const &,
	                           Driver::Irq_controller::Irq_config const &);
};


template <unsigned SIZE_LOG2>
class Intel::Irq_remap_table
{
	public:

		static constexpr size_t ENTRIES_LOG2 = SIZE_LOG2 - 4;
		static constexpr size_t ENTRIES      = 1 << ENTRIES_LOG2;

	private:

		Irq_remap::Lo::access_t _entries[ENTRIES*2];

		static size_t _lo_index(unsigned idx) { return 2*idx; }
		static size_t _hi_index(unsigned idx) { return 2*idx + 1; }

	public:

		using Irq_allocator = Bit_allocator<ENTRIES>;
		using Irq_info      = Driver::Io_mmu::Irq_info;
		using Irq_config    = Driver::Irq_controller::Irq_config;

		bool present(unsigned idx) {
			return Irq_remap::Lo::Present::get(_entries[_lo_index(idx)]); }

		unsigned destination_id(unsigned idx) {
			return Irq_remap::Lo::Destination_id::get(_entries[_lo_index(idx)]); }

		Pci::rid_t source_id(unsigned idx) {
			return Irq_remap::Hi::Source_id::get(_entries[_hi_index(idx)]); }

		template <typename FN>
		Irq_info map(Irq_allocator    & irq_alloc,
		             Pci::Bdf   const & bdf,
		             Irq_info   const & info,
		             Irq_config const & config,
		             FN && fn)
		{
			using Format = Irq_remap::Irq_address::Format;

			Irq_session::Info session_info = info.session_info;

			/* check whether info is already in remapped format */
			if (Format::get(session_info.address) == Format::REMAPPABLE)
				return info;

			try {
				unsigned idx = (unsigned)irq_alloc.alloc();

				_entries[_hi_index(idx)] = Irq_remap::hi_val(bdf);
				_entries[_lo_index(idx)] = Irq_remap::lo_val(session_info, config);

				clflush(&_entries[_lo_index(idx)]);
				clflush(&_entries[_hi_index(idx)]);

				fn(idx);

				if (session_info.type == Irq_session::Info::Type::MSI) {
					session_info.address = 0xfee00000U
					                       | Irq_remap::Irq_address::Handle::bits(idx)
					                       | Format::bits(Format::REMAPPABLE);
					session_info.value = 0;
				}

				/* XXX support multi-vectors MSI (see 5.1.5.2) */

				/* return remapped Irq_info */
				return { Irq_info::REMAPPED, session_info, idx };

			} catch (typename Irq_allocator::Out_of_indices) {
				error("IRQ remapping table is full"); }

			return info;
		}

		bool unmap(Irq_allocator & irq_alloc, Pci::Bdf const & bdf, unsigned idx)
		{
			Pci::rid_t rid = Pci::Bdf::rid(bdf);

			if (present(idx) && source_id(idx) == rid) {
				_entries[_lo_index(idx)] = 0;
				clflush(&_entries[_lo_index(idx)]);
				irq_alloc.free(idx);

				return true;
			}

			return false;
		}

		void generate(Xml_generator & xml)
		{
			auto attribute_hex = [&] (Xml_generator & xml,
			                          char const * name,
			                          unsigned long long value)
			{
				xml.attribute(name, Genode::String<32>(Genode::Hex(value)));
			};

			for (unsigned idx = 0; idx < ENTRIES; idx++) {
				if (!present(idx))
					continue;

				xml.node("irt_entry", [&] () {
					attribute_hex(xml, "index", idx);
					attribute_hex(xml, "source_id", source_id(idx));
					attribute_hex(xml, "hi", _entries[_hi_index(idx)]);
					attribute_hex(xml, "lo", _entries[_lo_index(idx)]);
				});
			}
		}

		void flush_all() {
			for (unsigned i=0; i < ENTRIES*2; i+=8)
				clflush(&_entries[i]);
		}

		Irq_remap_table()
		{
			for (unsigned i=0; i < ENTRIES; i++) {
				_entries[_lo_index(i)] = 0;
				_entries[_hi_index(i)] = 0;
			}

			flush_all();
		}

} __attribute__((aligned(4096)));


#endif /* _SRC__DRIVERS__PLATFORM__INTEL__IRQ_REMAP_TABLE_H_ */
