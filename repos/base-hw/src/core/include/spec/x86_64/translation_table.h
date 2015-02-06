/*
 * \brief   x86_64 translation table definitions for core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TRANSLATION_TABLE_H_
#define _TRANSLATION_TABLE_H_

#include <page_flags.h>
#include <assert.h>
#include <page_slab.h>

namespace Genode
{
	/**
	 * First level translation table
	 */
	class Translation_table;
}


class Genode::Translation_table
{
	public:

		enum {
			ALIGNM_LOG2               = 12,
			MIN_PAGE_SIZE_LOG2        = 12,
			MAX_COSTS_PER_TRANSLATION = 4*4096
		};

		void * operator new (size_t, void * p) { return p; }

		/**
		 * Constructor
		 */
		Translation_table() { }

		/**
		 * Maximum virtual offset that can be translated by this table
		 */
		static addr_t max_virt_offset()
		{
			PDBG("not implemented");
			return 0;
		}

		/**
		 * Insert translations into this table
		 *
		 * \param vo    offset of virt. transl. region in virt. table region
		 * \param pa    base of physical backing store
		 * \param size  size of translated region
		 * \param f     mapping flags
		 * \param s     second level page slab allocator
		 */
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Page_flags const & f, Page_slab * const s)
		{
			PDBG("not implemented");
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param slab  second level page slab allocator
		 */
		void remove_translation(addr_t vo, size_t size, Page_slab * slab)
		{
			PDBG("not implemented");
		}
};

#endif /* _TRANSLATION_TABLE_H_ */
