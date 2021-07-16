/*
 * \brief  C ABI for panicking / blocking forever
 * \author Norman Feske
 * \date   2021-03-08
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/sleep.h>

/* Linux emulation environment includes */
#include <legacy/lx_emul/extern_c_begin.h>
#include <legacy/lx_emul/bug.h>
#include <legacy/lx_emul/extern_c_end.h>


extern "C" void lx_sleep_forever()
{
	Genode::sleep_forever();
}
