/*
 * \brief  Implementation of Linux-specific local region map
 * \author Norman Feske
 * \date   2008-10-22
 *
 * Under Linux, region management happens at the mercy of the Linux kernel. So,
 * all we can do in user land is 1) keep track of regions and (managed)
 * dataspaces and 2) get the kernel to manage VM regions as we intent.
 *
 * The kernel sets up mappings for the binary on execve(), which are text and
 * data segments, the stack area and special regions (stack, vdso, vsyscall).
 * Later mappings are done by the Genode program itself, which knows nothing
 * about these initial mappings. Therefore, most mmap() operations are _soft_
 * to detect region conflicts with existing mappings or let the kernel find
 * some empty VM area (as core does on other platforms). The only _hard_
 * overmaps happen on attachment and population of managed dataspaces. Mapped,
 * but not populated dataspaces are "holes" in the Linux VM space represented
 * by PROT_NONE mappings (see _reserve_local()).
 *
 * The stack area is a managed dataspace as on other platforms, which is
 * created and attached during program launch. The managed dataspace replaces
 * the inital reserved area, which is therefore flushed beforehand. Hybrid
 * programs have no stack area.
 *
 * Note, we do not support nesting of managed dataspaces.
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Linux includes */
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

/* Genode includes */
#include <base/thread.h>
#include <linux_dataspace/client.h>
#include <linux_syscalls.h>

/* base-internal includes */
#include <base/internal/local_capability.h>
#include <base/internal/region_map_mmap.h>
#include <base/internal/stack_area.h>

using namespace Genode;


static bool is_sub_rm_session(Dataspace_capability ds)
{
	if (ds.valid() && !local(ds))
		return false;

	return Local_capability<Dataspace>::deref(ds) != 0;
}


/**
 * Lock for protecting mmap/unmap sequences and region-map meta data
 */
static Lock &lock()
{
	static Lock lock;
	return lock;
}


addr_t Region_map_mmap::_reserve_local(bool           use_local_addr,
                                       addr_t         local_addr,
                                       Genode::size_t size)
{
	/* special handling for stack area */
	if (use_local_addr
	 && local_addr == stack_area_virtual_base()
	 && size       == stack_area_virtual_size()) {

		/*
		 * On the first request to reserve the stack area, we flush the
		 * initial mapping preserved in linker script and apply the actual
		 * reservation. Subsequent requests are just ignored.
		 */

		static struct Context
		{
			Context()
			{
				flush_stack_area();
				reserve_stack_area();
			}
		} inst;

		return local_addr;
	}

	int const flags       = MAP_ANONYMOUS | MAP_PRIVATE;
	int const prot        = PROT_NONE;
	void * const addr_in  = use_local_addr ? (void *)local_addr : 0;
	void * const addr_out = lx_mmap(addr_in, size, prot, flags, -1, 0);

	/* reserve at local address failed - unmap incorrect mapping */
	if (use_local_addr && addr_in != addr_out)
		lx_munmap((void *)addr_out, size);

	if ((use_local_addr && addr_in != addr_out)
	 || (((long)addr_out < 0) && ((long)addr_out > -4095))) {
		error("_reserve_local: lx_mmap failed "
		      "(addr_in=", addr_in, ",addr_out=", addr_out, "/", (long)addr_out, ")");
		throw Region_map::Region_conflict();
	}

	return (addr_t) addr_out;
}


void *Region_map_mmap::_map_local(Dataspace_capability ds,
                                  Genode::size_t       size,
                                  addr_t               offset,
                                  bool                 use_local_addr,
                                  addr_t               local_addr,
                                  bool                 executable,
                                  bool                 overmap,
                                  bool                 writeable)
{
	int  const  fd        = _dataspace_fd(ds);
	bool const  writable  = _dataspace_writable(ds) && writeable;

	int  const  flags     = MAP_SHARED | (overmap ? MAP_FIXED : 0);
	int  const  prot      = PROT_READ
	                      | (writable   ? PROT_WRITE : 0)
	                      | (executable ? PROT_EXEC  : 0);
	void * const addr_in  = use_local_addr ? (void*)local_addr : 0;
	void * const addr_out = lx_mmap(addr_in, size, prot, flags, fd, offset);

	/*
	 * We can close the file after calling mmap. The Linux kernel will still
	 * keep the file mapped. By immediately closing the file descriptor, we
	 * won't need to keep track of dataspace file descriptors within the
	 * process.
	 */
	lx_close(fd);

	/* attach at local address failed - unmap incorrect mapping */
	if (use_local_addr && addr_in != addr_out)
		lx_munmap((void *)addr_out, size);

	if ((use_local_addr && addr_in != addr_out)
	 || (((long)addr_out < 0) && ((long)addr_out > -4095))) {
		error("_map_local: lx_mmap failed"
		      "(addr_in=", addr_in, ", addr_out=", addr_out, "/", (long)addr_out, ") "
		      "overmap=", overmap);
		throw Region_map::Region_conflict();
	}

	return addr_out;
}


void Region_map_mmap::_add_to_rmap(Region const &region)
{
	if (_rmap.add_region(region) < 0) {
		error("_add_to_rmap: could not add region to sub RM session");
		throw Region_conflict();
	}
}


