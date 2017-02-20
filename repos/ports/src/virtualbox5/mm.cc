/*
 * \brief  VirtualBox memory manager (MMR3)
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
#include <rm_session/connection.h>

#include <base/attached_ram_dataspace.h>

/* VirtualBox includes */
#include <VBox/vmm/mm.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>
#include <VBox/vmm/gmm.h>
#include "MMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/pgm.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>

/* libc memory allocator */
#include <libc_mem_alloc.h>

#include "util.h"
#include "mm.h"
#include "vmm.h"

enum { VERBOSE_MM = false };

static struct {
	Sub_rm_connection * conn;
	Libc::Mem_alloc_impl  * heap;
} memory_regions [MM_TAG_HM + 1];


static Libc::Mem_alloc * heap_by_mmtag(MMTAG enmTag)
{
	enum { REGION_SIZE = 4096 * 4096 };
	static Genode::Lock memory_init_lock;

	Assert(enmTag < sizeof(memory_regions) / sizeof(memory_regions[0]));

	if (memory_regions[enmTag].conn)
		return memory_regions[enmTag].heap;

	Genode::Lock::Guard guard(memory_init_lock);

	if (memory_regions[enmTag].conn)
		return memory_regions[enmTag].heap;

	memory_regions[enmTag].conn = new Sub_rm_connection(genode_env(), REGION_SIZE);
	memory_regions[enmTag].heap = new Libc::Mem_alloc_impl(*memory_regions[enmTag].conn,
	                                                       genode_env().ram());

	return memory_regions[enmTag].heap;
}


static Libc::Mem_alloc * heap_by_pointer(void * pv)
{
	for (unsigned i = 0; i < sizeof(memory_regions) / sizeof(memory_regions[0]); i++) {
		if (!memory_regions[i].heap)
			continue;

		if (memory_regions[i].conn->contains(pv))
			return memory_regions[i].heap;
	}

	return nullptr;
}


int MMR3Init(PVM) { return VINF_SUCCESS; }
int MMR3Term(PVM) { return VINF_SUCCESS; }
int MMR3InitUVM(PUVM) { return VINF_SUCCESS; }
void MMR3TermUVM(PUVM) { }


void *MMR3HeapAllocU(PUVM pUVM, MMTAG enmTag, size_t cbSize)
{
	return heap_by_mmtag(enmTag)->alloc(cbSize, Genode::log2(RTMEM_ALIGNMENT));
}


/**
 * Return alignment to be used for allocations of given tag
 */
static unsigned align_by_mmtag(MMTAG enmTag)
{
	switch (enmTag) {
	case MM_TAG_PGM:
	case MM_TAG_PDM_DEVICE:
	case MM_TAG_PDM_DEVICE_USER:
	case MM_TAG_VMM:
	case MM_TAG_CPUM_CTX:
		return 12;
	case MM_TAG_CPUM_CPUID:
    case MM_TAG_CPUM_MSRS:
		return Genode::log2(32);
	case MM_TAG_PGM_PHYS:
		return Genode::log2(16);
	default:
		return Genode::log2(RTMEM_ALIGNMENT);
	}
}


/**
 * Round allocation size for a given tag
 */
static size_t round_size_by_mmtag(MMTAG enmTag, size_t cb)
{
	return Genode::align_addr(cb, align_by_mmtag(enmTag));
}


void *MMR3HeapAlloc(PVM pVM, MMTAG enmTag, size_t cbSize)
{
	size_t const rounded_size = round_size_by_mmtag(enmTag, cbSize);
	return heap_by_mmtag(enmTag)->alloc(rounded_size, align_by_mmtag(enmTag));
}

void *MMR3HeapAllocZ(PVM pVM, MMTAG enmTag, size_t cbSize)
{
	void * const ret = MMR3HeapAlloc(pVM, enmTag, cbSize);

	if (ret)
		Genode::memset(ret, 0, cbSize);

	return ret;
}

