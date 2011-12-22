/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-17
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

namespace Fiasco {
#include <l4/util/util.h>
}

using namespace Fiasco;

extern "C" {

	void l4_sleep(int ms)
	{
		PWRN("%s: Not implemented yet!", __func__);
	}


	void l4_sleep_forever(void)
	{
		for (;;)
			l4_ipc_sleep(L4_IPC_NEVER);
	}

}
