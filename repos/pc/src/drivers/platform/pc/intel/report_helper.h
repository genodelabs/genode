/*
 * \brief  Helper for translating physical addresses into tables
 * \author Johannes Schlatow
 * \date   2023-08-31
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__REPORT_HELPER_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__REPORT_HELPER_H_

/* Genode includes */
#include <base/registry.h>

namespace Intel {
	using namespace Genode;

	class Registered_translation_table;
	class Report_helper;

	using Translation_table_registry = Registry<Registered_translation_table>;
}


class Intel::Registered_translation_table : private Translation_table_registry::Element
{
	public:

		virtual addr_t virt_addr(addr_t) = 0;

		Registered_translation_table(Translation_table_registry & registry)
		: Translation_table_registry::Element(registry, *this)
		{ }

		virtual ~Registered_translation_table() { }
};


class Intel::Report_helper
{
	private:

		Translation_table_registry & _registry;

	public:

		template <typename TABLE, typename FN>
		void with_table(addr_t phys_addr, FN && fn)
		{
			addr_t va { 0 };
			_registry.for_each([&] (Registered_translation_table & table) {
				if (!va)
					va = table.virt_addr(phys_addr);
			});

			if (va)
				fn(*reinterpret_cast<TABLE*>(va));
		}

		Report_helper(Translation_table_registry & registry)
		: _registry(registry)
		{ }

};

#endif /* _SRC__DRIVERS__PLATFORM__INTEL__REPORT_HELPER_H_ */
