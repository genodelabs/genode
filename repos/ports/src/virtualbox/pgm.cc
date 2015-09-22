/*
 * \brief  VirtualBox page manager (PGM)
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
#include <base/printf.h>
#include <util/string.h>

#include <vmm/printf.h>

/* VirtualBox includes */
#include "PGMInternal.h" /* enable access to pgm.s.* */
#include "EMInternal.h"

#include <VBox/vmm/mm.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/rem.h>
#include <iprt/err.h>

/* local includes */
#include "util.h"
#include "vmm_memory.h"
#include "guest_memory.h"

using Genode::Ram_session;
using Genode::Rm_session;

static bool verbose = false;
static bool verbose_debug = false;

Vmm_memory *vmm_memory()
{
	static Vmm_memory inst(*Genode::env()->ram_session());
	return &inst;
}


Guest_memory *guest_memory()
{
	static Guest_memory inst;
	return &inst;
}


static DECLCALLBACK(int) romwritehandler(PVM pVM, RTGCPHYS GCPhys,
                                         void *pvPhys, void *pvBuf,
                                         size_t cbBuf,
                                         PGMACCESSTYPE enmAccessType,
                                         void *pvUser)
{
	Assert(!"Somebody tries to write to ROM");
	return -1;
}

int PGMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys,
                         RTGCPHYS cb, const void *pvBinary, uint32_t cbBinary,
                         uint32_t fFlags, const char *pszDesc)
{
	if (verbose)
		PLOG("%s: GCPhys=0x%llx cb=0x%llx pvBinary=0x%p - '%s'", __func__,
		     (Genode::uint64_t)GCPhys, (Genode::uint64_t)cb, pvBinary, pszDesc);

	try {
		RTGCPHYS GCPhysLast = GCPhys + (cb - 1);

		size_t size = (size_t)cb;
		Assert(cb == size);

		void *pv = vmm_memory()->alloc_rom(size, pDevIns);
		Assert(pv);
		memcpy(pv, pvBinary, size);

		/* associate memory of VMM with guest VM */
		vmm_memory()->map_to_vm(pDevIns, GCPhys);

		guest_memory()->add_rom_mapping(GCPhys, cb, pv, pDevIns);

		bool fShadowed = fFlags & PGMPHYS_ROM_FLAGS_SHADOWED;
		Assert(!fShadowed);

		int rc = PGMR3HandlerPhysicalRegister(pVM,
		                                      PGMPHYSHANDLERTYPE_PHYSICAL_WRITE,
		                                      GCPhys, GCPhysLast,
		                                      romwritehandler,
		                                      NULL,
		                                      NULL, NULL, 0,
		                                      NULL, NULL, 0, pszDesc);
		Assert(rc == VINF_SUCCESS);

#ifdef VBOX_WITH_REM
		REMR3NotifyPhysRomRegister(pVM, GCPhys, cb, NULL, fShadowed);
#endif

	}
	catch (Guest_memory::Region_conflict) { return VERR_PGM_MAPPING_CONFLICT; }
	catch (Ram_session::Alloc_failed) { return VERR_PGM_MAPPING_CONFLICT; }
	catch (Rm_session::Attach_failed) { return VERR_PGM_MAPPING_CONFLICT; }

	return VINF_SUCCESS;
}


int PGMPhysWrite(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
	void *pv = guest_memory()->lookup(GCPhys, cbWrite);

	if (verbose_debug)
		PDBG("%s: GCPhys=0x%llx pvBuf=0x%p cb=0x%zx pv=%p", __func__,
		     (Genode::uint64_t)GCPhys, pvBuf, cbWrite, pv);

	if (pv) {
		void * pvx = vmm_memory()->lookup(GCPhys, cbWrite);
		Assert(!pvx);

		memcpy(pv, pvBuf, cbWrite);
		return VINF_SUCCESS;
	}

	PFNPGMR3PHYSHANDLER  pfnHandlerR3 = 0;
	void                *pvUserR3     = 0;

	pv = vmm_memory()->lookup(GCPhys, cbWrite, &pfnHandlerR3, &pvUserR3);

	if (!pv || !pfnHandlerR3 || !pvUserR3) {
		PERR("%s skipped: GCPhys=0x%llx pvBuf=0x%p cbWrite=0x%zx", __func__,
		     (Genode::uint64_t)GCPhys, pvBuf, cbWrite);
		return VERR_GENERAL_FAILURE;
	}

	int rc = pfnHandlerR3(pVM, GCPhys, 0, 0, cbWrite, PGMACCESSTYPE_WRITE,
	                      pvUserR3);

	if (rc != VINF_PGM_HANDLER_DO_DEFAULT) {
		PERR("unexpected %s return code %d", __FUNCTION__, rc);
		return VERR_GENERAL_FAILURE;
	}

	memcpy(pv, pvBuf, cbWrite);
	return VINF_SUCCESS;
}


