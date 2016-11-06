/*
 * \brief  RAM-session client that upgrades its session quota on demand
 * \author Norman Feske
 * \date   2013-09-25
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__EXPANDING_RAM_SESSION_CLIENT_H_
#define _INCLUDE__BASE__INTERNAL__EXPANDING_RAM_SESSION_CLIENT_H_

/* Genode includes */
#include <util/retry.h>
#include <ram_session/client.h>

/* base-internal includes */
#include <base/internal/upgradeable_client.h>

namespace Genode { class Expanding_ram_session_client; }


struct Genode::Expanding_ram_session_client : Upgradeable_client<Genode::Ram_session_client>
{
	Expanding_ram_session_client(Ram_session_capability cap, Parent::Client::Id id)
	: Upgradeable_client<Genode::Ram_session_client>(cap, id) { }

	Ram_dataspace_capability alloc(size_t size, Cache_attribute cached = UNCACHED) override
	{
		/*
		 * If the RAM session runs out of quota, issue a resource request
		 * to the parent and retry.
		 */
		enum { NUM_ATTEMPTS = 2 };
		return retry<Ram_session::Quota_exceeded>(
			[&] () {
				/*
				 * If the RAM session runs out of meta data, upgrade the
				 * session quota and retry.
				 */
				return retry<Ram_session::Out_of_metadata>(
					[&] () { return Ram_session_client::alloc(size, cached); },
					[&] () { upgrade_ram(8*1024); });
			},
			[&] () {
				char buf[128];

				/*
				 * The RAM service withdraws the meta data for the allocator
				 * from the RAM quota. In the worst case, a new slab block
				 * may be needed. To cover the worst case, we need to take
				 * this possible overhead into account when requesting
				 * additional RAM quota from the parent.
				 *
				 * Because the worst case almost never happens, we request
				 * a bit too much quota for the most time.
				 */
				enum { ALLOC_OVERHEAD = 4096U };
				Genode::snprintf(buf, sizeof(buf), "ram_quota=%lu",
				                 size + ALLOC_OVERHEAD);
				env()->parent()->resource_request(buf);
			},
			NUM_ATTEMPTS);
	}

	int transfer_quota(Ram_session_capability ram_session, size_t amount) override
	{
		enum { NUM_ATTEMPTS = 2 };
		int ret = -1;
		for (unsigned i = 0; i < NUM_ATTEMPTS; i++) {

			ret = Ram_session_client::transfer_quota(ram_session, amount);
			if (ret != -3) break;

			/*
			 * The transfer failed because we don't have enough quota. Request
			 * the needed amount from the parent.
			 *
			 * XXX Let transfer_quota throw 'Ram_session::Quota_exceeded'
			 */
			char buf[128];
			Genode::snprintf(buf, sizeof(buf), "ram_quota=%lu", amount);
			env()->parent()->resource_request(buf);
		}
		return ret;
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__EXPANDING_RAM_SESSION_CLIENT_H_ */
