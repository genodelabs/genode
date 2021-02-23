/*
 * \brief  Support to link libraries statically supposed to be dynamic
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2014-05-13
 */

/*
 * Copyright (C) 2014-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <util/string.h>

/* VirtualBox includes */
#include <iprt/err.h>
#include <iprt/ldr.h>
#include <VBox/hgcmsvc.h>

extern "C" {

int RTLdrLoad(const char *pszFilename, PRTLDRMOD phLdrMod)
{
	Genode::error("shared library '", pszFilename, "' not supported");
	return VERR_NOT_SUPPORTED;
}


RTDECL(const char *) RTLdrGetSuff(void)
{
	return ".so";
}

} /* extern "C" */
