/*
 * \brief  Platform specific part of memory allocation
 * \author Alexander Boettcher 
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ARM__PLATFORM__LX_MEM_
#define _ARM__PLATFORM__LX_MEM_

class Backend_memory {

	public:

		static Genode::Ram_dataspace_capability alloc(Genode::addr_t size,
		                                              bool cached) {
			return Genode::env()->ram_session()->alloc(size, cached); }

		static void free(Genode::Ram_dataspace_capability cap) {
			return Genode::env()->ram_session()->free(cap); }
};

#endif /* _ARM__PLATFORM__LX_MEM_ */
