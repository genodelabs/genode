/*
 * \brief  Genode backend for VirtualBox native execution manager
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-11-05
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* VirtualBox includes */
#include <VBox/vmm/cpum.h>      /* must be included before CPUMInternal.h */
#define VMCPU_INCL_CPUM_GST_CTX /* needed for cpum.GstCtx */
#include <CPUMInternal.h>       /* enable access to cpum.s.* */
#include <HMInternal.h>         /* enable access to hm.s.* */
#define RT_OS_WINDOWS           /* needed for definition all nem.s members */
#include <NEMInternal.h>        /* enable access to nem.s.* */
#undef RT_OS_WINDOWS
#include <PGMInternal.h>        /* enable access to pgm.s.* */
#include <VBox/vmm/vmcc.h>      /* must be included before PGMInline.h */
#include <PGMInline.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/em.h>
#include <VBox/err.h>

/* Genode includes */
#include <base/mutex.h>

/* local includes */
#include <stub_macros.h>
#include <sup.h>
#include <sup_vcpu.h>
#include <sup_gmm.h>
#include <sup_vm.h>

static bool const debug = true;

using namespace Genode;


namespace Sup { struct Nem; }

struct Sup::Nem
{
	Gmm &_gmm;

	typedef Sup::Gmm::Protection Protection;

	struct Range
	{
		addr_t first_byte { 0 };
		addr_t last_byte  { 0 };

		Protection prot { false, false, false };

		size_t size() const { return last_byte ? last_byte - first_byte + 1 : 0; }

		/* empty ranges are invalid */
		bool valid() const { return size() != 0; }

		enum class Extend_result { PREPENDED, APPENDED, FAILED };

		Extend_result extend(Range const &other)
		{
			/* ignore invalid ranges */
			if (!other.valid())
				return Extend_result::APPENDED;

			if (!(prot == other.prot))
				return Extend_result::FAILED;

			/* initialize if uninitialized */
			if (!valid()) {
				first_byte = other.first_byte;
				last_byte  = other.last_byte;
				prot       = other.prot;

				return Extend_result::APPENDED;
			}

			/* prepend */
			if (first_byte == other.last_byte + 1) {
				first_byte = other.first_byte;

				return Extend_result::PREPENDED;
			}

			/* append */
			if (last_byte + 1 == other.first_byte) {
				last_byte = other.last_byte;

				return Extend_result::APPENDED;
			}

			/* not contiguous (which includes overlaps) */
			return Extend_result::FAILED;
		}

		void print(Output &o) const
		{
			Genode::print(o, prot, ":", Hex_range(first_byte, size()));
		}
	};

	Mutex mutex       { };
	Range host_range  { };
	Range guest_range { };

	void commit_range_unsynchronized()
	{
		/* ignore commit of invalid ranges */
		if (!host_range.valid())
			return;

		/* commit the current range to GMM */
		_gmm.map_to_guest(Gmm::Vmm_addr   { host_range.first_byte },
		                  Gmm::Guest_addr { guest_range.first_byte },
		                  Gmm::Pages      { host_range.size() >> PAGE_SHIFT },
		                  host_range.prot);

		/* reset ranges */
		host_range  = { };
		guest_range = { };
	}

	void commit_range()
	{
		Mutex::Guard guard(mutex);

		commit_range_unsynchronized();
	}

	void map_to_guest(addr_t host_addr, addr_t guest_addr, size_t size, Protection prot)
	{
		Mutex::Guard guard(mutex);

		Range new_host_range  { host_addr,  host_addr  + (size - 1), prot };
		Range new_guest_range { guest_addr, guest_addr + (size - 1), prot };

		/* new page just extends the current ranges */
		Range::Extend_result const host_extend_result  = new_host_range.extend(host_range);
		Range::Extend_result const guest_extend_result = new_guest_range.extend(guest_range);

		bool const failed      = (host_extend_result == Range::Extend_result::FAILED);
		bool const same_result = (host_extend_result == guest_extend_result);

		if (!failed && same_result) {
			host_range  = new_host_range;
			guest_range = new_guest_range;

			return;
		}

		/* new page starts a new range */
		commit_range_unsynchronized();

		/* start over with new page */
		host_range  = { host_addr,  host_addr  + (size - 1), prot };
		guest_range = { guest_addr, guest_addr + (size - 1), prot };
	}

	void map_page_to_guest(addr_t host_addr, addr_t guest_addr, Protection prot)
	{
		map_to_guest(host_addr, guest_addr, X86_PAGE_SIZE, prot);
	}

	Gmm::Vmm_addr alloc_large_page()
	{
		Gmm::Pages const pages { X86_PAGE_2M_SIZE/X86_PAGE_4K_SIZE };

		return _gmm.alloc_from_reservation(pages);
	}

	Gmm & gmm() { return _gmm; }

	Nem(Gmm &gmm) : _gmm(gmm) { }
};


Sup::Nem * nem_ptr;

void Sup::nem_init(Gmm &gmm)
{
	nem_ptr = new Nem(gmm);
}


VMM_INT_DECL(int) NEMImportStateOnDemand(PVMCPUCC pVCpu, ::uint64_t fWhat) STOP


