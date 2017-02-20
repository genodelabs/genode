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


static Genode::Signal_receiver *resource_sig_rec()
{
	static Genode::Signal_receiver sig_rec;
	return &sig_rec;
}


Genode::Signal_context_capability
Genode::Expanding_parent_client::_fallback_sig_cap()
{
	static Signal_context            _sig_ctx;
	static Signal_context_capability _sig_cap;

	/* create signal-context capability only once */
	if (!_sig_cap.valid()) {

		/*
		 * Because the 'manage' function consumes meta data of the signal
		 * session, calling it may result in an 'Out_of_metadata' error. The
		 * 'manage' function handles this error by upgrading the session quota
		 * accordingly. However, this upgrade, in turn, may result in the
		 * depletion of the process' RAM quota. In this case, the process would
		 * issue a resource request to the parent. But in order to do so, the
		 * fallback signal handler has to be constructed. To solve this
		 * hen-and-egg problem, we allocate a so-called emergency RAM reserve
		 * immediately at the startup of the process as part of the
		 * 'Platform_env'. When initializing the fallback signal handler, these
		 * resources get released in order to ensure an eventual upgrade of the
		 * signal session to succeed.
		 *
		 * The corner case is tested by 'os/src/test/resource_request'.
		 */
		_emergency_ram_reserve.release();
		_sig_cap = resource_sig_rec()->manage(&_sig_ctx);
	}

	return _sig_cap;
}


void Genode::Expanding_parent_client::_wait_for_resource_response()
{
	resource_sig_rec()->wait_for_signal();
}
