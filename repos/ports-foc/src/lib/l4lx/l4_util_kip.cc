/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <util/string.h>

namespace Fiasco {
#include <l4/util/kip.h>
}

#include <linux.h>

using namespace Fiasco;


extern "C" {

	FASTCALL unsigned long int simple_strtoul(const char *nptr, char **endptr,
	                                          int base);


	int l4util_kip_kernel_has_feature(l4_kernel_info_t *k, const char *str)
	{
		const char *s = l4_kip_version_string(k);
		if (!s) {
			PWRN("Kip parsing failed!");
			return 0;
		}

		for (s += Genode::strlen(s) + 1; *s; s += Genode::strlen(s) + 1)
			if (Genode::strcmp(s, str) == 0)
				return 1;
		return 0;
	}


	unsigned long l4util_kip_kernel_abi_version(l4_kernel_info_t *k)
	{
		const char *s = l4_kip_version_string(k);
		if (!s)
			return 0;

		for (s += Genode::strlen(s) + 1; *s; s += Genode::strlen(s) + 1)
			if (Genode::strcmp(s, "abiver:", 7) == 0)
				return simple_strtoul(s + 7, NULL, 0);
		return 0;
	}

}
