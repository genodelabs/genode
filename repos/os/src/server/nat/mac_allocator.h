/*
 * \brief  MAC-address allocator
 * \author Stefan Kalkowski
 * \date   2010-08-25
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MAC_H_
#define _MAC_H_

/* Genode */
#include <base/exception.h>
#include <net/ethernet.h>

namespace Net {

	/**
	 * The MAC allocator is used to administer MAC addresses for
	 * the proxy-ARP's client's.
	 */
	class Mac_allocator
	{
		private:

			/* limit available MAC addresses to one byte range */
			enum { MSB_MAX = 0xFF };

			/* signals, whether most significant byte is in use */
			typedef struct
			{
					unsigned used : 1;
			} Msb;

			Msb _msbs[MSB_MAX]; /* bitfield of MSBs */

		public:

			struct Alloc_failed : Genode::Exception {};


			/* reference MAC address */
			static Mac_address mac_addr_base;

			Mac_allocator() { Genode::memset(&_msbs, 0, sizeof(_msbs)); }


			/**
			 * Allocates a new MAC address.
			 *
			 * \throws Alloc_failed if no more MAC addresses are available.
			 * \return MAC address
			 */
			Mac_address alloc()
			{
				for (int i=0; i < MSB_MAX; i++) {
					if (!_msbs[i].used) {
						_msbs[i].used = 1;
						Mac_address mac = mac_addr_base;
						mac.addr[5] = i;
						return mac;
					}
				}
				throw Alloc_failed();
			}

			/**
			 * Frees a formerly allocated MAC address.
			 */
			void free(Mac_address mac) {
				_msbs[(unsigned)mac.addr[5]].used = 0; }
	};
}

#endif /* _MAC_H_ */
