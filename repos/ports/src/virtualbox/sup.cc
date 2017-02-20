/*
 * \brief  VirtualBox SUPLib supplements
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
#include <base/attached_ram_dataspace.h>
#include <trace/timestamp.h>

/* Genode/Virtualbox includes */
#include "sup.h"
#include "vmm.h"

/* VirtualBox includes */
#include <iprt/semaphore.h>
#include <iprt/ldr.h>
#include <iprt/uint128.h>
#include <VBox/err.h>


struct Attached_gip : Genode::Attached_ram_dataspace
{
	Attached_gip()
	: Attached_ram_dataspace(genode_env().ram(), genode_env().rm(), PAGE_SIZE)
	{ }
};


enum {
	UPDATE_HZ  = 1000,
	UPDATE_MS  = 1000 / UPDATE_HZ,
	UPDATE_NS  = UPDATE_MS * 1000 * 1000,
};


PSUPGLOBALINFOPAGE g_pSUPGlobalInfoPage;


struct Periodic_gip : public Genode::Thread
{
	Periodic_gip(Genode::Env &env) : Thread(env, "periodic_gip", 8192) { start(); }

	static void update()
	{
		/**
		 * We're using rdtsc here since timer_session->elapsed_ms produces
		 * instable results when the timer service is using the Genode PIC
		 * driver as done for base-nova currently.
		 */

		Genode::uint64_t tsc_current = Genode::Trace::timestamp();

		/*
		 * Convert tsc to nanoseconds.
		 *
		 * There is no 'uint128_t' type on x86_32, so we use the 128-bit type
		 * and functions provided by VirtualBox.
		 *
		 * nanots128 = tsc_current * 1000*1000*1000 / genode_cpu_hz()
		 *
		 */

		RTUINT128U nanots128;
		RTUInt128AssignU64(&nanots128, tsc_current);

		RTUINT128U multiplier;
		RTUInt128AssignU32(&multiplier, 1000*1000*1000);
		RTUInt128AssignMul(&nanots128, &multiplier);

		RTUINT128U divisor;
		RTUInt128AssignU64(&divisor, genode_cpu_hz());
		RTUInt128AssignDiv(&nanots128, &divisor);

		SUPGIPCPU *cpu = &g_pSUPGlobalInfoPage->aCPUs[0];

		/*
		 * Transaction id must be incremented before and after update,
		 * read struct SUPGIPCPU description for more details.
		 */
		ASMAtomicIncU32(&cpu->u32TransactionId);

		cpu->u64TSC    = tsc_current;
		cpu->u64NanoTS = nanots128.s.Lo;

		/*
		 * Transaction id must be incremented before and after update,
		 * read struct SUPGIPCPU description for more details.
		 */
		ASMAtomicIncU32(&cpu->u32TransactionId);
	}

	void entry() override { genode_update_tsc(update, UPDATE_MS * 1000); }
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
	static Periodic_gip periodic_gip (genode_env());

	initialized = true;

	return VINF_SUCCESS;
}


int SUPR3Term(bool) { return VINF_SUCCESS; }


int SUPR3GipGetPhys(PRTHCPHYS pHCPhys)
{
	/*
	 * Return VMM-local address as physical address. This address is
	 * then fed to MMR3HyperMapHCPhys. (TMR3Init)
	 */
	*pHCPhys = (RTHCPHYS)g_pSUPGlobalInfoPage;

	return VINF_SUCCESS;
}


int SUPR3HardenedLdrLoadAppPriv(const char *pszFilename, PRTLDRMOD phLdrMod,
                               uint32_t fFlags, PRTERRINFO pErrInfo)
{
	return RTLdrLoad(pszFilename, phLdrMod);
}


uint32_t SUPSemEventMultiGetResolution(PSUPDRVSESSION)
{
	return 100000*10; /* called by 'vmR3HaltGlobal1Init' */
}


