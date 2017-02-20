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
#include <base/env.h>

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

		Region_map                  &_env_rm;
		Io_mem_connection            _mmio;
		Io_mem_dataspace_capability  _ds;
		void                        *_local_addr;

	public:

		/**
		 * Constructor
		 *
		 * \param base            base address of memory-mapped I/O resource
		 * \param size            size of resource
		 * \param write_combined  enable write combining for the resource
		 *
		 * \throw Parent::Service_denied
		 * \throw Parent::Quota_exceeded
		 * \throw Parent::Unavailable
		 * \throw Rm_session::Attach_failed
		 */
		Attached_io_mem_dataspace(Env &env, Genode::addr_t base, Genode::size_t size,
		                          bool write_combined = false)
		:
			_env_rm(env.rm()),
			_mmio(env, base, size, write_combined),
			_ds(_mmio.dataspace()),
			_local_addr(env.rm().attach(_ds))
		{
			/* apply sub-page offset to virtual address */
			_local_addr = (void *)((addr_t)_local_addr | (base & (addr_t)0xfff));
		}

		/**
		 * Constructor
		 *
		 * \noapi
		 * \deprecated  Use the constructor with 'Env &' as first
		 *              argument instead
		 */
		Attached_io_mem_dataspace(Genode::addr_t base, Genode::size_t size,
		                          bool write_combined = false) __attribute__((deprecated))
		:
			_env_rm(*env_deprecated()->rm_session()),
			_mmio(false, base, size, write_combined),
			_ds(_mmio.dataspace()),
			_local_addr(_env_rm.attach(_ds))
		{
			/* apply sub-page offset to virtual address */
			_local_addr = (void *)((addr_t)_local_addr | (base & (addr_t)0xfff));
		}

		/**
		 * Destructor
		 */
		~Attached_io_mem_dataspace() { _env_rm.detach(_local_addr); }

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
		T *local_addr() { return static_cast<T *>(_local_addr); }
};

#endif /* _INCLUDE__BASE__ATTACHED_IO_MEM_DATASPACE_H_ */
