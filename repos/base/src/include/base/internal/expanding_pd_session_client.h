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
	Parent &_parent;

	void _request_ram_from_parent(size_t amount)
	{
		_parent.resource_request(String<128>("ram_quota=", amount).string());
	}

	void _request_caps_from_parent(size_t amount)
	{
		_parent.resource_request(String<128>("cap_quota=", amount).string());
	}

	Expanding_pd_session_client(Parent &parent, Pd_session_capability cap)
	: Pd_session_client(cap), _parent(parent) { }

	Alloc_result try_alloc(size_t size, Cache cache) override
	{
		/*
		 * If the PD session runs out of quota, issue a resource request
		 * to the parent and retry.
		 */
		for (;;) {
			Alloc_result const result = Pd_session_client::try_alloc(size, cache);
			if (result.ok())
				return result;

			bool denied = false;
			result.with_error(
				[&] (Alloc_error error) {
					switch (error) {

					case Alloc_error::OUT_OF_RAM:
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
						break;

					case Alloc_error::OUT_OF_CAPS:
						_request_caps_from_parent(4);
						break;

					case Alloc_error::DENIED:
						denied = true;
					}
				});

			if (denied)
				return Alloc_error::DENIED;
		}
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

	void transfer_quota(Pd_session_capability pd_session, Cap_quota amount) override
	{
		enum { NUM_ATTEMPTS = 2 };
		retry<Out_of_caps>(
			[&] () { Pd_session_client::transfer_quota(pd_session, amount); },
			[&] () { _request_caps_from_parent(amount.value); },
			NUM_ATTEMPTS);
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__EXPANDING_PD_SESSION_CLIENT_H_ */