int PGMR3PhysWriteExternal(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf,
                           size_t cbWrite, const char *pszWho)
{
	VM_ASSERT_OTHER_THREAD(pVM);

	return PGMPhysWrite(pVM, GCPhys, pvBuf, cbWrite);
}


int PGMPhysRead(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
	void *pv = guest_memory()->lookup(GCPhys, cbRead);

	if (verbose_debug)
		PDBG("%s: GCPhys=0x%llx pvBuf=0x%p cbRead=0x%zx pv=%p", __func__,
		     (Genode::uint64_t)GCPhys, pvBuf, cbRead, pv);

	if (pv) {
		void * pvx = vmm_memory()->lookup(GCPhys, cbRead);
		Assert(!pvx);
		memcpy(pvBuf, pv, cbRead);
		return VINF_SUCCESS;
	}

	PFNPGMR3PHYSHANDLER  pfnHandlerR3 = 0;
	void                *pvUserR3     = 0;

	pv = vmm_memory()->lookup(GCPhys, cbRead, &pfnHandlerR3, &pvUserR3);
	if (!pv || !pfnHandlerR3 || !pvUserR3) {
		PERR("PGMPhysRead skipped: GCPhys=0x%llx pvBuf=0x%p cbRead=0x%zx",
		     (Genode::uint64_t)GCPhys, pvBuf, cbRead);
		return VERR_GENERAL_FAILURE;
	}

	memcpy(pvBuf, pv, cbRead);
	return VINF_SUCCESS;
}


int PGMR3PhysReadExternal(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
	VM_ASSERT_OTHER_THREAD(pVM);

	return PGMPhysRead(pVM, GCPhys, pvBuf, cbRead);
}


int PGMR3PhysMMIO2Register(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion,
                           RTGCPHYS cb, uint32_t fFlags, void **ppv, const char
                           *pszDesc)
{
	*ppv = vmm_memory()->alloc((size_t)cb, pDevIns, iRegion);

	if (verbose)
		PLOG("PGMR3PhysMMIO2Register: pszDesc=%s iRegion=%u cb=0x%zx -> 0x%p",
		     pszDesc, iRegion, (size_t)cb, *ppv);

	return VINF_SUCCESS;
}


int PGMR3PhysMMIO2Deregister(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion)
{
	PERR("%s: pDevIns %p iRegion=%x", __func__, pDevIns, iRegion);
	return VINF_SUCCESS;
}


int PGMR3PhysMMIO2Map(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion,
                      RTGCPHYS GCPhys)
{
	size_t cb = vmm_memory()->map_to_vm(pDevIns, GCPhys, iRegion);
	if (cb == 0) {
		PERR("%s: lookup for pDevIns=%p iRegion=%u failed\n", __func__,
		     pDevIns, iRegion);
		Assert(cb);
	}

	if (verbose)
		PLOG("%s: pDevIns=%p iRegion=%u cb=0x%zx GCPhys=0x%llx\n", __func__,
		     pDevIns, iRegion, cb, (Genode::uint64_t)GCPhys);

#ifdef VBOX_WITH_REM
	REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, REM_NOTIFY_PHYS_RAM_FLAGS_MMIO2);
#endif

	return VINF_SUCCESS;
}


int PGMR3PhysMMIO2Unmap(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion,
                        RTGCPHYS GCPhys)
{
	if (verbose_debug)
		PDBG("called phys=%llx iRegion=0x%x", (Genode::uint64_t)GCPhys,
		     iRegion);

	RTGCPHYS GCPhysStart = GCPhys;
	size_t size = 1;
	bool io = vmm_memory()->lookup_range(GCPhysStart, size);
	Assert(io);
	Assert(GCPhysStart == GCPhys);

	bool INVALIDATE = true;
	bool ok = vmm_memory()->unmap_from_vm(GCPhys, size, INVALIDATE);
	Assert(ok);

#ifdef VBOX_WITH_REM
	REMR3NotifyPhysRamDeregister(pVM, GCPhysStart, size);
#endif
	return VINF_SUCCESS;
}