VMM_INT_DECL(int) NEMHCQueryCpuTick(PVMCPUCC pVCpu, ::uint64_t *pcTicks,
                                    ::uint32_t *puAux) STOP


VMM_INT_DECL(int) NEMHCResumeCpuTickOnAll(PVMCC pVM, PVMCPUCC pVCpu,
                                          ::uint64_t uPausedTscValue) STOP


void nemHCNativeNotifyHandlerPhysicalRegister(PVMCC pVM,
                                              PGMPHYSHANDLERKIND enmKind,
                                              RTGCPHYS GCPhys, RTGCPHYS cb)
{
}


int nemR3NativeInit(PVM pVM, bool fFallback, bool fForced)
{
	VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_NATIVE_API);

	return VINF_SUCCESS;
}


int nemR3NativeInitAfterCPUM(PVM pVM)
{
	return VINF_SUCCESS;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
	return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM) STOP


/**
 * VM reset notification.
 *
 * @param   pVM         The cross context VM structure.
 */
void nemR3NativeReset(PVM pVM) TRACE()


/**
 * Reset CPU due to INIT IPI or hot (un)plugging.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the CPU being
 *                      reset.
 * @param   fInitIpi    Whether this is the INIT IPI or hot (un)plugging case.
 */
void nemR3NativeResetCpu(PVMCPU pVCpu, bool fInitIpi) TRACE()


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
	using namespace Sup;

	Vm &vm = *static_cast<Vm *>(pVM);

	/* commit on VM entry */
	nem_ptr->commit_range();

	VBOXSTRICTRC result = 0;
	vm.with_vcpu(Cpu_index { pVCpu->idCpu }, [&] (Sup::Vcpu &vcpu) {
		result = vcpu.run(); });

	return result;
}


bool nemR3NativeCanExecuteGuest(PVM pVM, PVMCPU pVCpu)
{
	return true;
}


bool nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable) TRACE(false)


/**
 * Forced flag notification call from VMEmt.h.
 *
 * This is only called when pVCpu is in the VMCPUSTATE_STARTED_EXEC_NEM state.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the CPU
 *                          to be notified.
 * @param   fFlags          Notification flags
 *                          (VMNOTIFYFF_FLAGS_DONE_REM/VMNOTIFYFF_FLAGS_POKE)
 */
void nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, ::uint32_t fFlags)
{
	/* nemHCWinCancelRunVirtualProcessor(pVM, pVCpu); */
	if (fFlags & VMNOTIFYFF_FLAGS_POKE) {
		Sup::Vm &vm = *(Sup::Vm *)pVM;

		vm.with_vcpu(Sup::Cpu_index { pVCpu->idCpu }, [&] (Sup::Vcpu &vcpu) {
			vcpu.pause(); });
	}
}


static void update_pgm_large_page(PVM pVM, addr_t guest_addr, addr_t host_addr,
                                  uint32_t page_id)
{
	/* init all pages in large page (see PGMR3PhysAllocateLargeHandyPage()) */
	for (unsigned i = 0; i < X86_PAGE_2M_SIZE/X86_PAGE_4K_SIZE; ++i) {

		PPGMPAGE page = nullptr;

		pgmPhysGetPageEx(pVM, guest_addr, &page);

		if (PGM_PAGE_GET_TYPE(page) != PGMPAGETYPE_RAM)
			error(__func__, ": page is not RAM");
		if (!PGM_PAGE_IS_ZERO(page))
			error(__func__, ": page is not zero page");

		pVM->pgm.s.cZeroPages--;
		pVM->pgm.s.cPrivatePages++;
		PGM_PAGE_SET_HCPHYS(pVM, page, host_addr);
		PGM_PAGE_SET_PAGEID(pVM, page, page_id);
		PGM_PAGE_SET_STATE(pVM, page, PGM_PAGE_STATE_ALLOCATED);
		PGM_PAGE_SET_PDE_TYPE(pVM, page, PGM_PAGE_PDE_TYPE_PDE);
		PGM_PAGE_SET_PTE_INDEX(pVM, page, 0);
		PGM_PAGE_SET_TRACKING(pVM, page, 0);

		page_id++;

		host_addr  += X86_PAGE_4K_SIZE;
		guest_addr += X86_PAGE_4K_SIZE;
	}
}


/**
 * NEM is notified about each RAM range by calling this function repeatedly
 *
 * PGMR3PhysRegisterRam() holds the PGM lock while calling.
 */