void * MMR3HeapAllocZU(PUVM pUVM, MMTAG enmTag, size_t cbSize) {
	void * const ret = MMR3HeapAllocU(pUVM, enmTag, cbSize);

	if (ret)
		Genode::memset(ret, 0, cbSize);

	return ret;
}

void * MMR3UkHeapAllocZ(PVM pVM, MMTAG enmTag, size_t cbSize, PRTR0PTR pR0Ptr)
{
	if (pR0Ptr)
		*pR0Ptr = NIL_RTR0PTR;
	return MMR3HeapAllocZ(pVM, enmTag, cbSize);
}

int MMR3HeapAllocZEx(PVM pVM, MMTAG enmTag, size_t cbSize, void **ppv)
{
	*ppv = MMR3HeapAllocZ(pVM, enmTag, cbSize);

	return VINF_SUCCESS;
}


int MMR3HyperInitFinalize(PVM)
{
	Genode::log(__func__, " called");
	return VINF_SUCCESS;
}


int MMR3HyperSetGuard(PVM, void* ptr, size_t, bool)
{
	Genode::log(__func__, " called ", ptr);
	return VINF_SUCCESS;
}


int MMR3HyperAllocOnceNoRel(PVM pVM, size_t cb, unsigned uAlignment,
                            MMTAG enmTag, void **ppv)
{
	AssertRelease(align_by_mmtag(enmTag) >= (uAlignment ? Genode::log2(uAlignment) : 0));

	unsigned const align_log2 = uAlignment ? Genode::log2(uAlignment)
	                                       : align_by_mmtag(enmTag);

	size_t const rounded_size = round_size_by_mmtag(enmTag, cb);

	void *ret = heap_by_mmtag(enmTag)->alloc(rounded_size, align_log2);
	if (ret)
		Genode::memset(ret, 0, cb);

	*ppv = ret;

	return VINF_SUCCESS;
}


int MMR3HyperAllocOnceNoRelEx(PVM pVM, size_t cb, uint32_t uAlignment,
                              MMTAG enmTag, uint32_t fFlags, void **ppv)
{
	AssertRelease(align_by_mmtag(enmTag) >= (uAlignment ? Genode::log2(uAlignment) : 0));

	return MMR3HyperAllocOnceNoRel(pVM, cb, uAlignment, enmTag, ppv);
}


int MMHyperAlloc(PVM pVM, size_t cb, unsigned uAlignment, MMTAG enmTag, void **ppv)
{
	
	if (!(align_by_mmtag(enmTag) >= (uAlignment ? Genode::log2(uAlignment) : 0)))
		Genode::error(__func__, " ", (int)enmTag, " ", uAlignment, " ", (int)MM_TAG_PGM);
	AssertRelease(align_by_mmtag(enmTag) >= (uAlignment ? Genode::log2(uAlignment) : 0));

	*ppv = MMR3HeapAllocZ(pVM, enmTag, cb);

	return VINF_SUCCESS;
}


int MMHyperFree(PVM pVM, void *pv)
{
	MMR3HeapFree(pv);
	return VINF_SUCCESS;
}


int MMHyperDupMem(PVM pVM, const void *pvSrc, size_t cb,
                  unsigned uAlignment, MMTAG enmTag, void **ppv)
{
    int rc = MMHyperAlloc(pVM, cb, uAlignment, enmTag, ppv);
    if (RT_SUCCESS(rc))
        memcpy(*ppv, pvSrc, cb);

    return rc;
}

bool MMHyperIsInsideArea(PVM, RTGCPTR ptr)
{
	Genode::log(__func__, " called");

	return false;
}

void MMR3HeapFree(void *pv)
{
	Libc::Mem_alloc *heap = heap_by_pointer(pv);

	Assert(heap);

	heap->free(pv);
}


uint64_t MMR3PhysGetRamSize(PVM pVM)
{
	/* when called from REMR3Init, it is expected to return 0 */
	return 0;
}