bool PGMR3PhysMMIO2IsBase(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys)
{
	bool res = vmm_memory()->lookup(GCPhys, 1);
	if (verbose_debug)
		PDBG("called phys=%llx res=%u", (Genode::uint64_t)GCPhys, res);
	return res;
}

	
int PGMR3HandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType,
                                 RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                 PFNPGMR3PHYSHANDLER pfnHandlerR3,
                                 void *pvUserR3, const char *pszModR0,
                                 const char *pszHandlerR0, RTR0PTR pvUserR0,
                                 const char *pszModRC,
                                 const char *pszHandlerRC,
                                 RTRCPTR pvUserRC, const char *pszDesc)
{
	if (verbose)
		PLOG("%s: GCPhys=0x%llx-%llx r3=0x%p enmType=%x - '%s'\n", __func__,
		     (Genode::uint64_t)GCPhys, (Genode::uint64_t)GCPhysLast,
		     pfnHandlerR3, enmType, pszDesc);

	bool ok = vmm_memory()->add_handler(GCPhys, GCPhysLast - GCPhys + 1,
	                                    pfnHandlerR3, pvUserR3, enmType);
	Assert(ok);

#ifdef VBOX_WITH_REM
	REMR3NotifyHandlerPhysicalRegister(pVM, enmType, GCPhys, GCPhysLast -
	                                   GCPhys + 1, !!pfnHandlerR3);
#endif

	return VINF_SUCCESS;
}


int PGMHandlerPhysicalDeregister(PVM pVM, RTGCPHYS GCPhys)
{
	size_t size = 1;

#ifdef VBOX_WITH_REM
	PFNPGMR3PHYSHANDLER pfnHandlerR3 = 0;
	PGMPHYSHANDLERTYPE enmType = PGMPHYSHANDLERTYPE_MMIO;

	void * pv = vmm_memory()->lookup(GCPhys, size, &pfnHandlerR3, 0, &enmType);
	Assert(pv);

	if (verbose_debug)
		PDBG("called phys=%llx enmType=%x", (Genode::uint64_t)GCPhys, enmType);

#endif

	bool ok = vmm_memory()->add_handler(GCPhys, size, 0, 0);
	Assert(ok);

#ifdef VBOX_WITH_REM
	bool fRestoreAsRAM = pfnHandlerR3 && enmType != PGMPHYSHANDLERTYPE_MMIO;

	/* GCPhysstart and size gets written ! */
	RTGCPHYS GCPhysStart = GCPhys;
	bool io = vmm_memory()->lookup_range(GCPhysStart, size);
	Assert(io);

	REMR3NotifyHandlerPhysicalDeregister(pVM, enmType, GCPhysStart, size,
	                                     !!pfnHandlerR3, fRestoreAsRAM);
#endif

	return VINF_SUCCESS;
}


int PGMR3PhysRegisterRam(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb,
                         const char *pszDesc)
{
	if (verbose)
		PLOG("PGMR3PhysRegisterRam: GCPhys=0x%llx, cb=0x%llx, pszDesc=%s",
		     (Genode::uint64_t)GCPhys, (Genode::uint64_t)cb, pszDesc);

	try {

		/*
		 * XXX Is this function the right place for the allocation?
		 *     The lack of allocation-related VERR_PGM_ error codes suggests
		 *     so.
		 */
		size_t size = (size_t)cb;
		Assert(cb == size);
		void *pv = vmm_memory()->alloc_ram(size);

		guest_memory()->add_ram_mapping(GCPhys, cb, pv);

#ifdef VBOX_WITH_REM
		REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, REM_NOTIFY_PHYS_RAM_FLAGS_RAM);
#endif
	}
	catch (Guest_memory::Region_conflict) {
		return VERR_PGM_MAPPING_CONFLICT; }
	catch (Ram_session::Alloc_failed) {
		return VERR_PGM_MAPPING_CONFLICT; /* XXX use a better error code? */ }
	catch (Rm_session::Attach_failed) {
		return VERR_PGM_MAPPING_CONFLICT; /* XXX use a better error code? */ }

	return VINF_SUCCESS;
}


