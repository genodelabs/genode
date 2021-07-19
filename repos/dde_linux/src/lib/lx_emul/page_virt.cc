/*
 * \brief  Lx_emul backend for page-struct management
 * \author Norman Feske
 * \date   2021-07-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <util/avl_tree.h>
#include <lx_kit/env.h>
#include <lx_kit/map.h>
#include <lx_kit/byte_range.h>
#include <lx_emul/page_virt.h>

namespace Lx_emul { class Page_info; }

using addr_t = Genode::addr_t;
using size_t = Genode::size_t;


struct Lx_emul::Page_info
{
	struct Key { addr_t virt; } key;

	struct page * page_ptr;

	bool higher(Key const other_key) const
	{
		return key.virt > other_key.virt;
	}

	struct Query_virt_range
	{
		addr_t virt;
		size_t size;

		bool matches(Page_info const &page_info) const
		{
			size_t const page_size = 4096;

			Lx_kit::Byte_range page_range { page_info.key.virt, page_size };
			Lx_kit::Byte_range virt_range { virt, size };

			return page_range.intersects(virt_range);
		}

		Key key() const { return Key { virt }; }
	};

	struct Query_virt_addr : Query_virt_range
	{
		Query_virt_addr(void const *virt) : Query_virt_range{(addr_t)virt, 1} { }
	};
};


static Lx_kit::Map<Lx_emul::Page_info> &page_registry()
{
	static Lx_kit::Map<Lx_emul::Page_info> map { Lx_kit::env().heap };
	return map;
}


extern "C" void lx_emul_associate_page_with_virt_addr(struct page *page, void const *virt)
{
	page_registry().insert(Lx_emul::Page_info::Key { (addr_t)virt }, page);
}


void lx_emul_disassociate_page_from_virt_addr(void const *virt)
{
	page_registry().remove(Lx_emul::Page_info::Query_virt_addr(virt));
}


struct page *lx_emul_associated_page(void const *virt, unsigned long size)
{
	Lx_emul::Page_info::Query_virt_range query { .virt = (addr_t)virt, .size = size };

	struct page *page_ptr = nullptr;
	page_registry().apply(query, [&] (Lx_emul::Page_info const &page_info) {
		page_ptr = page_info.page_ptr; });

	return page_ptr;
}
