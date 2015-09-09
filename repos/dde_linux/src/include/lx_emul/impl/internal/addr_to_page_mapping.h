/*
 * \brief  Address-to-page mapping helper
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__ADDR_TO_PAGE_MAPPING_H_
#define _LX_EMUL__IMPL__INTERNAL__ADDR_TO_PAGE_MAPPING_H_

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/malloc.h>
#include <lx_emul/impl/internal/list.h>

namespace Lx { class Addr_to_page_mapping; }

class Lx::Addr_to_page_mapping : public Lx::List<Addr_to_page_mapping>::Element
{
	private:

		unsigned long _addr { 0 };
		struct page   *_page { 0 };

		static Genode::List<Addr_to_page_mapping> *_list()
		{
			static Genode::List<Addr_to_page_mapping> _l;
			return &_l;
		}

	public:

		Addr_to_page_mapping(unsigned long addr, struct page *page)
		: _addr(addr), _page(page) { }

		static void insert(struct page *page)
		{
			Addr_to_page_mapping *m = (Addr_to_page_mapping*)
				Lx::Malloc::mem().alloc(sizeof (Addr_to_page_mapping));

			m->_addr = (unsigned long)page->addr;
			m->_page = page;

			_list()->insert(m);
		}

		static void remove(struct page *page)
		{
			Addr_to_page_mapping *mp = 0;
			for (Addr_to_page_mapping *m = _list()->first(); m; m = m->next())
				if (m->_page == page)
					mp = m;

			if (mp) {
				_list()->remove(mp);
				Lx::Malloc::mem().free(mp);
			}
		}

		static struct page* find_page(unsigned long addr)
		{
			for (Addr_to_page_mapping *m = _list()->first(); m; m = m->next())
				if (m->_addr == addr)
					return m->_page;

			return 0;
		}
};


#endif /* _LX_EMUL__IMPL__INTERNAL__ADDR_TO_PAGE_MAPPING_H_ */