int PGMMapSetPage(PVM pVM, RTGCPTR GCPtr, uint64_t cb, uint64_t fFlags)
{
	if (verbose)
		PLOG("%s: GCPtr=0x%llx cb=0x%llx, flags=0x%llx", __func__,
		     (Genode::uint64_t)GCPtr, (Genode::uint64_t)cb,
		     (Genode::uint64_t)fFlags);

	return VINF_SUCCESS;
}


RTHCPHYS PGMGetHyperCR3(PVMCPU pVCpu)
{
//	PDBG("%s %lx", __func__, CPUMGetHyperCR3(pVCpu));
	return 1;
}


int PGMR3Init(PVM pVM)
{
	/*
	 * Satisfy assertion in VMMR3Init. Normally called via:
	 *
	 * PGMR3Init -> pgmR3InitPaging -> pgmR3ModeDataInit -> InitData -> MapCR3
	 */
	for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++) {
		PVMCPU pVCpu = &pVM->aCpus[idCpu];
		CPUMSetHyperCR3(pVCpu, PGMGetHyperCR3(pVCpu));

		pVCpu->pgm.s.fA20Enabled      = true;
		pVCpu->pgm.s.GCPhysA20Mask    = ~((RTGCPHYS)!pVCpu->pgm.s.fA20Enabled << 20);
	}

    int rc = PDMR3CritSectInit(pVM, &pVM->pgm.s.CritSectX, RT_SRC_POS, "PGM");
    AssertRCReturn(rc, rc);

	return VINF_SUCCESS;
}


int PGMR3Term(PVM pVM)
{
	if (verbose)
		PDBG("called");

	return VINF_SUCCESS;
}


int PGMPhysGCPtr2CCPtrReadOnly(PVMCPU pVCpu, RTGCPTR GCPtr, void const **ppv,
                           PPGMPAGEMAPLOCK pLock)
{
	PERR("%s not implemented - caller 0x%p",
	     __func__, __builtin_return_address(0));

	Assert(!"not implemented");
	return VERR_GENERAL_FAILURE;
}


int PGMR3PhysTlbGCPhys2Ptr(PVM pVM, RTGCPHYS GCPhys, bool fWritable, void **ppv)
{
	size_t const         size         = 1;
	PFNPGMR3PHYSHANDLER  pfnHandlerR3 = 0;
	void                *pvUserR3     = 0;
	PGMPHYSHANDLERTYPE   enmType;

	void * pv = vmm_memory()->lookup(GCPhys, size, &pfnHandlerR3, &pvUserR3,
	                                 &enmType);
	if (!pv) {
		/* It could be ordinary guest memory - look it up. */
		pv = guest_memory()->lookup(GCPhys, size);

		if (!pv) {
			PERR("%s: lookup for GCPhys=0x%llx failed", __func__,
			    (Genode::uint64_t)GCPhys);
			return VERR_PGM_PHYS_TLB_UNASSIGNED;
		}

		*ppv = pv;

		if (verbose_debug)
			PDBG("%s: %llx %u -> 0x%p", __func__,
			     (Genode::uint64_t)GCPhys, fWritable, pv);

		return VINF_SUCCESS;
	}

	/* pv valid - check handlers next */
	if (!pfnHandlerR3 && !pvUserR3) {
		*ppv = pv;
		return VINF_SUCCESS;
	}

	if (enmType == PGMPHYSHANDLERTYPE_PHYSICAL_WRITE) {
		*ppv = pv;
		return VINF_PGM_PHYS_TLB_CATCH_WRITE;
	}
	PERR("%s: denied access - handlers set - GCPhys=0x%llx %p %p %x", __func__,
	     (Genode::uint64_t)GCPhys, pfnHandlerR3, pvUserR3, enmType);

	return VERR_PGM_PHYS_TLB_CATCH_ALL;
}


void PGMR3PhysSetA20(PVMCPU pVCpu, bool fEnable) 
{
	if (!pVCpu->pgm.s.fA20Enabled != fEnable) {
		pVCpu->pgm.s.fA20Enabled = fEnable;
#ifdef VBOX_WITH_REM
		REMR3A20Set(pVCpu->pVMR3, pVCpu, fEnable);
#endif
	}

	return;
}