int MMR3HyperMapHCPhys(PVM pVM, void *pvR3, RTR0PTR pvR0, RTHCPHYS HCPhys,
                       size_t cb, const char *pszDesc, PRTGCPTR pGCPtr)
{
	static_assert(sizeof(*pGCPtr) == sizeof(HCPhys) , "pointer transformation bug");
	*pGCPtr = (RTGCPTR)HCPhys;

	return VINF_SUCCESS;
}


int MMR3HyperReserve(PVM pVM, unsigned cb, const char *pszDesc, PRTGCPTR pGCPtr)
{
	if (VERBOSE_MM)
		Genode::log("MMR3HyperReserve: cb=", Genode::Hex(cb), ", "
		            "pszDesc=", pszDesc);

	return VINF_SUCCESS;
}


VMMR3DECL(int) MMR3IncreaseBaseReservation(PVM pVM, uint64_t cAddBasePages)
{
	return VINF_SUCCESS;
}


int MMR3AdjustFixedReservation(PVM, int32_t, const char *pszDesc)
{
	if (VERBOSE_MM)
		Genode::log(__func__, " called for '", pszDesc, "'");
	return VINF_SUCCESS;
}


int MMR3HyperMapMMIO2(PVM pVM, PPDMDEVINS pDevIns, uint32_t iSubDev,
                      uint32_t iRegion,
                      RTGCPHYS off, RTGCPHYS cb, const char *pszDesc,
                      PRTRCPTR pRCPtr)
{
	if (VERBOSE_MM)
		Genode::log("pszDesc=", pszDesc, " iRegion=", iRegion, " "
		            "off=", Genode::Hex(off), " cb=", Genode::Hex(cb));

	return VINF_SUCCESS;
}


/*
 * Based on 'VBox/VMM/VMMR3/MM.cpp'
 */
int MMR3InitPaging(PVM pVM)
{
    /*
     * Query the CFGM values.
     */
    int rc;
    PCFGMNODE pMMCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "MM");
    if (!pMMCfg)
    {
        rc = CFGMR3InsertNode(CFGMR3GetRoot(pVM), "MM", &pMMCfg);
        AssertRCReturn(rc, rc);
    }

    /** @cfgm{RamSize, uint64_t, 0, 16TB, 0}
     * Specifies the size of the base RAM that is to be set up during
     * VM initialization.
     */
    uint64_t cbRam;
    rc = CFGMR3QueryU64(CFGMR3GetRoot(pVM), "RamSize", &cbRam);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cbRam = 0;
    else
        AssertMsgRCReturn(rc, ("Configuration error: Failed to query integer \"RamSize\", rc=%Rrc.\n", rc), rc);
    cbRam &= X86_PTE_PAE_PG_MASK;

    /** @cfgm{RamHoleSize, uint32_t, 0, 4032MB, 512MB}
     * Specifies the size of the memory hole. The memory hole is used
     * to avoid mapping RAM to the range normally used for PCI memory regions.
     * Must be aligned on a 4MB boundary. */
    uint32_t cbRamHole;
    rc = CFGMR3QueryU32Def(CFGMR3GetRoot(pVM), "RamHoleSize", &cbRamHole, MM_RAM_HOLE_SIZE_DEFAULT);
    uint64_t const offRamHole = _4G - cbRamHole;

    /*
     * Make the initial memory reservation with GMM.
     */
    LogFlow(("GMMR3INitialReservation missing\n"));

    /*
     * If RamSize is 0 we're done now.
     */
    if (cbRam < PAGE_SIZE)
    {
        Log(("MM: No RAM configured\n"));
        return VINF_SUCCESS;
    }

    /*
     * Setup the base ram (PGM).
     */
    if (cbRam > offRamHole)
    {
        pVM->mm.s.cbRamBelow4GB = offRamHole;
        rc = PGMR3PhysRegisterRam(pVM, 0, offRamHole, "Base RAM");
        if (RT_SUCCESS(rc))
        {
            pVM->mm.s.cbRamAbove4GB = cbRam - offRamHole;
            rc = PGMR3PhysRegisterRam(pVM, _4G, cbRam - offRamHole, "Above 4GB Base RAM");
        }
    } else {
        pVM->mm.s.cbRamBelow4GB = cbRam;
        pVM->mm.s.cbRamAbove4GB = 0;
        rc = PGMR3PhysRegisterRam(pVM, 0, RT_MIN(cbRam, offRamHole), "Base RAM");
    }

    LogFlow(("MMR3InitPaging: returns %Rrc\n", rc));
    return rc;
}


