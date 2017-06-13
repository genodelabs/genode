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
#include <base/connection.h>
#include <base/service.h>

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


void Genode::init_parent_resource_requests(Genode::Env & env)
{
	/**
	 * Catch up asynchronous resource request and notification
	 * mechanism construction of the expanding parent environment
	 */
	using Parent = Expanding_parent_client;
	static_cast<Parent*>(&env.parent())->init_fallback_signal_handling();
}
