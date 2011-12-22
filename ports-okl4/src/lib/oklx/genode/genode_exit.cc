/*
 * \brief  Genode C API exit function needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-08-14
 */

/*
 * Copyright (C) 2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>

extern "C" {

	namespace Okl4 {
#include <genode/exit.h>
	}

	void genode_exit(int value)
	{
		Genode::env()->parent()->exit(1);
	}

} // extern "C"
