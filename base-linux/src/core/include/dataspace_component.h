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
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LINUX__DATASPACE_COMPONENT_H_
#define _CORE__INCLUDE__LINUX__DATASPACE_COMPONENT_H_

#include <linux_dataspace/linux_dataspace.h>
#include <util/string.h>
#include <util/misc_math.h>
#include <base/rpc_server.h>
#include <base/printf.h>

namespace Genode {

	/**
	 * Deriving classes can own a dataspace to implement conditional behavior
	 */
	class Dataspace_owner { };

	class Dataspace_component : public Rpc_object<Linux_dataspace>
	{
		private:

			size_t         _size;               /* size of dataspace in bytes */
			addr_t         _addr;               /* meaningless on linux       */
			Filename       _fname;              /* filename for mmap          */
			bool           _writable;           /* false if read-only         */

			/* Holds the dataspace owner if a distinction between owner and
			 * others is necessary on the dataspace, otherwise it is 0 */
			Dataspace_owner * _owner;

		public:

			/**
			 * Constructor
			 */
			Dataspace_component(size_t size, addr_t addr,
			                    bool /* write_combined */, bool writable,
			                    Dataspace_owner * owner)
			: _size(size), _addr(addr), _writable(writable),
			  _owner(owner) { }

			/**
			 * Default constructor returns invalid dataspace
			 */
			Dataspace_component() : _size(0), _addr(0), _writable(false),
			                        _owner(0) { }

			/**
			 * This constructor is only provided for compatibility
			 * reasons and should not be used.
			 */
			Dataspace_component(size_t size, addr_t core_local_addr,
			                    addr_t phys_addr, bool write_combined,
			                    bool writable, Dataspace_owner * _owner)
				: _size(size), _addr(phys_addr), _owner(_owner)
			{
				PWRN("Should only be used for IOMEM and not within Linux.");
			}

			/**
			 * Define/request corresponding filename of dataspace
			 *
			 * To use dataspaces as shared memory objects on Linux, we have to
			 * assign a file to each dataspace. This way, multiple Linux process
			 * can mmap this file.
			 */
			void fname(const char *fname) { strncpy(_fname.buf, fname, sizeof(_fname.buf)); }

			/**
			 * Check if dataspace is owned by a specified object
			 */
			bool owner(Dataspace_owner * const o) const { return _owner == o; }

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
	};
}

#endif /* _CORE__INCLUDE__LINUX__DATASPACE_COMPONENT_H_ */
