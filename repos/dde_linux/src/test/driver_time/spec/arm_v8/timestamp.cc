/*
 * \brief  Timestamp to Genode mapping
 * \author Sebastian Sumpf
 * \date   2024-07-01
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <trace/timestamp.h>

extern "C" unsigned long long lx_emul_timestamp();
extern "C" unsigned long long lx_emul_timestamp()
{
	return Genode::Trace::timestamp();
}
