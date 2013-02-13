/*
 * \brief  Environment initialization
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-27
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <platform_env.h>

namespace Genode {

	/*
	 * Request pointer to static environment of the Genode application
	 */
	Env *env()
	{
		/*
		 * By placing the environment as static object here, we ensure that its
		 * constructor gets called when this function is used the first time.
		 */
		static Genode::Platform_env _env;
		return &_env;
	}
}