bool PGMPhysIsA20Enabled(PVMCPU pVCpu)
{
	return pVCpu->pgm.s.fA20Enabled; 
}


template <typename T>
static void PGMR3PhysWrite(PVM pVM, RTGCPHYS GCPhys, T value)
{
	VM_ASSERT_EMT(pVM);

	void *pv = guest_memory()->lookup(GCPhys, sizeof(value));

	if (verbose_debug)
		PDBG("%s: GCPhys=0x%llx cb=0x%zx pv=%p", __func__,
		    (Genode::uint64_t)GCPhys, sizeof(value), pv);

	if (!pv) {
		PERR("%s: invalid write attempt phy=%llx", __func__,
		    (Genode::uint64_t)GCPhys);
		return;
	}

	/* sanity check */
	void * pvx = vmm_memory()->lookup(GCPhys, sizeof(value));
	Assert(!pvx);

	*reinterpret_cast<T *>(pv) = value;
}


void PGMR3PhysWriteU8(PVM pVM, RTGCPHYS GCPhys, uint8_t value)
{
	PGMR3PhysWrite(pVM, GCPhys, value);
}


void PGMR3PhysWriteU16(PVM pVM, RTGCPHYS GCPhys, uint16_t value)
{
	PGMR3PhysWrite(pVM, GCPhys, value);
}


void PGMR3PhysWriteU32(PVM pVM, RTGCPHYS GCPhys, uint32_t value)
{
	PGMR3PhysWrite(pVM, GCPhys, value);
}


template <typename T>
static T PGMR3PhysRead(PVM pVM, RTGCPHYS GCPhys)
{
	void *pv = guest_memory()->lookup(GCPhys, sizeof(T));

	if (verbose_debug)
		PDBG("%s: GCPhys=0x%llx cb=0x%zx pv=%p",
		     __func__, (Genode::uint64_t)GCPhys, sizeof(T), pv);

	if (!pv) {
		PERR("%s: invalid read attempt phys=%llx", __func__,
		     (Genode::uint64_t)GCPhys);
		return 0;
	}

	/* sanity check */
	void * pvx = vmm_memory()->lookup(GCPhys, sizeof(T));
	Assert(!pvx);

	return *reinterpret_cast<T *>(pv);
}


uint64_t PGMR3PhysReadU64(PVM pVM, RTGCPHYS GCPhys)
{
	return PGMR3PhysRead<uint64_t>(pVM, GCPhys);
}


uint32_t PGMR3PhysReadU32(PVM pVM, RTGCPHYS GCPhys)
{
	return PGMR3PhysRead<uint32_t>(pVM, GCPhys);
}


int PGMPhysGCPhys2CCPtrReadOnly(PVM pVM, RTGCPHYS GCPhys, void const **ppv,
                                PPGMPAGEMAPLOCK pLock)
{
	void *pv = guest_memory()->lookup(GCPhys, 0x1000);

	if (verbose_debug)
		PDBG("%s: GCPhys=0x%llx cb=0x%d pv=%p",
		     __func__, (Genode::uint64_t)GCPhys, 0x1000, pv);

	if (!pv) {
		PERR("unknown address pv=%p ppv=%p GCPhys=%llx", pv, ppv,
		     (Genode::uint64_t)GCPhys);

		guest_memory()->dump();

		return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
	}

	*ppv = pv;

	return VINF_SUCCESS;
}


int PGMHandlerPhysicalReset(PVM, RTGCPHYS GCPhys)
{
	size_t size = 1;
	if (!vmm_memory()->unmap_from_vm(GCPhys, size))
		PWRN("%s: unbacked region - GCPhys %llx", __func__,
		     (Genode::uint64_t)GCPhys);

	return VINF_SUCCESS;
}


