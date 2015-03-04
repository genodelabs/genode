/*
 * \brief  I/O MEM dataspace utility
 * \author Norman Feske
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__ATTACHED_IO_MEM_DATASPACE_H_
#define _INCLUDE__OS__ATTACHED_IO_MEM_DATASPACE_H_

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
		Attached_io_mem_dataspace(Genode::addr_t base, Genode::size_t size,
		                          bool write_combined = false)
		:
			_mmio(base, size, write_combined),
			_ds(_mmio.dataspace()),
			_local_addr(env()->rm_session()->attach(_ds))
		{ }

		/**
		 * Destructor
		 */
		~Attached_io_mem_dataspace()
		{
			env()->rm_session()->detach(_local_addr);
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
		T *local_addr() { return static_cast<T *>(_local_addr); }
};

#endif /* _INCLUDE__OS__ATTACHED_IO_MEM_DATASPACE_H_ */
