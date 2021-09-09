/*
 * \brief  VirtualBox host drivers
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* VirtualBox includes */
#include <VBoxDD.h>
#include <KeyboardImpl.h>
#include <MouseImpl.h>
#include <VMMDev.h>
#include <ConsoleImpl.h>
#include <DisplayImpl.h>

#define REGISTER(driver) \
	do { \
		rc = pCallbacks->pfnRegister(pCallbacks, &driver); \
		if (RT_FAILURE(rc)) \
			return rc; \
	} while (0)


extern "C" DECLEXPORT(int) VBoxDriversRegister(PCPDMDRVREGCB pCallbacks, uint32_t u32Version)
{
	int rc = 0;

	REGISTER(g_DrvMouseQueue);
	REGISTER(g_DrvKeyboardQueue);
	REGISTER(g_DrvVD);
	REGISTER(g_DrvSCSI);
	REGISTER(g_DrvHostDVD);
	REGISTER(g_DrvHostInterface);
	REGISTER(g_DrvAUDIO);
	REGISTER(g_DrvHostNullAudio);
	REGISTER(g_DrvHostOSSAudio);
	REGISTER(g_DrvACPI);
	REGISTER(g_DrvAcpiCpu);
	REGISTER(g_DrvVUSBRootHub);
	REGISTER(g_DrvNamedPipe);
	REGISTER(g_DrvTCP);
	REGISTER(g_DrvUDP);
	REGISTER(g_DrvRawFile);
	REGISTER(g_DrvChar);
	REGISTER(g_DrvHostSerial);
	REGISTER(g_DrvIfTrace);

	REGISTER(Keyboard::DrvReg);
	REGISTER(Mouse::DrvReg);
	REGISTER(VMMDev::DrvReg);
	REGISTER(Console::DrvStatusReg);
	REGISTER(Display::DrvReg);

	return VINF_SUCCESS;
}