Region_map::Local_addr Region_map_mmap::attach(Dataspace_capability ds,
                                               size_t size, off_t offset,
                                               bool use_local_addr,
                                               Region_map::Local_addr local_addr,
                                               bool executable, bool writeable)
{
	Lock::Guard lock_guard(lock());

	/* only support attach_at for sub RM sessions */
	if (_sub_rm && !use_local_addr) {
		error("Region_map_mmap::attach: attaching w/o local addr not supported");
		throw Region_conflict();
	}

	if (offset < 0) {
		error("Region_map_mmap::attach: negative offset not supported");
		throw Region_conflict();
	}

	if (!ds.valid())
		throw Invalid_dataspace();

	size_t const remaining_ds_size = _dataspace_size(ds) > (addr_t)offset
	                               ? _dataspace_size(ds) - (addr_t)offset : 0;

	/* determine size of virtual address region */
	size_t const region_size = size ? min(remaining_ds_size, size)
	                                : remaining_ds_size;
	if (region_size == 0)
		throw Region_conflict();

	/*
	 * We have to distinguish the following cases
	 *
	 * 1 we are a root RM session and ds is a plain dataspace
	 * 2 we are a root RM session and ds is a sub RM session
	 *   2.1 ds is already attached (base != 0)
	 *   2.2 ds is not yet attached
	 * 3 we are a sub RM session and ds is a plain dataspace
	 *   3.1 we are attached to a root RM session
	 *   3.2 we are not yet attached
	 * 4 we are a sub RM session and ds is a sub RM session (not supported)
	 */

	if (_sub_rm) {

		/*
		 * Case 4
		 */
		if (is_sub_rm_session(ds)) {
			error("Region_map_mmap::attach: nesting sub RM sessions is not supported");
			throw Invalid_dataspace();
		}

		/*
		 * Check for the dataspace to not exceed the boundaries of the
		 * sub RM session
		 */
		if (region_size + (addr_t)local_addr > _size) {
			error("Region_map_mmap::attach: dataspace does not fit in sub RM session");
			throw Region_conflict();
		}

		_add_to_rmap(Region(local_addr, offset, ds, region_size));

		/*
		 * Case 3.1
		 *
		 * This RM session is a sub RM session. If the sub RM session is
		 * attached (_base > 0), add its attachment offset to the local base
		 * and map it. We have to enforce the mapping via the 'overmap'
		 * argument as the region was reserved by a PROT_NONE mapping.
		 */
		if (_is_attached())
			_map_local(ds, region_size, offset, true, _base + (addr_t)local_addr, executable, true, writeable);

		return (void *)local_addr;

	} else {

		if (is_sub_rm_session(ds)) {

			Dataspace *ds_if = Local_capability<Dataspace>::deref(ds);

			Region_map_mmap *rm = dynamic_cast<Region_map_mmap *>(ds_if);

			if (!rm)
				throw Invalid_dataspace();

			/*
			 * Case 2.1
			 *
			 * Detect if sub RM session is already attached
			 */
			if (rm->_base) {
				error("Region_map_mmap::attach: mapping a sub RM session twice is not supported");
				throw Region_conflict();
			}

			/*
			 * Reserve local address range that can hold the entire sub RM
			 * session.
			 */
			rm->_base = _reserve_local(use_local_addr, local_addr, region_size);

			_add_to_rmap(Region(rm->_base, offset, ds, region_size));

			/*
			 * Cases 2.2, 3.2
			 *
			 * The sub rm session was not attached until now but it may have
			 * been populated with dataspaces. Go through all regions and map
			 * each of them.
			 */
			for (int i = 0; i < Region_registry::MAX_REGIONS; i++) {
				Region region = rm->_rmap.region(i);
				if (!region.used())
					continue;

				/*
				 * We have to enforce the mapping via the 'overmap' argument as
				 * the region was reserved by a PROT_NONE mapping.
				 */
				_map_local(region.dataspace(), region.size(), region.offset(),
				           true, rm->_base + region.start() + region.offset(),
				           executable, true, writeable);
			}

			return rm->_base;

		} else {

			/*
			 * Case 1
			 *
			 * Boring, a plain dataspace is attached to a root RM session.
			 * Note, we do not overmap.
			 */
			void *addr = _map_local(ds, region_size, offset, use_local_addr,
			                        local_addr, executable, false, writeable);

			_add_to_rmap(Region((addr_t)addr, offset, ds, region_size));

			return addr;
		}
	}
}


void Region_map_mmap::detach(Region_map::Local_addr local_addr)
{
	Lock::Guard lock_guard(lock());

	/*
	 * Cases
	 *
	 * 1 we are root RM
	 * 2 we are sub RM (region must be normal dataspace)
	 *   2.1 we are not attached
	 *   2.2 we are attached to a root RM
	 */

	Region region = _rmap.lookup(local_addr);
	if (!region.used())
		return;

	/*
	 * Remove meta data from region map
	 */
	_rmap.remove_region(local_addr);

	if (_sub_rm) {

		/*
		 * Case 2.1, 2.2
		 *
		 * By removing a region from an attached sub RM session we mark the
		 * corresponding local address range as reserved. A plain 'munmap'
		 * would mark this range as free to use for the root RM session, which
		 * we need to prevent.
		 *
		 * If we are not attached, no local address-space manipulation is
		 * needed.
		 */
		if (_is_attached()) {
			lx_munmap((void *)((addr_t)local_addr + _base), region.size());
			_reserve_local(true, (addr_t)local_addr + _base, region.size());
		}

	} else {

		/*
		 * Case 1
		 *
		 * We need no distiction between detaching normal dataspaces and
		 * sub RM session. In both cases, we simply mark the local address
		 * range as free.
		 */
		lx_munmap(local_addr, region.size());
	}

	/*
	 * If the detached dataspace is sub RM session, mark it as detached
	 */
	if (is_sub_rm_session(region.dataspace())) {

		Dataspace *ds_if = Local_capability<Dataspace>::deref(region.dataspace());
		Region_map_mmap *rm = dynamic_cast<Region_map_mmap *>(ds_if);
		if (rm)
			rm->_base = 0;
	}
}
