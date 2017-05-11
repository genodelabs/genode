/*
 * \brief  VirtualBox page manager (PGM)
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* VirtualBox includes */
#include "PGMInternal.h"
#include <VBox/vmm/pgm.h>

static bool verbose = false;

int PGMMap(PVM pVM, RTGCUINTPTR GCPtr, RTHCPHYS HCPhys, uint32_t cbPages,
           unsigned fFlags)
{
	/* GCPtr steams from the #unimplemented# MMR3HyperReserve call, which
	 * returns a value - seems to be used solely in RC mode */

	if (verbose)
		Genode::log(__func__, ": GCPtr=", Genode::Hex(GCPtr), " "
		            "HCPHys=", Genode::Hex(HCPhys), " "
		            "cbPages=", Genode::Hex(cbPages), " , "
		            "flags=", Genode::Hex(fFlags), " "
		            "rip=", __builtin_return_address(0));

	return VINF_SUCCESS;
}


int PGMMapSetPage(PVM pVM, RTGCPTR GCPtr, uint64_t cb, uint64_t fFlags)
{
	if (verbose)
		Genode::log(__func__, ": GCPtr=", Genode::Hex(GCPtr), " "
		            "cb=", Genode::Hex(cb), " "
		            "flags=", Genode::Hex(fFlags));

	return VINF_SUCCESS;
}


int PGMR3MapPT(PVM, RTGCPTR GCPtr, uint32_t cb, uint32_t fFlags,
               PFNPGMRELOCATE pfnRelocate, void *pvUser, const char *pszDesc)
{
	if (verbose)
		Genode::log(__func__, " GCPtr=", Genode::Hex(GCPtr), "+", Genode::Hex(cb),
		            " flags=", Genode::Hex(fFlags), " pvUser=", pvUser,
		            " desc=", pszDesc);

	return VINF_SUCCESS;
}


int PGMR3MappingsSize(PVM pVM, uint32_t *pcb)
{
	Genode::log(__func__, ": not implemented ", __builtin_return_address(0));

	*pcb = 0;

	return 0;
}



int pgmMapActivateCR3(PVM, PPGMPOOLPAGE)
{
	if (verbose)
		Genode::log(__func__, ": not implemented ", __builtin_return_address(0));
	return VINF_SUCCESS;
}


int pgmMapDeactivateCR3(PVM, PPGMPOOLPAGE)
{
	if (verbose)
		Genode::log(__func__, ": not implemented ", __builtin_return_address(0));
	return VINF_SUCCESS;
}
