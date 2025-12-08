/*
 * \brief  Capability slab management
 * \author Norman Feske
 * \date   2023-06-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/retry.h>
#include <base/internal/globals.h>
#include <base/internal/native_env.h>
#include <hw_native_pd/client.h>

using namespace Genode;


static Parent                *parent_ptr;
static Pd_session::Native_pd *native_pd_ptr;


void Genode::init_cap_slab(Pd_session &pd, Parent &parent)
{
	static Hw_native_pd_client native_pd(pd.native_pd());

	parent_ptr    = &parent;
	native_pd_ptr = &native_pd;
}


size_t Genode::avail_capability_slab()
{
	return native_pd_ptr ? native_pd_ptr->avail_cap_slab() : 0UL;
}


void Genode::upgrade_capability_slab()
{
	if (!native_pd_ptr || !parent_ptr) {
		error("missing call of 'init_cap_slab'");
		return;
	}

	auto request_resources_from_parent = [&] (Ram_quota ram, Cap_quota caps)
	{
		/*
		 * The call of 'resource_request' is handled synchronously by
		 * 'Expanding_parent_client'.
		 */
		String<100> const args("ram_quota=", ram, ", cap_quota=", caps);
		parent_ptr->resource_request(args.string());
	};

	while (true) {
		auto result = native_pd_ptr->upgrade_cap_slab();
		if (result.ok())
			return;

		result.with_error(
			[&] (auto err) {
				switch (err) {
				case Alloc_error::OUT_OF_RAM:
					request_resources_from_parent(Ram_quota{8192},
					                              Cap_quota{0});
					break;
				case Alloc_error::OUT_OF_CAPS:
					request_resources_from_parent(Ram_quota{0},
					                              Cap_quota{2});
					break;
				case Alloc_error::DENIED:
					error("Could not upgrade capability slab, unrecoverable!");
					sleep_forever();
				};
			});
	}
}
