/*
 * \brief  Core-internal dataspace representation on Linux
 * \author Norman Feske
 * \date   2006-05-19
 *
 * On Linux userland, we do not deal with physical memory. Instead,
 * we create a file for each dataspace that is to be mmapped.
 * Therefore, the allocator is not really used for allocating
 * memory but only as a container for quota.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__DATASPACE_COMPONENT_H_
#define _CORE__INCLUDE__DATASPACE_COMPONENT_H_

#include <linux_dataspace/linux_dataspace.h>
#include <util/string.h>
#include <util/misc_math.h>
#include <base/rpc_server.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

namespace Genode {

	/**
	 * Deriving classes can own a dataspace to implement conditional behavior
	 */
	class Dataspace_owner { };

	class Dataspace_component : public Rpc_object<Linux_dataspace>
	{
		private:

			Filename       _fname;              /* filename for mmap          */
			size_t         _size;               /* size of dataspace in bytes */
			addr_t         _addr;               /* meaningless on linux       */
			int            _fd;                 /* file descriptor            */
			bool           _writable;           /* false if read-only         */

			/* Holds the dataspace owner if a distinction between owner and
			 * others is necessary on the dataspace, otherwise it is 0 */
			Dataspace_owner * _owner;

			static Filename _file_name(const char *args);
			size_t _file_size();

		public:

			/**
			 * Constructor
			 */
			Dataspace_component(size_t size, addr_t addr,
			                    Cache_attribute, bool writable,
			                    Dataspace_owner * owner)
			: _size(size), _addr(addr), _fd(-1), _writable(writable),
			  _owner(owner) { }

			/**
			 * Default constructor returns invalid dataspace
			 */
			Dataspace_component()
			: _size(0), _addr(0), _fd(-1), _writable(false), _owner(0) { }

			/**
			 * This constructor is only provided for compatibility
			 * reasons and should not be used.
			 */
			Dataspace_component(size_t size, addr_t core_local_addr,
			                    addr_t phys_addr, Cache_attribute,
			                    bool writable, Dataspace_owner * _owner)
			:
				_size(size), _addr(phys_addr), _fd(-1), _owner(_owner)
			{
				warning("Should only be used for IOMEM and not within Linux.");
				_fname.buf[0] = 0;
			}

			/**
			 * This constructor is especially used for ROM dataspaces
			 *
			 * \param args  session parameters containing 'filename' key/value
			 */
			Dataspace_component(const char *args);

			/**
			 * Assign file descriptor to dataspace
			 *
			 * The file descriptor assigned to the dataspace will be enable
			 * processes outside of core to mmap the dataspace.
			 */
			void fd(int fd) { _fd = fd; }

			/**
			 * Check if dataspace is owned by a specified object
			 */
			bool owner(Dataspace_owner const *o) const { return _owner == o; }

			/**
			 * Detach dataspace from all rm sessions.
			 */
			void detach_from_rm_sessions() { }

			/*************************
			 ** Dataspace interface **
			 *************************/

			size_t size()      { return _size; }
			addr_t phys_addr() { return _addr; }
			bool   writable()  { return _writable; }


			/****************************************
			 ** Linux-specific dataspace interface **
			 ****************************************/

			Filename fname() { return _fname; }

			Untyped_capability fd()
			{
				Untyped_capability fd_cap =
					Capability_space::import(Rpc_destination(_fd), Rpc_obj_key());

				return fd_cap;
			}
	};
}

#endif /* _CORE__INCLUDE__DATASPACE_COMPONENT_H_ */