char * MMR3HeapStrDupU(PUVM pUVM, MMTAG enmTag, const char *string)
{
	if (!string)
		return NULL;

	size_t len = strlen(string) + 1;
	char *dup = reinterpret_cast<char *>(MMR3HeapAllocU(pUVM, enmTag, len));
	if (dup)
		memcpy(dup, string, len);

	return dup;
}


char * MMR3HeapStrDup(PVM pVM, MMTAG enmTag, const char *string)
{
	Assert(pVM);
	Assert(pVM->pUVM);
	return MMR3HeapStrDupU(pVM->pUVM, enmTag, string);
}


char * MMR3HeapAPrintfVU(PUVM pUVM, MMTAG enmTag, const char *pszFormat, va_list va)
{
    /*
     * The lazy bird way.
     */
    char *psz;
    int cch = RTStrAPrintfV(&psz, pszFormat, va);
    if (cch < 0)
        return NULL;
    Assert(psz[cch] == '\0');
    char *pszRet = (char *)MMR3HeapAllocU(pUVM, enmTag, cch + 1);
    if (pszRet)
        memcpy(pszRet, psz, cch + 1);
    RTStrFree(psz);
	Genode::log(__func__, " called ", Genode::Cstring(pszRet), " ", Genode::Cstring(pszFormat));
    return pszRet;
}


VMMR3DECL(RTHCPHYS) MMR3HyperHCVirt2HCPhys(PVM pVM, void *pvR3) {
	return (RTHCPHYS)(uintptr_t)pvR3; }


VMMDECL(RTHCPHYS) MMPage2Phys(PVM pVM, void *pvPage) {
	return (RTHCPHYS)(uintptr_t)pvPage; }


VMMR3DECL(void *) MMR3PageAlloc(PVM pVM)
{
	using Genode::Attached_ram_dataspace;
	Attached_ram_dataspace * ds = new Attached_ram_dataspace(genode_env().ram(),
	                                                         genode_env().rm(),
	                                                         4096);
	return ds->local_addr<void>();
}


VMMR3DECL(void *) MMR3PageAllocLow(PVM pVM) { return MMR3PageAlloc(pVM); }

int MMR3ReserveHandyPages(PVM pVM, uint32_t cHandyPages)
{
	Genode::log(__func__, " called");
	return VINF_SUCCESS;
}


VMMDECL(void *) MMHyperHeapOffsetToPtr(PVM pVM, uint32_t offHeap)
{
	if (sizeof(void*) == 8) {
		uint64_t ptr = offHeap;
		return reinterpret_cast<void *>(ptr);
	}

	return reinterpret_cast<void *>(offHeap);
}


VMMDECL(uint32_t) MMHyperHeapPtrToOffset(PVM pVM, void *pv)
{
	Genode::addr_t offset = reinterpret_cast<Genode::addr_t>(pv);

	Assert (reinterpret_cast<void *>(offset) == pv);

	return offset;
}


VMMR3DECL(uint32_t) MMR3PhysGetRamSizeBelow4GB(PVM pVM)
{
	VM_ASSERT_VALID_EXT_RETURN(pVM, UINT32_MAX);
	return pVM->mm.s.cbRamBelow4GB;
}


VMMR3DECL(uint64_t) MMR3PhysGetRamSizeAbove4GB(PVM pVM)
{
	VM_ASSERT_VALID_EXT_RETURN(pVM, UINT64_MAX);
	return pVM->mm.s.cbRamAbove4GB;
}


extern "C" {

char * MMR3HeapAPrintf(PVM pVM, MMTAG enmTag, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    char *psz = MMR3HeapAPrintfVU(pVM->pUVM, enmTag, pszFormat, va);
    va_end(va);
    return psz;
}

}
