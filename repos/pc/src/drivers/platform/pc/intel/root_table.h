/*
 * \brief  Intel IOMMU Root Table implementation
 * \author Johannes Schlatow
 * \date   2023-08-31
 *
 * The root table is a page-aligned 4KB size structure. It is indexed by the
 * bus number. In legacy mode, each entry contains a context-table pointer
 * (see 9.1 and 11.4.5).
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__ROOT_TABLE_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__ROOT_TABLE_H_

/* Genode includes */
#include <base/env.h>
#include <util/register.h>
#include <util/xml_generator.h>
#include <cpu/clflush.h>

namespace Intel {
	using namespace Genode;

	class Root_table;

	/* forward declaration */
	class Report_helper;
}


class Intel::Root_table
{
	private:

		struct Entry : Genode::Register<64>
		{
			struct Present : Bitfield< 0, 1> { };
			struct Address : Bitfield<12,52> { };
		};

		typename Entry::access_t _entries[512];

	public:

		template <typename FN>
		static void for_each(FN && fn)
		{
			uint8_t bus = 0;
			do {
				fn(bus);
				bus++;
			} while (bus != 0xFF);
		}

		bool present(uint8_t bus) {
			return Entry::Present::get(_entries[bus*2]); }

		addr_t address(uint8_t bus) {
			return Entry::Address::masked(_entries[bus*2]); }

		void address(uint8_t bus, addr_t addr, bool flush)
		{
			_entries[bus*2] = Entry::Address::masked(addr) | Entry::Present::bits(1);

			if (flush)
				clflush(&_entries[bus*2]);
		}

		void generate(Xml_generator &, Env &, Report_helper &);

		Root_table()
		{
			for (Genode::size_t i=0; i < 512; i++)
				_entries[i] = 0;
		}

} __attribute__((aligned(4096)));

#endif /* _SRC__DRIVERS__PLATFORM__INTEL__ROOT_TABLE_H_ */
