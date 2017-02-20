/*
 * \brief  Support to link libraries statically supposed to be dynamic
 * \author Alexander Boettcher
 * \date   2014-05-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* VirtualBox includes */
#include <iprt/err.h>
#include <iprt/ldr.h>
#include <VBox/hgcmsvc.h>

extern "C" {

DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad_sf (VBOXHGCMSVCFNTABLE *ptable);
DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad_cb (VBOXHGCMSVCFNTABLE *ptable);


static struct shared {
	const char * name;
	const char * symbol;
	void * func;
} shared[] = {
	{ "VBoxSharedFolders", VBOX_HGCM_SVCLOAD_NAME, (void *)VBoxHGCMSvcLoad_sf },
	{ "VBoxSharedClipboard", VBOX_HGCM_SVCLOAD_NAME, (void *)VBoxHGCMSvcLoad_cb }
};


int RTLdrLoad(const char *pszFilename, PRTLDRMOD phLdrMod)
{
	for (unsigned i = 0; i < sizeof(shared) / sizeof(shared[0]); i++) {
		if (Genode::strcmp(shared[i].name, pszFilename))
			continue;

		*phLdrMod = reinterpret_cast<RTLDRMOD>(&shared[i]);
		return VINF_SUCCESS;
	}

	Genode::error("shared library '", pszFilename, "' not supported");
	return VERR_NOT_SUPPORTED;
}


int RTLdrGetSymbol(RTLDRMOD hLdrMod, const char *pszSymbol, void **ppvValue) 
{
	
	struct shared * library = reinterpret_cast<struct shared *>(hLdrMod);

	if (!(shared <= library &&
	      library < shared + sizeof(shared) / sizeof(shared[0]))) {

		Genode::error("shared library handle ", hLdrMod, " unknown - "
		              "symbol looked for '", pszSymbol, "'");

		return VERR_NOT_SUPPORTED;
	}

	if (Genode::strcmp(pszSymbol, library->symbol)) {
		Genode::error("shared library '", library->name, "' does not provide "
		              "symbol '", pszSymbol, "'");

		return VERR_NOT_SUPPORTED;
	}

	*ppvValue = library->func;

	return VINF_SUCCESS;
}

int RTLdrClose(RTLDRMOD hLdrMod)
{
	return VINF_SUCCESS;
}

}
