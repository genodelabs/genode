/*
 * \brief  Address-to-page mapping helper
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__ADDR_TO_PAGE_MAPPING_H_
#define _LX_KIT__ADDR_TO_PAGE_MAPPING_H_

/* Linux emulation environment includes */
#include <legacy/lx_kit/internal/list.h>
#include <legacy/lx_kit/malloc.h>

namespace Lx { class Addr_to_page_mapping; }

class Lx::Addr_to_page_mapping : public Lx_kit::List<Addr_to_page_mapping>::Element
{
	private:

		unsigned long                    _addr { 0 };
		struct page                     *_page { 0 };
		Genode::Ram_dataspace_capability _cap;

		static Genode::List<Addr_to_page_mapping> *_list()
		{
			static Genode::List<Addr_to_page_mapping> _l;
			return &_l;
		}

	public:

		static void insert(struct page *page,
		                   Genode::Ram_dataspace_capability cap)
		{
			Addr_to_page_mapping *m = (Addr_to_page_mapping*)
				Lx::Malloc::mem().alloc(sizeof (Addr_to_page_mapping));

			m->_addr = (unsigned long)page->addr;
			m->_page = page;
			m->_cap  = cap;

			_list()->insert(m);
		}

		static Genode::Ram_dataspace_capability remove(struct page *page)
		{
			Genode::Ram_dataspace_capability cap;
			Addr_to_page_mapping *mp = 0;
			for (Addr_to_page_mapping *m = _list()->first(); m; m = m->next())
				if (m->_page == page)
					mp = m;

			if (mp) {
				cap = mp->_cap;
				_list()->remove(mp);
				Lx::Malloc::mem().free(mp);
			}

			return cap;
		}

		static struct page* find_page(unsigned long addr)
		{
			for (Addr_to_page_mapping *m = _list()->first(); m; m = m->next())
				if (m->_addr == addr)
					return m->_page;

			return 0;
		}

		static struct page* find_page_by_paddr(unsigned long paddr);
};


#endif /* _LX_KIT__ADDR_TO_PAGE_MAPPING_H_ */
