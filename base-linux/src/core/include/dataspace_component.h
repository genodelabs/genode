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
 * Copyright (C) 2006-2011 Genode Labs GmbH
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

	class Dataspace_component : public Rpc_object<Linux_dataspace>
	{
		private:

			size_t         _size;               /* size of dataspace in bytes */
			addr_t         _addr;               /* meaningless on linux       */
			Filename       _fname;              /* filename for mmap          */
			bool           _writable;           /* false if read-only         */

		public:

			/**
			 * Constructor
			 */
			Dataspace_component(size_t size, addr_t addr, bool writable)
			: _size(size), _addr(addr), _writable(writable) { }

			/**
			 * Default constructor returns invalid dataspace
			 */
			Dataspace_component() : _size(0), _addr(0), _writable(false) { }

			/**
			 * This constructor is only provided for compatibility
			 * reasons and should not be used.
			 */
			Dataspace_component(size_t size, addr_t core_local_addr,
			                    addr_t phys_addr, bool write_combined,
			                    bool writable)
				: _size(size), _addr(phys_addr)
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