int nemR3NativeNotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb)
{
	/*
	 * PGM notifies us about each RAM range configured, which means "Base RAM"
	 * below 4 GiB and "Above 4GB Base RAM" (see MMR3InitPaging()). We eagerly
	 * map all 2M-aligened "large" pages in the ranges to guest memory and
	 * initialize PGM to benefit from reduced TLB usage and less backing store
	 * for many mapped regions. RAM pages outside the large pages are backed on
	 * demand by PGM by "small" handy pages by default. Unfortunately, the
	 * configuration of NEM disables automatic use of large pages in PGM.
	 */

	/* start at first 2M-aligned page in range */
	addr_t const guest_base = RT_ALIGN(GCPhys, X86_PAGE_2M_SIZE);

	/* iterate over all large pages in range */
	for (addr_t addr = guest_base; addr + _2M <= GCPhys + cb; addr += _2M ) {

		/*
		 * We skip the first 2 MiB to prevent errors with ROM mappings below 1
		 * MiB. Also, a range of 64 KiB at 1 MiB is replaced regularly on A20
		 * switching. Both facts invalidate our large-page mapping.
		 */
		if (addr < _2M) continue;

		/* allocate and map in GMM */
		Sup::Gmm::Vmm_addr const vmm_addr    = nem_ptr->alloc_large_page();
		Sup::Gmm::Page_id  const vmm_page_id = nem_ptr->gmm().page_id(vmm_addr);
		uint32_t           const page_id32   = nem_ptr->gmm().page_id_as_uint32(vmm_page_id);

		Sup::Nem::Protection const prot { true, true, true };

		nem_ptr->map_to_guest(vmm_addr.value, addr, X86_PAGE_2M_SIZE, prot);

		update_pgm_large_page(pVM, addr, vmm_addr.value, page_id32);
	}

	/* invalidate PGM caches (see pgmPhysAllocPage()) */
	PGM_INVL_ALL_VCPU_TLBS(pVM);
	pgmPhysInvalidatePageMapTLB(pVM);

	return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysMmioExMap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb,
                                   ::uint32_t fFlags, void *pvMmio2)
{
	/*
	 * This is called from PGMPhys.cpp with
	 *
	 * fFlags = (pFirstMmio->fFlags & PGMREGMMIO2RANGE_F_MMIO2       ? NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2   : 0)
	 *        | (pFirstMmio->fFlags & PGMREGMMIO2RANGE_F_OVERLAPPING ? NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE : 0);
	 */

	return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb,
                                     ::uint32_t fFlags)
{
	return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys,
                                          RTGCPHYS cb, ::uint32_t fFlags)
{
	return VINF_SUCCESS;
}


int nemR3NativeNotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys,
                                         RTGCPHYS cb, ::uint32_t fFlags)
{
	return VINF_SUCCESS;
}


/**
 * Called when the A20 state changes.
 *
 * Do a very minimal emulation of the HMA to make DOS happy.
 *
 * @param   pVCpu           The CPU the A20 state changed on.
 * @param   fEnabled        Whether it was enabled (true) or disabled.
 */
void nemR3NativeNotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
	PVM pVM = pVCpu->CTX_SUFF(pVM);

	/* unmap HMA guest memory on A20 change */
	if (pVM->nem.s.fA20Enabled != fEnabled) {
		pVM->nem.s.fA20Enabled  = fEnabled;

		Sup::Nem::Protection const prot_none {
			.readable   = false,
			.writeable  = false,
			.executable = false,
		};

		for (RTGCPHYS GCPhys = _1M; GCPhys < _1M + _64K; GCPhys += X86_PAGE_SIZE)
			nem_ptr->map_page_to_guest(0, GCPhys | RT_BIT_32(20),  prot_none);
	}
}


void nemHCNativeNotifyHandlerPhysicalDeregister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind,
                                                RTGCPHYS GCPhys, RTGCPHYS cb,
                                                int fRestoreAsRAM,
                                                bool fRestoreAsRAM2)
{
}


void nemHCNativeNotifyHandlerPhysicalModify(PVMCC pVM, PGMPHYSHANDLERKIND enmKind,
                                            RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew,
                                            RTGCPHYS cb, bool fRestoreAsRAM) STOP


int nemHCNativeNotifyPhysPageAllocated(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys,
                                       ::uint32_t fPageProt, PGMPAGETYPE enmType,
                                       ::uint8_t *pu2State)
{
	nemHCNativeNotifyPhysPageProtChanged(pVM, GCPhys, HCPhys,
	                                     fPageProt, enmType, pu2State);

	return VINF_SUCCESS;
}


void nemHCNativeNotifyPhysPageProtChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys,
                                          ::uint32_t fPageProt, PGMPAGETYPE enmType,
                                          ::uint8_t *pu2State)
{
	Sup::Nem::Protection const prot {
		.readable   = (fPageProt & NEM_PAGE_PROT_READ) != 0,
		.writeable  = (fPageProt & NEM_PAGE_PROT_WRITE) != 0,
		.executable = (fPageProt & NEM_PAGE_PROT_EXECUTE) != 0,
	};

	/*
	 * The passed host and guest addresses may not be aligned, e.g., when
	 * called from DevVGA.cpp vgaLFBAccess(). Therefore, we do the alignment
	 * here explicitly.
	 */

	nem_ptr->map_page_to_guest(HCPhys & ~PAGE_OFFSET_MASK,
	                           GCPhys & ~PAGE_OFFSET_MASK, prot);
}


void nemHCNativeNotifyPhysPageChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev,
                                      RTHCPHYS HCPhysNew, ::uint32_t fPageProt,
                                      PGMPAGETYPE enmType, ::uint8_t *pu2State) STOP

