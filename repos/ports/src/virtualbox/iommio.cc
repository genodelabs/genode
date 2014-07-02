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
#include <VBox/vmm/iom.h>
#include <VBox/vmm/rem.h>

/* local includes */
#include "guest_memory.h"


int IOMR3MmioRegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart,
                        uint32_t cbRange, RTHCPTR pvUser,
                        R3PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback,
                        R3PTRTYPE(PFNIOMMMIOREAD)  pfnReadCallback,
                        R3PTRTYPE(PFNIOMMMIOFILL)  pfnFillCallback,
                        uint32_t fFlags, const char *pszDesc)
{
	PLOG("%s: GCPhys=0x%lx cb=0x%x pszDesc=%s rd=%p wr=%p fl=%p",
	     __PRETTY_FUNCTION__,
	     (long)GCPhysStart, cbRange, pszDesc,
	     pfnWriteCallback, pfnReadCallback, pfnFillCallback);

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
	PLOG("%s: GCPhys=0x%lx cb=0x%x", __PRETTY_FUNCTION__, GCPhysStart, cbRange);

	bool status = guest_memory()->remove_mmio_mapping(GCPhysStart, cbRange);
	if (status)
		return VINF_SUCCESS;
	

	return !VINF_SUCCESS;
}


VBOXSTRICTRC IOMMMIOWrite(PVM pVM, RTGCPHYS GCPhys, uint32_t u32Value, size_t cbValue)
{
//	PDBG("GCPhys=0x%x, u32Value=0x%x, cbValue=%zd", GCPhys, u32Value, cbValue);

	return guest_memory()->mmio_write(pVM, GCPhys, u32Value, cbValue);
}


VBOXSTRICTRC IOMMMIORead(PVM pVM, RTGCPHYS GCPhys, uint32_t *pu32Value,
                         size_t cbValue)
{
//	PDBG("GCPhys=0x%x, cbValue=%zd", GCPhys, cbValue);

	return guest_memory()->mmio_read(pVM, GCPhys, pu32Value, cbValue);
}


int IOMMMIOMapMMIO2Page(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysRemapped,
                        uint64_t fPageFlags)
{
	PDBG("called - %lx %lx", GCPhys, GCPhysRemapped);
	return VINF_SUCCESS;
}

int IOMMMIOResetRegion(PVM pVM, RTGCPHYS GCPhys)
{
	PDBG("called - %lx", GCPhys);
	return VINF_SUCCESS;
}
