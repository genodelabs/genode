/*
 * \brief  PD-session client that issues resource requests on demand
 * \author Norman Feske
 * \date   2013-09-25
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__EXPANDING_PD_SESSION_CLIENT_H_
#define _INCLUDE__BASE__INTERNAL__EXPANDING_PD_SESSION_CLIENT_H_

/* Genode includes */
#include <util/retry.h>
#include <pd_session/client.h>

namespace Genode { class Expanding_pd_session_client; }


struct Genode::Expanding_pd_session_client : Pd_session_client
{
	void _request_ram_from_parent(size_t amount)
	{
		Parent &parent = *env_deprecated()->parent();
		parent.resource_request(String<128>("ram_quota=", amount).string());
	}

	void _request_caps_from_parent(size_t amount)
	{
		Parent &parent = *env_deprecated()->parent();
		parent.resource_request(String<128>("cap_quota=", amount).string());
	}

	Expanding_pd_session_client(Pd_session_capability cap) : Pd_session_client(cap) { }

	Ram_dataspace_capability alloc(size_t size, Cache_attribute cached = UNCACHED) override
	{
		/*
		 * If the RAM session runs out of quota, issue a resource request
		 * to the parent and retry.
		 */
		enum { NUM_ATTEMPTS = 2 };
		enum { UPGRADE_CAPS = 4 };
		return retry<Out_of_ram>(
			[&] () {
				return retry<Out_of_caps>(
					[&] () { return Pd_session_client::alloc(size, cached); },
					[&] () {
						warning("cap quota exhausted, issuing resource request to parent");
						_request_caps_from_parent(UPGRADE_CAPS);
					},
					NUM_ATTEMPTS);
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

	void transfer_quota(Pd_session_capability pd_session, Ram_quota amount) override
	{
		/*
		 * Should the transfer fail because we don't have enough quota, request
		 * the needed amount from the parent.
		 */
		enum { NUM_ATTEMPTS = 2 };
		retry<Out_of_ram>(
			[&] () { Pd_session_client::transfer_quota(pd_session, amount); },
			[&] () { _request_ram_from_parent(amount.value); },
			NUM_ATTEMPTS);
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__EXPANDING_PD_SESSION_CLIENT_H_ */
