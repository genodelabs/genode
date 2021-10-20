/*
 * \brief  Genode backend for VirtualBox Suplib
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-10-12
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* VirtualBox includes */
#include <iprt/err.h>
#include <iprt/semaphore.h>
#include <VBox/sup.h>
#include <SUPLibInternal.h>

/* local includes */
#include <stub_macros.h>

static bool const debug = true;

/* check type assumptions */
static_assert(sizeof(RTSEMEVENT) == sizeof(PSUPSEMEVENT), "type mismatch");
static_assert(sizeof(RTSEMEVENTMULTI) == sizeof(PSUPSEMEVENTMULTI), "type mismatch");


int SUPSemEventCreate(PSUPDRVSESSION pSession, PSUPSEMEVENT phEvent)
{
	return RTSemEventCreate((RTSEMEVENT *)phEvent);
}


int SUPSemEventClose(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
	Assert (hEvent);

	return RTSemEventDestroy((RTSEMEVENT)hEvent);
}


int SUPSemEventSignal(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
	Assert (hEvent);

	return RTSemEventSignal((RTSEMEVENT)hEvent);
}


int SUPSemEventWaitNoResume(PSUPDRVSESSION pSession,
                            SUPSEMEVENT hEvent,
                            uint32_t cMillies)
{
	Assert (hEvent);

	return RTSemEventWaitNoResume((RTSEMEVENT)hEvent, cMillies);
}


int SUPSemEventWaitNsAbsIntr(PSUPDRVSESSION pSession,
                             SUPSEMEVENT    hEvent,
                             uint64_t       uNsTimeout) STOP


int SUPSemEventWaitNsRelIntr(PSUPDRVSESSION pSession,
                             SUPSEMEVENT    hEvent,
                             uint64_t       cNsTimeout) STOP


uint32_t SUPSemEventGetResolution(PSUPDRVSESSION pSession)
{
	return 10'000'000; /* nanoseconds */
}


int SUPSemEventMultiCreate(PSUPDRVSESSION pSession,
                           PSUPSEMEVENTMULTI phEventMulti)
{
	AssertPtrReturn(phEventMulti, VERR_INVALID_POINTER);

	return RTSemEventMultiCreate((RTSEMEVENTMULTI *)phEventMulti);
}


int SUPSemEventMultiClose(PSUPDRVSESSION   pSession,
                          SUPSEMEVENTMULTI hEventMulti)
{
	return RTSemEventMultiDestroy((RTSEMEVENTMULTI)hEventMulti);
}


int SUPSemEventMultiSignal(PSUPDRVSESSION   pSession,
                           SUPSEMEVENTMULTI hEventMulti)
{
	return RTSemEventMultiSignal((RTSEMEVENTMULTI)hEventMulti);
}


int SUPSemEventMultiReset(PSUPDRVSESSION   pSession,
                          SUPSEMEVENTMULTI hEventMulti)
{
	return RTSemEventMultiReset((RTSEMEVENTMULTI)hEventMulti);
}


int SUPSemEventMultiWaitNoResume(PSUPDRVSESSION   pSession,
                                 SUPSEMEVENTMULTI hEventMulti,
                                 uint32_t         cMillies)
{
	uint32_t fFlags = RTSEMWAIT_FLAGS_RELATIVE | RTSEMWAIT_FLAGS_MILLISECS | RTSEMWAIT_FLAGS_INTERRUPTIBLE;
	if (cMillies == RT_INDEFINITE_WAIT)
		fFlags |= RTSEMWAIT_FLAGS_INDEFINITE;

	return RTSemEventMultiWaitEx((RTSEMEVENTMULTI)hEventMulti, fFlags, cMillies);
}


int SUPSemEventMultiWaitNsAbsIntr(PSUPDRVSESSION   pSession,
                                  SUPSEMEVENTMULTI hEventMulti,
                                  uint64_t         uNsTimeout) STOP


int SUPSemEventMultiWaitNsRelIntr(PSUPDRVSESSION   pSession,
                                  SUPSEMEVENTMULTI hEventMulti,
                                  uint64_t         cNsTimeout)
{
	return RTSemEventMultiWaitNoResume((RTSEMEVENTMULTI)hEventMulti, cNsTimeout/1'000'000);
}


uint32_t SUPSemEventMultiGetResolution(PSUPDRVSESSION pSession)
{
	return 10'000'000; /* nanoseconds */
}

