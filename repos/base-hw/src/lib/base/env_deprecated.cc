/*
 * \brief  Environment initialization
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-27
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/platform_env.h>
#include <base/internal/globals.h>
#include <base/internal/native_env.h>
#include <base/connection.h>
#include <base/service.h>

#include <hw_native_pd/client.h>

namespace Genode {

	/*
	 * Request pointer to static environment of the Genode application
	 */
	Env_deprecated *env_deprecated()
	{
		/*
		 * By placing the environment as static object here, we ensure that its
		 * constructor gets called when this function is used the first time.
		 */
		static Genode::Platform_env _env;
		return &_env;
	}
}


using Native_pd_capability = Genode::Capability<Genode::Pd_session::Native_pd>;
static Native_pd_capability native_pd_cap;


void Genode::init_parent_resource_requests(Genode::Env & env)
{
	/**
	 * Catch up asynchronous resource request and notification
	 * mechanism construction of the expanding parent environment
	 */
	using Parent = Expanding_parent_client;
	static_cast<Parent*>(&env.parent())->init_fallback_signal_handling();
	native_pd_cap = env.pd().native_pd();
}


void Genode::upgrade_capability_slab()
{
	if (!native_pd_cap.valid()) {
		Genode::error("Cannot upgrade capability slab "
		              "not initialized appropriatedly");
		return;
	}

	auto request_resources_from_parent = [&] (Ram_quota ram, Cap_quota caps)
	{
		/*
		 * The call of 'resource_request' is handled synchronously by
		 * 'Expanding_parent_client'.
		 */
		String<100> const args("ram_quota=", ram, ", cap_quota=", caps);
		internal_env().parent().resource_request(args.string());
	};

	retry<Genode::Out_of_caps>(
		[&] () {
			retry<Genode::Out_of_ram>(
				[&] () {
					Genode::Hw_native_pd_client pd(native_pd_cap);
					pd.upgrade_cap_slab();
				},
				[&] () {
					request_resources_from_parent(Ram_quota{8192}, Cap_quota{0});
				});
		},
		[&] () {
			request_resources_from_parent(Ram_quota{0}, Cap_quota{2});
		});
}
