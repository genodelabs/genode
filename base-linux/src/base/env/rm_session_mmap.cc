/*
 * \brief  Implementation of Linux-specific local region manager
 * \author Norman Feske
 * \date   2008-10-22
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/platform_env.h>
#include <base/thread.h>
#include <linux_dataspace/client.h>
#include <linux_syscalls.h>

using namespace Genode;


static bool is_sub_rm_session(Dataspace_capability ds)
{
	if (ds.valid())
		return false;

	return Dataspace_capability::deref(ds) != 0;
}


void *
Platform_env_base::Rm_session_mmap::_map_local(Dataspace_capability ds,
                                               Genode::size_t       size,
                                               addr_t               offset,
                                               bool                 use_local_addr,
                                               addr_t               local_addr,
                                               bool                 executable)
{
	int  const  fd        = _dataspace_fd(ds);
	bool const  writable  = _dataspace_writable(ds);

	int  const  flags     = MAP_SHARED | (use_local_addr ? MAP_FIXED : 0);
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

	if (((long)addr_out < 0) && ((long)addr_out > -4095)) {
		PERR("_map_local: return value of mmap is %ld", (long)addr_out);
		throw Rm_session::Region_conflict();
	}

	return addr_out;
}


void Platform_env::Rm_session_mmap::_add_to_rmap(Region const &region)
{
	if (_rmap.add_region(region) < 0) {
		PERR("_add_to_rmap: could not add region to sub RM session");
		throw Region_conflict();
	}
}


Rm_session::Local_addr
Platform_env::Rm_session_mmap::attach(Dataspace_capability ds,
                                      size_t size, off_t offset,
                                      bool use_local_addr,
                                      Rm_session::Local_addr local_addr,
                                      bool executable)
{
	Lock::Guard lock_guard(_lock);

	/* only support attach_at for sub RM sessions */
	if (_sub_rm && !use_local_addr) {
		PERR("Rm_session_mmap::attach: attaching w/o local addr not supported\n");
		throw Out_of_metadata();
	}

	if (offset < 0) {
		PERR("Rm_session_mmap::attach: negative offset not supported\n");
		throw Region_conflict();
	}

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
			PERR("Rm_session_mmap::attach: nesting sub RM sessions is not supported");
			throw Invalid_dataspace();
		}

		/*
		 * Check for the dataspace to not exceed the boundaries of the
		 * sub RM session
		 */
		if (region_size + (addr_t)local_addr > _size) {
			PERR("Rm_session_mmap::attach: dataspace does not fit in sub RM session");
			throw Region_conflict();
		}

		_add_to_rmap(Region(local_addr, offset, ds, region_size));

		/*
		 * Case 3.1
		 *
		 * This RM session is a sub RM session. If the sub RM session is
		 * attached (_base > 0), add its attachement offset to the local base
		 * and map it.
		 */
		if (_is_attached())
			_map_local(ds, region_size, offset, true, _base + (addr_t)local_addr, executable);

		return (void *)local_addr;

	} else {

		if (is_sub_rm_session(ds)) {

			Dataspace *ds_if = Dataspace_capability::deref(ds);

			Rm_session_mmap *rm = dynamic_cast<Rm_session_mmap *>(ds_if);

			if (!rm)
				throw Invalid_dataspace();

			/*
			 * Case 2.1
			 *
			 * Detect if sub RM session is already attached
			 */
			if (rm->_base) {
				PERR("Rm_session_mmap::attach: mapping a sub RM session twice is not supported");
				throw Out_of_metadata();
			}

			_add_to_rmap(Region(local_addr, offset, ds, region_size));

			/*
			 * Allocate local address range that can hold the entire sub RM
			 * session.
			 */
			rm->_base = lx_vm_reserve(use_local_addr ? (addr_t)local_addr : 0,
			                          region_size);

			/*
			 * Cases 2.2, 3.2
			 *
			 * The sub rm session was not attached until now but it may have
			 * been populated with dataspaces. Go through all regions an map
			 * each of them.
			 */
			for (int i = 0; i < Region_map::MAX_REGIONS; i++) {
				Region region = rm->_rmap.region(i);
				if (!region.used())
					continue;

				_map_local(region.dataspace(), region.size(), region.offset(),
				           true, rm->_base + region.start() + region.offset(),
				           executable);
			}

			return rm->_base;

		} else {

			/*
			 * Case 1
			 *
			 * Boring, a plain dataspace is attached to a root RM session.
			 */
			void *addr = _map_local(ds, region_size, offset, use_local_addr,
			                        local_addr, executable);

			_add_to_rmap(Region((addr_t)addr, offset, ds, region_size));

			return addr;
		}
	}
}


void Platform_env::Rm_session_mmap::detach(Rm_session::Local_addr local_addr)
{
	Lock::Guard lock_guard(_lock);

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
		if (_is_attached())
			lx_vm_reserve((addr_t)local_addr + _base, region.size());

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

		Dataspace *ds_if = Dataspace_capability::deref(region.dataspace());
		Rm_session_mmap *rm = dynamic_cast<Rm_session_mmap *>(ds_if);
		if (rm)
			rm->_base = 0;
	}

}