int SUPSemEventCreate(PSUPDRVSESSION pSession, PSUPSEMEVENT phEvent)
{
	return RTSemEventCreate((PRTSEMEVENT)phEvent);
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


int SUPSemEventWaitNoResume(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent,
                            uint32_t cMillies)
{
	Assert (hEvent);

	return RTSemEventWaitNoResume((RTSEMEVENT)hEvent, cMillies);
}


SUPDECL(int) SUPSemEventMultiCreate(PSUPDRVSESSION,
                                    PSUPSEMEVENTMULTI phEventMulti)
{
    RTSEMEVENTMULTI sem;

    /*
     * Input validation.
     */
    AssertPtrReturn(phEventMulti, VERR_INVALID_POINTER);

    /*
     * Create the event semaphore object.
     */
	int rc = RTSemEventMultiCreate(&sem);

	static_assert(sizeof(sem) == sizeof(*phEventMulti), "oi");
	*phEventMulti = reinterpret_cast<SUPSEMEVENTMULTI>(sem);
	return rc;
}


SUPDECL(int) SUPSemEventMultiClose(PSUPDRVSESSION, SUPSEMEVENTMULTI hEvMulti)
{
	return RTSemEventMultiDestroy(reinterpret_cast<RTSEMEVENTMULTI>(hEvMulti));
}


int SUPR3CallVMMR0(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation,
                   void *pvArg)
{
	if (uOperation == VMMR0_DO_CALL_HYPERVISOR) {
		Genode::log(__func__, ": VMMR0_DO_CALL_HYPERVISOR - doing nothing");
		return VINF_SUCCESS;
	}
	if (uOperation == VMMR0_DO_VMMR0_TERM) {
		Genode::log(__func__, ": VMMR0_DO_VMMR0_TERM - doing nothing");
		return VINF_SUCCESS;
	}
	if (uOperation == VMMR0_DO_GVMM_DESTROY_VM) {
		Genode::log(__func__, ": VMMR0_DO_GVMM_DESTROY_VM - doing nothing");
		return VINF_SUCCESS;
	}

	AssertMsg(uOperation != VMMR0_DO_VMMR0_TERM &&
	          uOperation != VMMR0_DO_CALL_HYPERVISOR &&
	          uOperation != VMMR0_DO_GVMM_DESTROY_VM,
	          ("SUPR3CallVMMR0: unhandled uOperation %d", uOperation));
	return VERR_GENERAL_FAILURE;
}


void genode_VMMR0_DO_GVMM_CREATE_VM(PSUPVMMR0REQHDR pReqHdr)
{
	GVMMCREATEVMREQ &req = reinterpret_cast<GVMMCREATEVMREQ &>(*pReqHdr);

	size_t const cCpus = req.cCpus;

	/*
	 * Allocate and initialize VM struct
	 *
	 * The VM struct is followed by the variable-sizedA array of VMCPU
	 * objects. 'RT_UOFFSETOF' is used to determine the size including
	 * the VMCPU array.
	 *
	 * VM struct must be page-aligned, which is checked at least in
	 * PDMR3CritSectGetNop().
	 */
	size_t const cbVM = RT_UOFFSETOF(VM, aCpus[cCpus]);

	static Genode::Attached_ram_dataspace vm(genode_env().ram(),
	                                         genode_env().rm(),
	                                         cbVM);
	Assert (vm.size() >= cbVM);

	VM *pVM = vm.local_addr<VM>();
	Genode::memset(pVM, 0, cbVM);

	/*
	 * On Genode, VMMR0 and VMMR3 share a single address space. Hence, the
	 * same pVM pointer is valid as pVMR0 and pVMR3.
	 */
	pVM->enmVMState       = VMSTATE_CREATING;
	pVM->pVMR0            = (RTHCUINTPTR)pVM;
	pVM->pVMRC            = (RTGCUINTPTR)pVM;
	pVM->pSession         = req.pSession;
	pVM->cbSelf           = cbVM;
	pVM->cCpus            = cCpus;
	pVM->uCpuExecutionCap = 100;  /* expected by 'vmR3CreateU()' */
	pVM->offVMCPU         = RT_UOFFSETOF(VM, aCpus);

	for (uint32_t i = 0; i < cCpus; i++) {
		pVM->aCpus[i].pVMR0           = pVM->pVMR0;
		pVM->aCpus[i].pVMR3           = pVM;
		pVM->aCpus[i].idHostCpu       = NIL_RTCPUID;
		pVM->aCpus[i].hNativeThreadR0 = NIL_RTNATIVETHREAD;
	}

	pVM->aCpus[0].hNativeThreadR0 = RTThreadNativeSelf();

	/* out parameters of the request */
	req.pVMR0 = pVM->pVMR0;
	req.pVMR3 = pVM;
}


void genode_VMMR0_DO_GVMM_REGISTER_VMCPU(PVMR0 pVMR0, VMCPUID idCpu)
{
	PVM pVM = reinterpret_cast<PVM>(pVMR0);
	pVM->aCpus[idCpu].hNativeThreadR0 = RTThreadNativeSelf();
}


HRESULT genode_check_memory_config(ComObjPtr<Machine> machine)
{
	HRESULT rc;

	/* Validate configured memory of vbox file and Genode config */
	ULONG memory_vbox;
	rc = machine->COMGETTER(MemorySize)(&memory_vbox);
	if (FAILED(rc))
		return rc;

	/* Request max available memory */
	size_t memory_genode = genode_env().ram().avail() >> 20;
	size_t memory_vmm    = 28;

	if (memory_vbox + memory_vmm > memory_genode) {
		using Genode::error;
		error("Configured memory ", memory_vmm, " MB (vbox file) is insufficient.");
		error(memory_genode,              " MB (1) - ",
		      memory_vmm,                 " MB (2) = ",
		      memory_genode - memory_vmm, " MB (3)");
		error("(1) available memory based defined by Genode config");
		error("(2) minimum memory required for VBox VMM");
		error("(3) maximal available memory to VM");
		return E_FAIL;
	}
	return S_OK;
}
