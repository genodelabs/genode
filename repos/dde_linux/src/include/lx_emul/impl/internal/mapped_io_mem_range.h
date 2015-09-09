/*
 * \brief  Representation of a locally-mapped MMIO range
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__MAPPED_IO_MEM_RANGE_H_
#define _LX_EMUL__IMPL__INTERNAL__MAPPED_IO_MEM_RANGE_H_

/* Genode includes */
#include <os/attached_dataspace.h>
#include <io_mem_session/io_mem_session.h>

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/list.h>

namespace Lx {

	class Mapped_io_mem_range;

	void *ioremap(resource_size_t, unsigned long, Genode::Cache_attribute);
}

/**
 * Representation of a locally-mapped MMIO range
 *
 * This class is supposed to be a private utility to support 'ioremap'.
 */
class Lx::Mapped_io_mem_range : public Lx::List<Mapped_io_mem_range>::Element
{
	private:

		Genode::Attached_dataspace _ds;

		Genode::size_t const _size;
		Genode::addr_t const _phys;
		Genode::addr_t const _virt;

	public:

		Mapped_io_mem_range(Genode::addr_t phys, Genode::size_t size,
		                    Genode::Io_mem_dataspace_capability ds_cap)
		:
			_ds(ds_cap),
			_size(size),
			_phys(phys),
			_virt((Genode::addr_t)_ds.local_addr<void>() | (phys & 0xfffUL))
		{ }

		Genode::addr_t phys() const { return _phys; }
		Genode::addr_t virt() const { return _virt; }

		/**
		 * Return true if the mapped range contains the specified sub range
		 */
		bool contains(Genode::addr_t phys, Genode::size_t size) const
		{
			return (phys >= _phys) && (phys + size - 1 <= _phys + _size - 1);
		}
};


void *Lx::ioremap(resource_size_t phys_addr, unsigned long size,
                  Genode::Cache_attribute cache_attribute)
{
	using namespace Genode;

	static Lx::List<Lx::Mapped_io_mem_range> ranges;

	/* search for the requested region within the already mapped ranges */
	for (Lx::Mapped_io_mem_range *r = ranges.first(); r; r = r->next()) {

		if (r->contains(phys_addr, size)) {
			void * const virt = (void *)(r->virt() + phys_addr - r->phys());
			PLOG("ioremap: return sub range phys 0x%lx (size %lx) to virt 0x%lx",
			     (long)phys_addr, (long)size, (long)virt);
			return virt;
		}
	}

	Io_mem_dataspace_capability ds_cap =
		Lx::pci_dev_registry()->io_mem(phys_addr, cache_attribute, size);

	if (!ds_cap.valid()) {

		PERR("Failed to request I/O memory: [%zx,%lx)", phys_addr,
		     phys_addr + size);
		return nullptr;
	}

	Genode::size_t const ds_size = Genode::Dataspace_client(ds_cap).size();

	Lx::Mapped_io_mem_range *io_mem =
		new (env()->heap()) Lx::Mapped_io_mem_range(phys_addr, ds_size, ds_cap);

	ranges.insert(io_mem);

	PLOG("ioremap: mapped phys 0x%lx (size %lx) to virt 0x%lx",
	     (long)phys_addr, (long)size, (long)io_mem->virt());

	addr_t const sub_page_offset = phys_addr & 0xfff;
	return (void *)(io_mem->virt() + sub_page_offset);
}

#endif /* _LX_EMUL__IMPL__INTERNAL__MAPPED_IO_MEM_RANGE_H_ */
