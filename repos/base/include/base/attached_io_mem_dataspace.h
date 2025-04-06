/*
 * \brief  I/O MEM dataspace utility
 * \author Norman Feske
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ATTACHED_IO_MEM_DATASPACE_H_
#define _INCLUDE__BASE__ATTACHED_IO_MEM_DATASPACE_H_

#include <io_mem_session/connection.h>
#include <base/attached_dataspace.h>

namespace Genode { class Attached_io_mem_dataspace; }


/**
 * Request and locally attach a memory-mapped I/O resource
 *
 * This class is a wrapper for a typical sequence of operations performed
 * by device drivers to access memory-mapped device resources. Its sole
 * purpose is to avoid duplicated code.
 */
class Genode::Attached_io_mem_dataspace
{
	private:

		Io_mem_connection           _mmio;
		Io_mem_dataspace_capability _ds;
		Env::Local_rm::Result const _attached;
		addr_t                const _at;

		static addr_t _with_sub_page_offset(addr_t local, addr_t io_base)
		{
			return local | (io_base & 0xfffUL);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param base            base address of memory-mapped I/O resource
		 * \param size            size of resource
		 * \param write_combined  enable write combining for the resource
		 *
		 * \throw Service_denied
		 * \throw Insufficient_ram_quota
		 * \throw Insufficient_cap_quota
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 * \throw Attached_dataspace::Region_conflict
		 * \throw Attached_dataspace::Invalid_dataspace
		 */
		Attached_io_mem_dataspace(Env &env, Genode::addr_t base, Genode::size_t size,
		                          bool write_combined = false)
		:
			_mmio(env, base, size, write_combined),
			_ds(_mmio.dataspace()),

			_attached(env.rm().attach(_ds, {
				.size       = { }, .offset     = { },
				.use_at     = { }, .at         = { },
				.executable = { }, .writeable  = true })),

			_at(_attached.convert<addr_t>(
				[&] (Env::Local_rm::Attachment const &a) {
					return _with_sub_page_offset(addr_t(a.ptr), base); },
				[&] (Env::Local_rm::Error) { return 0UL; }))
		{
			if (!_ds.valid()) throw Attached_dataspace::Invalid_dataspace();
			if (!_at)         throw Attached_dataspace::Region_conflict();
		}

		/**
		 * Return capability of the used RAM dataspace
		 */
		Io_mem_dataspace_capability cap() { return _ds; }

		/**
		 * Request local address
		 *
		 * This is a template to avoid inconvenient casts at the caller.
		 * A newly allocated I/O MEM dataspace is untyped memory anyway.
		 */
		template <typename T>
		T *local_addr() { return reinterpret_cast<T *>(_at); }
};

#endif /* _INCLUDE__BASE__ATTACHED_IO_MEM_DATASPACE_H_ */
