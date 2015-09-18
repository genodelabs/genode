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
	void  iounmap(volatile void*);
	Genode::Dataspace_capability ioremap_lookup(Genode::addr_t, Genode::size_t);
}

/**
 * Representation of a locally-mapped MMIO range
 *
 * This class is supposed to be a private utility to support 'ioremap'.
 */
class Lx::Mapped_io_mem_range : public Lx::List<Mapped_io_mem_range>::Element
{
	private:

		Genode::size_t const        _size;
		Genode::addr_t const        _phys;
		Genode::Rm_connection       _rm;
		Genode::Attached_dataspace  _ds;
		Genode::addr_t const        _virt;

	public:

		Mapped_io_mem_range(Genode::addr_t phys, Genode::size_t size,
		                    Genode::Io_mem_dataspace_capability ds_cap,
		                    Genode::addr_t offset)
		: _size(size),
		  _phys(phys),
		  _rm(0, size),
		  _ds(_rm.dataspace()),
		  _virt((Genode::addr_t)_ds.local_addr<void>() | (phys &0xfffUL)) {
			_rm.attach_at(ds_cap, 0, size, offset); }

		Genode::addr_t phys() const { return _phys; }
		Genode::addr_t virt() const { return _virt; }
		Genode::Dataspace_capability cap() const { return _ds.cap(); }

		/**
		 * Return true if the mapped range contains the specified sub range
		 */
		bool phys_range(Genode::addr_t phys, Genode::size_t size) const
		{
			return (phys >= _phys) && (phys + size - 1 <= _phys + _size - 1);
		}

		/**
		 * Return true if the mapped range contains the specified sub range
		 */
		bool virt_range(Genode::addr_t virt, Genode::size_t size) const
		{
			return (virt >= _virt) && (virt + size - 1 <= _virt + _size - 1);
		}
};


static Lx::List<Lx::Mapped_io_mem_range> ranges;


void *Lx::ioremap(resource_size_t phys_addr, unsigned long size,
                  Genode::Cache_attribute cache_attribute)
{
	using namespace Genode;

	/* search for the requested region within the already mapped ranges */
	for (Lx::Mapped_io_mem_range *r = ranges.first(); r; r = r->next()) {

		if (r->phys_range(phys_addr, size)) {
			void * const virt = (void *)(r->virt() + phys_addr - r->phys());
			PLOG("ioremap: return sub range phys 0x%lx (size %lx) to virt 0x%lx",
			     (long)phys_addr, (long)size, (long)virt);
			return virt;
		}
	}

	addr_t offset;
	Io_mem_dataspace_capability ds_cap =
		Lx::pci_dev_registry()->io_mem(phys_addr, cache_attribute,
		                               size, offset);

	if (!ds_cap.valid()) {
		PERR("Failed to request I/O memory: [%zx,%lx)", phys_addr,
		     phys_addr + size);
		return nullptr;
	}

	Lx::Mapped_io_mem_range *io_mem =
		new (env()->heap()) Lx::Mapped_io_mem_range(phys_addr, size,
		                                            ds_cap, offset);

	ranges.insert(io_mem);

	PLOG("ioremap: mapped phys 0x%lx (size %lx) to virt 0x%lx",
	     (long)phys_addr, (long)size, (long)io_mem->virt());

	addr_t const sub_page_offset = phys_addr & 0xfff;
	return (void *)(io_mem->virt() + sub_page_offset);
}

void Lx::iounmap(volatile void * virt)
{
	using namespace Genode;

	/* search for the requested region within the already mapped ranges */
	for (Lx::Mapped_io_mem_range *r = ranges.first(); r; r = r->next()) {

		if (r->virt() == (addr_t)virt) {
			ranges.remove(r);
			destroy(env()->heap(), r);
			return;
		}
	}
}

Genode::Dataspace_capability
Lx::ioremap_lookup(Genode::addr_t virt_addr, Genode::size_t size)
{
	/* search for the requested region within the already mapped ranges */
	for (Lx::Mapped_io_mem_range *r = ranges.first(); r; r = r->next())
		if (r->virt_range(virt_addr, size))
			return r->cap();

	return Genode::Dataspace_capability();
}

#endif /* _LX_EMUL__IMPL__INTERNAL__MAPPED_IO_MEM_RANGE_H_ */