extern "C" int MMIO2_MAPPED_SYNC(PVM pVM, RTGCPHYS GCPhys, size_t cbWrite,
                                 void **ppv, Genode::Flexpage_iterator &fli,
                                 bool &writeable)
{
	using Genode::Flexpage_iterator;
	using Genode::addr_t;

	/* DON'T USE normal printf in this function - corrupts unsaved UTCB !!! */

	PFNPGMR3PHYSHANDLER  pfnHandlerR3 = 0;
	void                *pvUserR3     = 0;

	void * pv = vmm_memory()->lookup(GCPhys, cbWrite, &pfnHandlerR3, &pvUserR3);

	if (!pv)
		return VERR_PGM_PHYS_TLB_UNASSIGNED;

	fli = Flexpage_iterator((addr_t)pv, cbWrite, GCPhys, cbWrite, GCPhys);

	if (!pfnHandlerR3 && !pvUserR3) {
		*ppv = pv;
//		Vmm::printf("------------------ %s: GCPhys=0x%llx vmm local %p io mem map - no handlers\n", __func__, GCPhys, pv);
//		return VERR_PGM_PHYS_TLB_UNASSIGNED;
		/* you may map it */
		return VINF_SUCCESS;
	}

	if (pfnHandlerR3 && pvUserR3) {
		int rc = pfnHandlerR3(pVM, GCPhys, 0, 0, cbWrite, PGMACCESSTYPE_WRITE,
		                      pvUserR3);

		if (rc == VINF_PGM_HANDLER_DO_DEFAULT) {
			*ppv = pv;
			/* you may map it */
			return VINF_SUCCESS;
		}
		
		Vmm::printf("%s: GCPhys=0x%llx failed - unexpected rc=%d\n",
		            __func__, (Genode::uint64_t)GCPhys, rc);
		return rc;
	}

	RTGCPHYS map_start = GCPhys;
	size_t map_size = 1;

	bool io = vmm_memory()->lookup_range(map_start, map_size);
	Assert(io);

	pv = vmm_memory()->lookup(map_start, map_size);
	Assert(pv);

	fli = Flexpage_iterator((addr_t)pv, map_size, map_start, map_size, map_start);
	if (verbose_debug)
		Vmm::printf("%s: GCPhys=0x%llx - %llx+%zx\n", __func__,
		            (Genode::uint64_t)GCPhys, (Genode::uint64_t)map_start,
		            map_size);

	*ppv = pv;

	writeable = false;

	return VINF_SUCCESS;
}


/**
 * Resets a virtual CPU when unplugged.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   pVCpu               Pointer to the VMCPU.
 */
VMMR3DECL(void) PGMR3ResetCpu(PVM pVM, PVMCPU pVCpu)
{
    int rc = PGMR3ChangeMode(pVM, pVCpu, PGMMODE_REAL);
    AssertRC(rc);

    /*
     * Re-init other members.
     */
    pVCpu->pgm.s.fA20Enabled = true;
    pVCpu->pgm.s.GCPhysA20Mask = ~((RTGCPHYS)!pVCpu->pgm.s.fA20Enabled << 20);

    /*
     * Clear the FFs PGM owns.
     */
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
}


void PGMR3Reset(PVM pVM)
{
	VM_ASSERT_EMT(pVM);

	for (VMCPUID i = 0; i < pVM->cCpus; i++)
	{
//		int rc = PGMR3ChangeMode(pVM, &pVM->aCpus[i], PGMMODE_REAL);
//		AssertRC(rc);
	}

	for (VMCPUID i = 0; i < pVM->cCpus; i++)
	{
		PVMCPU pVCpu = &pVM->aCpus[i];

		VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
		VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);

		VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);

		if (!pVCpu->pgm.s.fA20Enabled)
		{
			pVCpu->pgm.s.fA20Enabled = true;
			pVCpu->pgm.s.GCPhysA20Mask = ~((RTGCPHYS)!pVCpu->pgm.s.fA20Enabled << 20);
#ifdef PGM_WITH_A20
			pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
			VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
			HMFlushTLB(pVCpu);
#endif
		}
	}

	vmm_memory()->revoke_all();
}


int PGMR3MappingsSize(PVM pVM, uint32_t *pcb)
{
	if (verbose)
		PINF("%s - not implemented - %p", __func__,
		     __builtin_return_address(0));

	*pcb = 0;

	return 0;
}


void PGMR3MemSetup(PVM pVM, bool fAtReset)
{
	if (verbose)
		PDBG(" called");
}


VMMDECL(bool) PGMIsLockOwner(PVM pVM)
{
	return PDMCritSectIsOwner(&pVM->pgm.s.CritSectX);
}


VMM_INT_DECL(void) PGMNotifyNxeChanged(PVMCPU pVCpu, bool fNxe)
{
	if (verbose)
		PINF("%s - not implemented - %p", __func__,
		     __builtin_return_address(0));
}
