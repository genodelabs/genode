/*
 * \brief  RAM-session client that upgrades its session quota on demand
 * \author Norman Feske
 * \date   2013-09-25
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__EXPANDING_RAM_SESSION_CLIENT_H_
#define _INCLUDE__BASE__INTERNAL__EXPANDING_RAM_SESSION_CLIENT_H_

/* Genode includes */
#include <util/retry.h>
#include <ram_session/connection.h>

/* base-internal includes */
#include <base/internal/upgradeable_client.h>

namespace Genode { class Expanding_ram_session_client; }


struct Genode::Expanding_ram_session_client : Upgradeable_client<Genode::Ram_session_client>
{
	void _request_ram_from_parent(size_t amount)
	{
		Parent &parent = *env_deprecated()->parent();
		parent.resource_request(String<128>("ram_quota=", amount).string());
	}
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
				enum { OVERHEAD = 4096UL };
				_request_ram_from_parent(size + OVERHEAD);
			},
			NUM_ATTEMPTS);
	}

	int transfer_quota(Ram_session_capability ram_session, Ram_quota amount) override
	{
		/*
		 * Should the transfer fail because we don't have enough quota, request
		 * the needed amount from the parent.
		 */
		enum { NUM_ATTEMPTS = 2 };
		int ret = -1;
		for (unsigned i = 0; i < NUM_ATTEMPTS; i++) {

			ret = Ram_session_client::transfer_quota(ram_session, amount);
			if (ret != -3) break;

			_request_ram_from_parent(amount.value);
		}
		return ret;
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__EXPANDING_RAM_SESSION_CLIENT_H_ */
