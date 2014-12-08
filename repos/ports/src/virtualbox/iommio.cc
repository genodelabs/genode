/*
 * \brief  VirtualBox Memory-mapped I/O monitor
 * \author Norman Feske
 * \date   2013-09-02
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* VirtualBox includes */
#include "IOMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/rem.h>

/* local includes */
#include "guest_memory.h"

static const bool verbose = false;

VMMR3_INT_DECL(int) IOMR3Init(PVM pVM)
{
	/*
	 * Assert alignment and sizes.
	 */
	AssertCompileMemberAlignment(VM, iom.s, 32);
	AssertCompile(sizeof(pVM->iom.s) <= sizeof(pVM->iom.padding));
	AssertCompileMemberAlignment(IOM, CritSect, sizeof(uintptr_t));

	/*
	 * Initialize the REM critical section.
	 */
#ifdef IOM_WITH_CRIT_SECT_RW
	int rc = PDMR3CritSectRwInit(pVM, &pVM->iom.s.CritSect, RT_SRC_POS, "IOM Lock");
#else
	int rc = PDMR3CritSectInit(pVM, &pVM->iom.s.CritSect, RT_SRC_POS, "IOM Lock");
#endif
	AssertRCReturn(rc, rc);
	return VINF_SUCCESS;
}

int IOMR3Term(PVM)
{
	if (verbose)
		PDBG("called");
	return VINF_SUCCESS;
}


VMMDECL(bool) IOMIsLockWriteOwner(PVM pVM)
{
#ifdef IOM_WITH_CRIT_SECT_RW
	return PDMCritSectRwIsInitialized(&pVM->iom.s.CritSect)
	       && PDMCritSectRwIsWriteOwner(&pVM->iom.s.CritSect);
#else
   return PDMCritSectIsOwner(&pVM->iom.s.CritSect);
#endif
}


int IOMR3MmioRegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart,
                        uint32_t cbRange, RTHCPTR pvUser,
                        R3PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback,
                        R3PTRTYPE(PFNIOMMMIOREAD)  pfnReadCallback,
                        R3PTRTYPE(PFNIOMMMIOFILL)  pfnFillCallback,
                        uint32_t fFlags, const char *pszDesc)
{
	if (verbose)
		PLOG("%s: GCPhys=0x%llx cb=0x%x pszDesc=%s rd=%p wr=%p fl=%p flags=%x",
		     __PRETTY_FUNCTION__,
		     (Genode::uint64_t)GCPhysStart, cbRange, pszDesc,
		     pfnWriteCallback, pfnReadCallback, pfnFillCallback, fFlags);

	REMR3NotifyHandlerPhysicalRegister(pVM, PGMPHYSHANDLERTYPE_MMIO,
	                                   GCPhysStart, cbRange, true);

	guest_memory()->add_mmio_mapping(GCPhysStart, cbRange,
	                                 pDevIns, pvUser, pfnWriteCallback,
	                                 pfnReadCallback, pfnFillCallback, fFlags);

	return VINF_SUCCESS;

}


int IOMR3MmioDeregister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart,
                        uint32_t cbRange)
{
	if (verbose)
		PLOG("%s: GCPhys=0x%llx cb=0x%x", __PRETTY_FUNCTION__,
		     (Genode::uint64_t)GCPhysStart, cbRange);

	bool status = guest_memory()->remove_mmio_mapping(GCPhysStart, cbRange);
	if (status)
		return VINF_SUCCESS;
	

	return !VINF_SUCCESS;
}


VMMDECL(VBOXSTRICTRC) IOMMMIOWrite(PVM pVM, PVMCPU, RTGCPHYS GCPhys,
                                   uint32_t u32Value, size_t cbValue)
{
	VBOXSTRICTRC rc = IOM_LOCK_SHARED(pVM);
	Assert(rc == VINF_SUCCESS);

	rc = guest_memory()->mmio_write(GCPhys, u32Value, cbValue);

	/*
	 * Check whether access is unaligned or access width is less than device
	 * supports. See original IOMMMIOWrite & iomMMIODoComplicatedWrite of VBox.
	 */
	if (rc == VERR_IOM_NOT_MMIO_RANGE_OWNER) {

		/* implement what we need to - extend by need */

		Assert((GCPhys & 3U) == 0);
		Assert(cbValue == 1);

		uint32_t value;

		rc = guest_memory()->mmio_read(GCPhys, &value, sizeof(value));
		Assert(rc == VINF_SUCCESS);

		value &= 0xffffff00;
		value |= (u32Value & 0x000000ff);

		rc = guest_memory()->mmio_write(GCPhys, value, sizeof(value));
	}

	Assert(rc != VERR_IOM_NOT_MMIO_RANGE_OWNER);

	IOM_UNLOCK_SHARED(pVM);

	return rc;
}


VMMDECL(VBOXSTRICTRC) IOMMMIORead(PVM pVM, PVMCPU, RTGCPHYS GCPhys,
                                  uint32_t *pvalue, size_t bytes)
{
    VBOXSTRICTRC rc = IOM_LOCK_SHARED(pVM);
	Assert(rc == VINF_SUCCESS);

	rc = guest_memory()->mmio_read(GCPhys, pvalue, bytes);

	/*
	 * Check whether access is unaligned or access width is less than device
	 * supports. See original IOMMMIORead & iomMMIODoComplicatedRead of VBox.
	 */
	if (rc == VERR_IOM_NOT_MMIO_RANGE_OWNER) {
		/* implement what we need to - extend by need */
		Assert((GCPhys & 3U) == 0);
		Assert(bytes == 1 || bytes == 2);
		uint32_t value;
		rc = guest_memory()->mmio_read(GCPhys, &value, sizeof(value));
		Assert(rc == VINF_SUCCESS);

		if (rc == VINF_SUCCESS) {
			switch (bytes) {
				case 1:
					*(uint8_t *) pvalue = (uint8_t)value;
				case 2:
					*(uint16_t *)pvalue = (uint16_t)value;
			}
		}
	}

	IOM_UNLOCK_SHARED(pVM);

	return rc;
}


int IOMMMIOMapMMIO2Page(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysRemapped,
                        uint64_t fPageFlags)
{
	if (verbose)
		PDBG("called - %llx %llx", (Genode::uint64_t)GCPhys,
		     (Genode::uint64_t)GCPhysRemapped);
	return VINF_SUCCESS;
}


int IOMMMIOResetRegion(PVM pVM, RTGCPHYS GCPhys)
{
	if (verbose)
		PDBG("called - %llx", (Genode::uint64_t)GCPhys);
	return VINF_SUCCESS;
}
