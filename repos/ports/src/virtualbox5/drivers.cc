/*
 * \brief  VirtualBox host drivers
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* VirtualBox includes */
#include <VBoxDD.h>


extern "C" int VBoxDriversRegister(PCPDMDRVREGCB pCallbacks, uint32_t u32Version)
{
	PDMDRVREG const *drvs[] = {
		&g_DrvKeyboardQueue,
		&g_DrvMouseQueue,
//		&g_DrvBlock,
//		&g_DrvMediaISO,
		&g_DrvACPI,
		&g_DrvChar,
//		&g_DrvRawImage,
		&g_DrvRawFile,
		&g_DrvHostSerial,
		&g_DrvVD,
		&g_DrvHostInterface,
		&g_DrvVUSBRootHub,
		&g_DrvAUDIO,
		&g_DrvHostNullAudio,
		0
	};

	for (unsigned i = 0; drvs[i]; i++) {
		int rc = pCallbacks->pfnRegister(pCallbacks, drvs[i]);
		if (RT_FAILURE(rc))
			return rc;
	}

	return VINF_SUCCESS;
}

