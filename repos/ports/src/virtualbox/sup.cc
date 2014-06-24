/*
 * \brief  VirtualBox SUPLib supplements
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
#include <os/attached_ram_dataspace.h>
#include <base/semaphore.h>
#include <os/timed_semaphore.h>

/* Genode/Virtualbox includes */
#include "sup.h"

/* VirtualBox includes */
#include <VBox/err.h>


struct Attached_gip : Genode::Attached_ram_dataspace
{
	Attached_gip()
	: Attached_ram_dataspace(Genode::env()->ram_session(), PAGE_SIZE)
	{ }
};


enum {
	UPDATE_HZ  = 100,                     /* Hz */
	/* Note: UPDATE_MS < 10ms is not supported by alarm timer, take care !*/
	UPDATE_MS  = 1000 / UPDATE_HZ,
	UPDATE_NS  = UPDATE_MS * 1000 * 1000,
};


PSUPGLOBALINFOPAGE g_pSUPGlobalInfoPage;


class Periodic_GIP : public Genode::Alarm {

	bool on_alarm(unsigned) override
	{
		/**
		 * We're using rdtsc here since timer_session->elapsed_ms produces
		 * instable results when the timer service is using the Genode PIC
		 * driver as done for base-nova currently.
		 */
		static unsigned long long tsc_last = 0;

		Genode::uint32_t now_low, now_high;
		asm volatile("rdtsc" :  "=a"(now_low), "=d"(now_high) : : "memory");

		Genode::uint64_t tsc_current = now_high;
		tsc_current <<= 32;
		tsc_current  |= now_low;

		Genode::uint64_t elapsed_tsc;
		Genode::uint32_t elapsed_ms;
		enum { BOGUS_MULTIPLIER = 10 };

		/* handle wrap around, backwards running tsc and too bogus timeouts */
		if (tsc_current < tsc_last ||
		    tsc_current - tsc_last > BOGUS_MULTIPLIER * UPDATE_MS * genode_cpu_hz() / 1000) {

/*
			if (tsc_current < tsc_last)
				PDBG("bogus timeout - set fixed to %lums - calculated ms %llu "
				     " - time wrapped", UPDATE_MS,
				     (tsc_current - tsc_last) * 1000 / genode_cpu_hz());
			else
				PDBG("bogus timeout - set fixed to %lums - calculated ms %llu "
				     " > %u * %lums", UPDATE_MS,
				     (tsc_current - tsc_last) * 1000 / genode_cpu_hz(),
				     BOGUS_MULTIPLIER, UPDATE_MS);
*/
			elapsed_ms  = UPDATE_MS;
			elapsed_tsc = UPDATE_MS * genode_cpu_hz() / 1000;

		} else {
			elapsed_tsc = tsc_current - tsc_last;
			elapsed_ms  = elapsed_tsc * 1000 / genode_cpu_hz();
		}

		Genode::uint64_t elapsed_nanots = 1000ULL * 1000 * elapsed_ms;

		tsc_last        = tsc_current;



		SUPGIPCPU *cpu = &g_pSUPGlobalInfoPage->aCPUs[0];

		/*
		 * Transaction id must be incremented before and after update,
		 * read struct SUPGIPCPU description for more details.
		 */
		ASMAtomicIncU32(&cpu->u32TransactionId);

		cpu->u64NanoTS += elapsed_nanots;
		cpu->u64TSC    += elapsed_tsc;

		/*
		 * Transaction id must be incremented before and after update,
		 * read struct SUPGIPCPU description for more details.
		 */
		ASMAtomicIncU32(&cpu->u32TransactionId);

		return true;
	}
};



