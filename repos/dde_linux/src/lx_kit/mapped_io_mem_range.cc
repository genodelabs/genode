/*
 * \brief  Representation of a locally-mapped MMIO range
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/attached_dataspace.h>
#include <io_mem_session/io_mem_session.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

/* Linux emulation environment */
#include <lx_emul.h>

/* Linux emulation environment includes */
#include <lx_kit/internal/list.h>
#include <lx_kit/mapped_io_mem_range.h>
#include <lx_kit/pci_dev_registry.h>
#include <lx_kit/types.h>

namespace Lx_kit { class Mapped_io_mem_range; }


/**
 * Representation of a locally-mapped MMIO range
 *
 * This class is supposed to be a private utility to support 'ioremap'.
 */
class Lx_kit::Mapped_io_mem_range : public Lx_kit::List<Mapped_io_mem_range>::Element,
                                    public Genode::Rm_connection
{
	private:

		Genode::size_t const        _size;
		Genode::addr_t const        _phys;
		Genode::Region_map_client   _region_map;
		Genode::Attached_dataspace  _ds;
		Genode::addr_t const        _virt;

	public:

		Mapped_io_mem_range(Genode::addr_t phys, Genode::size_t size,
		                    Genode::Io_mem_dataspace_capability ds_cap,
		                    Genode::addr_t offset)
		: _size(size),
		  _phys(phys),
		  _region_map(Rm_connection::create(size)),
		  _ds(_region_map.dataspace()),
		  _virt((Genode::addr_t)_ds.local_addr<void>() | (phys &0xfffUL)) {
			_region_map.attach_at(ds_cap, 0, size, offset); }

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


static Lx_kit::List<Lx_kit::Mapped_io_mem_range> ranges;


/********************************************
 ** Lx_kit::Mapped_io_mem_range implementation **
 ********************************************/

void *Lx::ioremap(addr_t phys_addr, unsigned long size,
                  Genode::Cache_attribute cache_attribute)
{
	using namespace Genode;

	/* search for the requested region within the already mapped ranges */
	for (Lx_kit::Mapped_io_mem_range *r = ranges.first(); r; r = r->next()) {

		if (r->phys_range(phys_addr, size)) {
			void * const virt = (void *)(r->virt() + phys_addr - r->phys());
			log("ioremap: return sub range phys ", Hex(phys_addr), " "
			    "(size ", size, ") to virt ", virt);
			return virt;
		}
	}

	addr_t offset = 0;
	Io_mem_dataspace_capability ds_cap =
		Lx::pci_dev_registry()->io_mem(phys_addr, cache_attribute,
		                               size, offset);

	if (!ds_cap.valid()) {
		error("failed to request I/O memory: ",
		      Hex_range<addr_t>(phys_addr, size));
		return nullptr;
	}

	Lx_kit::Mapped_io_mem_range *io_mem =
		new (env()->heap()) Lx_kit::Mapped_io_mem_range(phys_addr, size,
		                                            ds_cap, offset);

	ranges.insert(io_mem);

	log("ioremap: mapped phys ", Hex(phys_addr), " (size ", size, ") "
	    "to virt ", Hex(io_mem->virt()));

	return (void *)io_mem->virt();
}

void Lx::iounmap(volatile void * virt)
{
	using namespace Genode;

	/* search for the requested region within the already mapped ranges */
	for (Lx_kit::Mapped_io_mem_range *r = ranges.first(); r; r = r->next()) {

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
	for (Lx_kit::Mapped_io_mem_range *r = ranges.first(); r; r = r->next())
		if (r->virt_range(virt_addr, size))
			return r->cap();

	return Genode::Dataspace_capability();
}
