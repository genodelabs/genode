/*
 * \brief  VirtualBox pluggable device manager (PDM)
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* VirtualBox includes */
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmusb.h>

#include "util.h"


static void RCSymbolDummy()
{
	Genode::log(__func__, ": unexpected call of RC symbol");
	for (;;);
}


int PDMR3LdrGetSymbolRC(PVM pVM, const char *pszModule, const char *pszSymbol,
                        PRTRCPTR pRCPtrValue)
{
	*pRCPtrValue = to_rtrcptr(RCSymbolDummy);
	return VINF_SUCCESS;
}


int PDMR3LdrGetSymbolRCLazy(PVM pVM, const char *pszModule,
                            const char *pszSearchPath, const char *pszSymbol,
                            PRTRCPTR pRCPtrValue)
{
	*pRCPtrValue = to_rtrcptr(RCSymbolDummy);
	return VINF_SUCCESS;
}


static void R0SymbolDummy()
{
	Assert(!"unexpected call of R0 symbol");
}


int PDMR3LdrGetSymbolR0(PVM pVM, const char *pszModule, const char *pszSymbol,
                        PRTR0PTR ppvValue)
{
	*ppvValue = (RTR0PTR)R0SymbolDummy;
	return VINF_SUCCESS;
}


int PDMR3LdrGetSymbolR0Lazy(PVM pVM, const char *pszModule,
                            const char *pszSearchPath, const char *pszSymbol,
                            PRTR0PTR ppvValue)
{
	*ppvValue = (RTR0PTR)R0SymbolDummy;
	return VINF_SUCCESS;
}


extern "C" int VBoxDriversRegister(PCPDMDRVREGCB, uint32_t);
extern "C" int VBoxDevicesRegister(PPDMDEVREGCB,  uint32_t);
extern "C" int VBoxDriversRegister_Main(PCPDMDRVREGCB, uint32_t);
extern "C" int VBoxUsbRegister(PCPDMUSBREGCB, uint32_t);


static int dummy_VBoxDriversRegister(PCPDMDRVREGCB, uint32_t) { return VINF_SUCCESS; }
static int dummy_VBoxDevicesRegister(PPDMDEVREGCB,  uint32_t) { return VINF_SUCCESS; }


int PDMR3LdrGetSymbolR3(PVM pVM, const char *pszModule, const char *pszSymbol,
                        void **ppvValue)
{
	/*
	 * This function is called at initialization time via
	 * PDMR3Init -> pdmR3DrvInit -> pdmR3DrvLoad -> PDMR3LdrGetSymbolR3
	 *
	 * In this case, it is expected to return the pointer to the symbol
	 * called 'VBoxDriversRegister', which is normally contained in the
	 * dynamically loaded VBoxDD module. However, we link the driver
	 * statically to the binary. So we return the local pointer.
	 */
	if (Genode::strcmp(pszModule, "VBoxDD") == 0) {

		if (Genode::strcmp(pszSymbol, "VBoxDriversRegister") == 0) {
			*ppvValue = (void *)VBoxDriversRegister;
			return VINF_SUCCESS;
		}

		if (Genode::strcmp(pszSymbol, "VBoxDevicesRegister") == 0) {
			*ppvValue = (void *)VBoxDevicesRegister;
			return VINF_SUCCESS;
		}
		if (Genode::strcmp(pszSymbol, "VBoxUsbRegister") == 0) {
			*ppvValue = (void *)VBoxUsbRegister;
			return VINF_SUCCESS;
		}
	}

	if (Genode::strcmp(pszModule, "VBoxDD2") == 0) {

		if (Genode::strcmp(pszSymbol, "VBoxDriversRegister") == 0) {
			*ppvValue = (void *)dummy_VBoxDriversRegister;
			return VINF_SUCCESS;
		}

		if (Genode::strcmp(pszSymbol, "VBoxDevicesRegister") == 0) {
			*ppvValue = (void *)dummy_VBoxDevicesRegister;
			return VINF_SUCCESS;
		}
	}

	if (Genode::strcmp(pszModule, "VBoxC") == 0) {
		if (Genode::strcmp(pszSymbol, "VBoxDriversRegister") == 0) {
			*ppvValue = (void *)VBoxDriversRegister_Main;
			return VINF_SUCCESS;
		}
	}

	Genode::error("pszModule=", pszModule, " pszSymbol=", pszSymbol);

	return VERR_SYMBOL_NOT_FOUND;
}