int SUPR3Init(PSUPDRVSESSION *ppSession)
{
	static bool initialized = false;

	if (initialized) return VINF_SUCCESS;

	static Attached_gip gip;

	g_pSUPGlobalInfoPage = gip.local_addr<SUPGLOBALINFOPAGE>();

	/* checked by TMR3Init */
	g_pSUPGlobalInfoPage->u32Version            = SUPGLOBALINFOPAGE_VERSION;
	g_pSUPGlobalInfoPage->u32Magic              = SUPGLOBALINFOPAGE_MAGIC;
	g_pSUPGlobalInfoPage->u32Mode               = SUPGIPMODE_SYNC_TSC;
	g_pSUPGlobalInfoPage->cCpus                 = 1;
	g_pSUPGlobalInfoPage->cPages                = 1;
	g_pSUPGlobalInfoPage->u32UpdateHz           = UPDATE_HZ;
	g_pSUPGlobalInfoPage->u32UpdateIntervalNS   = UPDATE_NS;
//	g_pSUPGlobalInfoPage->u64NanoTSLastUpdateHz =
//	g_pSUPGlobalInfoPage->OnlineCpuSet          =
//	g_pSUPGlobalInfoPage->PresentCpuSet         =
//	g_pSUPGlobalInfoPage->PossibleCpuSet        =
	g_pSUPGlobalInfoPage->cOnlineCpus           = 0;
	g_pSUPGlobalInfoPage->cPresentCpus          = 0;
	g_pSUPGlobalInfoPage->cPossibleCpus         = 0;
	g_pSUPGlobalInfoPage->idCpuMax              = 0;

	SUPGIPCPU *cpu = &g_pSUPGlobalInfoPage->aCPUs[0];

	cpu->u32TransactionId        = 0;
	cpu->u32UpdateIntervalTSC    = genode_cpu_hz() / UPDATE_HZ;
	cpu->u64NanoTS               = 0ULL;
	cpu->u64TSC                  = 0ULL;
	cpu->u64CpuHz                = genode_cpu_hz();
	cpu->cErrors                 = 0;
	cpu->iTSCHistoryHead         = 0;
//	cpu->au32TSCHistory[8]       =
	cpu->u32PrevUpdateIntervalNS = UPDATE_NS;
	cpu->enmState                = SUPGIPCPUSTATE_ONLINE;
	cpu->idCpu                   = 0;
	cpu->iCpuSet                 = 0;
	cpu->idApic                  = 0;

	/* schedule periodic call of GIP update function */
	static Periodic_GIP alarm;
	Genode::Timeout_thread::alarm_timer()->schedule(&alarm, UPDATE_MS);

	initialized = true;

	return VINF_SUCCESS;
}


int SUPR3GipGetPhys(PRTHCPHYS pHCPhys)
{
	/*
	 * Return VMM-local address as physical address. This address is
	 * then fed to MMR3HyperMapHCPhys. (TMR3Init)
	 */
	*pHCPhys = (RTHCPHYS)g_pSUPGlobalInfoPage;

	return VINF_SUCCESS;
}


int SUPSemEventCreate(PSUPDRVSESSION pSession, PSUPSEMEVENT phEvent)
{
	*phEvent = (SUPSEMEVENT)new Genode::Semaphore();
	return VINF_SUCCESS;
}


int SUPSemEventClose(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
	if (hEvent)
		delete reinterpret_cast<Genode::Semaphore *>(hEvent);
	return VINF_SUCCESS;
}


int SUPSemEventSignal(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
	if (hEvent)
		reinterpret_cast<Genode::Semaphore *>(hEvent)->up();
	else
		PERR("%s called %lx", __FUNCTION__, hEvent);

	return VINF_SUCCESS;	
}


int SUPSemEventWaitNoResume(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent,
                            uint32_t cMillies)
{
	if (hEvent && cMillies == RT_INDEFINITE_WAIT)
		reinterpret_cast<Genode::Semaphore *>(hEvent)->down();
	else {
		PERR("%s called %lx millis=%u - not implemented", __FUNCTION__, hEvent, cMillies);
		reinterpret_cast<Genode::Semaphore *>(hEvent)->down();
	}

	return VINF_SUCCESS;	
}


int SUPR3CallVMMR0(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation,
                   void *pvArg)
{
	PDBG("SUPR3CallVMMR0 called uOperation=%d", uOperation);

	if (uOperation == VMMR0_DO_CALL_HYPERVISOR) {
		PDBG("VMMR0_DO_CALL_HYPERVISOR - doing nothing");
		return VINF_SUCCESS;
	}

	PDBG("SUPR3CallVMMR0Ex: unhandled uOperation %d", uOperation);
	for (;;);
}
