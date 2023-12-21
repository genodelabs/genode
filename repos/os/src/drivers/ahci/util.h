/*
 * \brief  Utilities used by the AHCI driver
 * \author Josef Soentgen
 * \date   2018-03-05
 */

/*
 * Copyright (C) 2018-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AHCI_UTIL_H_
#define _AHCI_UTIL_H_

/* Genode includes */
#include <base/fixed_stdint.h>

namespace Util {

	using namespace Genode;
	/*
	 * Wrap array into convinient interface
	 *
	 * The used datatype T must implement the following methods:
	 *
	 *    bool valid() const   returns true if the object is valid
	 *    void invalidate()    adjusts the object so that valid() returns false
	 */
	template <typename T, size_t CAP>
	struct Slots
	{
		T _entries[CAP] { };
		size_t _limit { CAP };

		/**
		 * Get free slot
		 */
		T *get()
		{
			for (size_t i = 0; i < _limit; i++) {
				if (!_entries[i].valid()) { return &_entries[i]; }
			}
			return nullptr;
		}

		/**
		 * Iterate over all slots until FUNC returns true
		 */
		template <typename FUNC>
		bool for_each(FUNC const &func)
		{
			for (size_t i = 0; i < _limit; i++) {
				if (!_entries[i].valid()) { continue; }
				if ( func(_entries[i])) { return true; }
			}
			return false;
		}

		size_t index(T const &entry) const
		{
			size_t index = &entry - _entries;
			return index;
		}

		void limit(size_t limit) { _limit = limit; }
	};
}

#endif /* _AHCI_UTIL_H_ */
