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

/* Genode includes */
#include <base/env.h>

/* VirtualBox includes */
#include <IOMInternal.h> /* enable access to iom.s.* */
#include <NEMInternal.h> /* enable access to nem.s.* */
#include <PGMInternal.h> /* enable access to pgm.s.* */
#include <VMMInternal.h> /* enable access to vmm.s.* */
#define PDMPCIDEV_INCLUDE_PRIVATE   /* needed for PDMPCIDEVINT_DECLARED */
#define VBOX_IN_VMM                 /* needed for definition of PDMTASKTYPE */
#include <PDMInternal.h>
#undef VBOX_IN_VMM
#include <SUPDrvIOC.h>
#include <VBox/err.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/gvmm.h>
#include <SUPLibInternal.h>

/* local includes */
#include <init.h>
#include <sup_drv.h>
#include <sup_vm.h>
#include <pthread_emt.h>
#include <stub_macros.h>

static bool const debug = true;

using namespace Genode;

static Sup::Drv *sup_drv;

void Sup::init(Env &env)
{
	sup_drv = new Sup::Drv(env);

	nem_init(sup_drv->gmm());
}


static bool before_first_call_of_ioctl_query_funcs = true;


/*******************************
 ** Ioctl interface functions **
 *******************************/

/*
 * Helper to remove aliasing between request.u.In and request.u.Out
 *
 * The request structures pass IN and OUT parameters in a union, which
 * creates two problems.
 *
 * - OUT are not initialized to zero by default. Instead, they contain
 *   bits of IN parameters.
 *
 * - IN parameters cannot safely be consumed after assigning any OUT
 *   parameter.
 *
 * This utility solves these issues by copying-out IN parameters first,
 * resetting the OUT parameters to zero, and calling 'fn' with separate
 * IN and OUT arguments.
 */
template <typename T, typename FN>
static void with_inout_ioctl(T &request, FN const &fn)
{
	auto const in  = request.u.In;
	auto      &out = request.u.Out;
	auto      &rc  = request.Hdr.rc;

	out = { };
	rc  = VINF_SUCCESS;

	fn(in, out, request.Hdr.rc);
}


template <typename T, typename FN>
static void with_out_ioctl(T &request, FN const &fn)
{
	auto &out = request.u.Out;
	auto &rc  = request.Hdr.rc;

	out = { };
	rc  = VINF_SUCCESS;

	fn(out, request.Hdr.rc);
}


static void ioctl(SUPCOOKIE &request)
{
	with_inout_ioctl(request, [&] (auto const &, auto &out, auto &) {

		out.u32SessionVersion = SUPDRV_IOC_VERSION;
	});
}


static void ioctl(SUPQUERYFUNCS &request)
{
	with_out_ioctl(request, [&] (auto &out, auto &) { });

	before_first_call_of_ioctl_query_funcs = false;
}


static void ioctl(SUPGIPMAP &request)
{
	with_out_ioctl(request, [&] (auto &out, auto &) {

		request.u.Out.pGipR3 = sup_drv->gip();
	});
}


static void ioctl(SUPVTCAPS &request)
{
	with_out_ioctl(request, [&] (auto &out, auto &rc) {

		/*
		 * Return VERR_VMX_NO_VMX and VERR_SVM_NO_SVM to trigger the use of
		 * the native execution manager (follow NEMR3Init).
		 */
		switch (sup_drv->cpu_virt()) {
		case Sup::Drv::Cpu_virt::VMX:
			rc        = VERR_VMX_NO_VMX;
			out.fCaps = SUPVTCAPS_VT_X | SUPVTCAPS_NESTED_PAGING;
			break;
		case Sup::Drv::Cpu_virt::SVM:
			rc        = VERR_SVM_NO_SVM;
			out.fCaps = SUPVTCAPS_AMD_V | SUPVTCAPS_NESTED_PAGING;
			break;
		case Sup::Drv::Cpu_virt::NONE:
			rc        = VERR_UNSUPPORTED_CPU;
			out.fCaps = 0;
			break;
		}

		/*
		 * Prevent returning an erroneous rc value when VT caps are queried
		 * during the early initialization path of Host::init,
		 * i_updateProcessorFeatures. Otherwise, the assertions in
		 * i_updateProcessorFeatures would trigger.
		 *
		 * Later, when called during the VM initialization via vmR3InitRing3,
		 * HMR3Init, we have to return VERR_VMX_NO_VMX or VERR_SVM_NO_SVM to
		 * force the call of NEMR3Init.
		 */
		if (before_first_call_of_ioctl_query_funcs)
			rc = VINF_SUCCESS;

		/*
		 * XXX are the following interesting?
		 * SUPVTCAPS_VTX_VMCS_SHADOWING
		 * SUPVTCAPS_VTX_UNRESTRICTED_GUEST
		 */
	});
}


static void setup_vcpu_handler(Sup::Vm &vm, Sup::Cpu_index cpu)
{
	Pthread::Emt &emt = Pthread::emt_for_cpu(cpu);

	Sup::Vcpu &vcpu = sup_drv->create_vcpu(vm, cpu, emt);

	vm.register_vcpu(cpu, vcpu);
}


static int vmmr0_gvmm_create_vm(GVMMCREATEVMREQ &request)
{
	Sup::Cpu_count cpu_count { request.cCpus };

	Sup::Vm &new_vm = Sup::Vm::create(request.pSession, cpu_count);

	/*
	 * The first EMT thread creates the VM and must be registered implicitly.
	 * Additional EMTs register themselves via vmmr0_gvmm_register_vcpu().
	 */

	setup_vcpu_handler(new_vm, Sup::Cpu_index { 0 });

	request.pVMR3 = &new_vm;
	request.pVMR0 = (PVMR0)request.pVMR3;

	return VINF_SUCCESS;
}


static int vmmr0_gvmm_register_vcpu(PVMR0 pvmr0, uint32_t cpu)
{
	Sup::Vm &vm = *(Sup::Vm *)pvmr0;

	/*
	 * EMT threads for additional CPUs are registered on initialization.
	 * The EMT-0 thread does not register via this function.
	 *
	 * GVMMR0RegisterVCpu() does the following...
	 *
	 * pGVM->aCpus[idCpu].hNativeThreadR0 = pGVM->aCpus[idCpu].hEMT = RTThreadNativeSelf();
	 */

	setup_vcpu_handler(vm, Sup::Cpu_index { cpu });

	return VINF_SUCCESS;
}


static int vmmr0_gvmm_sched_halt(PVMR0 pvmr0, ::uint32_t cpu, ::uint64_t expire_timestamp)
{
	Sup::Vm &vm = *(Sup::Vm *)pvmr0;

	::uint64_t const now_timestamp = RTTimeNanoTS();

	::uint64_t ns_diff = expire_timestamp > now_timestamp
	                   ? expire_timestamp - now_timestamp : 0;

	if (!ns_diff)
		return VINF_SUCCESS;

	if (ns_diff > RT_NS_1SEC) {
		warning(" clamping halt duration of ", ns_diff, " ns to 1 s");
		ns_diff = RT_NS_1SEC;
	}

	vm.with_vcpu(Sup::Cpu_index { cpu }, [&] (Sup::Vcpu &vcpu) {
		vcpu.halt(ns_diff); });

	/*
	 * returns VINF_SUCCESS      on normal wakeup (timeout or kicked by other thread)
	 *         VERR_INTERRUPTED  if a signal was scheduled for the thread
	 */
	return VINF_SUCCESS;
}


static int vmmr0_gvmm_sched_wake_up(PVMR0 pvmr0, uint32_t cpu)
{
	Sup::Vm &vm = *(Sup::Vm *)pvmr0;

	vm.with_vcpu(Sup::Cpu_index { cpu }, [&] (Sup::Vcpu &vcpu) {
		vcpu.wake_up(); });

	return VINF_SUCCESS;
}


static int vmmr0_gvmm_sched_poll(PVMR0 pvmr0, uint32_t cpu, bool /*yield*/)
{
	/*
	 * GVMMR0SchedPoll() just wakes up waiters on gvmm.s.HaltEventMulti. In our
	 * case, we just call vmmr0_gvmm_sched_wake_up(). Note, 'yield' must always
	 * be false according to comment in GVMMR0SchedPoll().
	 */
	return vmmr0_gvmm_sched_wake_up(pvmr0, cpu);
}


static int vmmr0_gmm_initial_reservation(GMMINITIALRESERVATIONREQ &request)
{
	Sup::Gmm::Pages pages { request.cBasePages
	                      + request.cShadowPages
	                      + request.cFixedPages };

	sup_drv->gmm().reservation_pages(pages);

	return VINF_SUCCESS;
}


static int vmmr0_gmm_update_reservation(GMMUPDATERESERVATIONREQ &request)
{
	Sup::Gmm::Pages pages { request.cBasePages
	                      + request.cShadowPages
	                      + request.cFixedPages };

	sup_drv->gmm().reservation_pages(pages);

	return VINF_SUCCESS;
}


static int vmmr0_gmm_allocate_pages(GMMALLOCATEPAGESREQ &request)
{
	Sup::Gmm::Pages pages { request.cPages };

	using Vmm_addr = Sup::Gmm::Vmm_addr;
	using Page_id  = Sup::Gmm::Page_id;

	Vmm_addr const vmm_addr = sup_drv->gmm().alloc_from_reservation(pages);

	for (unsigned i = 0; i < request.cPages; i++) {

		GMMPAGEDESC &page = request.aPages[i];

		Vmm_addr const page_addr { vmm_addr.value + i*PAGE_SIZE };
		Page_id  const page_id = sup_drv->gmm().page_id(page_addr);

		page.HCPhysGCPhys = page_addr.value;
		page.idPage       = sup_drv->gmm().page_id_as_uint32(page_id);
		page.idSharedPage = NIL_GMM_PAGEID;
	}

	return VINF_SUCCESS;
}


static int vmmr0_gmm_free_pages(GMMFREEPAGESREQ &request)
{
	for (unsigned i = 0; i < request.cPages; i++) {

		GMMFREEPAGEDESC &page = request.aPages[i];

		using Vmm_addr = Sup::Gmm::Vmm_addr;
		using Page_id  = Sup::Gmm::Page_id;

		Page_id const page_id { page.idPage };

		Vmm_addr const vmm_addr = sup_drv->gmm().vmm_addr(page_id);

		sup_drv->gmm().free(vmm_addr);
	}

	return VINF_SUCCESS;
}


static int vmmr0_gmm_map_unmap_chunk(GMMMAPUNMAPCHUNKREQ &request)
{
	if (request.idChunkMap != NIL_GMM_CHUNKID) {
		Sup::Gmm::Page_id const page_id { request.idChunkMap << GMM_CHUNKID_SHIFT };

		request.pvR3 = (RTR3PTR)sup_drv->gmm().vmm_addr(page_id).value;
	}

	return VINF_SUCCESS;
}


static int vmmr0_iom_grow_io_ports(PVMR0 pvmr0, ::uint64_t min_entries)
{
	/* satisfy IOMR3IoPortCreate */
	Sup::Vm &vm = *(Sup::Vm *)pvmr0;

	unsigned const old_bound = vm.iom.s.cIoPortAlloc;
	unsigned const new_bound = max(min_entries, old_bound);

	IOMIOPORTENTRYR3     *r3_entries     = new IOMIOPORTENTRYR3    [new_bound] { };
	IOMIOPORTLOOKUPENTRY *lookup_entries = new IOMIOPORTLOOKUPENTRY[new_bound] { };

	/* preserve content of the existing arrays */
	for (unsigned i = 0; i < old_bound; i++) {
		r3_entries    [i] = vm.iom.s.paIoPortRegs[i];
		lookup_entries[i] = vm.iom.s.paIoPortLookup[i];
	}

	/* initialize new array elements */
	for (unsigned i = old_bound; i < new_bound; i++) {
		r3_entries[i].idxSelf  = (uint16_t)i;
		r3_entries[i].idxStats = UINT16_MAX;
	}

	/* replace old arrays with new ones */
	delete vm.iom.s.paIoPortLookup;
	delete vm.iom.s.paIoPortRegs;

	vm.iom.s.paIoPortRegs   = r3_entries;
	vm.iom.s.paIoPortLookup = lookup_entries;
	vm.iom.s.cIoPortAlloc   = new_bound;

	return VINF_SUCCESS;
}


static int vmmr0_iom_grow_mmio_regs(PVMR0 pvmr0, ::uint64_t min_entries)
{
	/* satisfy IOMR3MmioCreate */
	Sup::Vm &vm = *(Sup::Vm *)pvmr0;

	unsigned const old_bound = vm.iom.s.cMmioAlloc;
	unsigned const new_bound = max(min_entries, old_bound);

	IOMMMIOENTRYR3     *r3_entries     = new IOMMMIOENTRYR3    [new_bound] { };
	IOMMMIOLOOKUPENTRY *lookup_entries = new IOMMMIOLOOKUPENTRY[new_bound] { };

	/* preserve content of the existing arrays */
	for (unsigned i = 0; i < old_bound; i++) {
		r3_entries    [i] = vm.iom.s.paMmioRegs[i];
		lookup_entries[i] = vm.iom.s.paMmioLookup[i];
	}

	/* initialize new array elements */
	for (unsigned i = old_bound; i < new_bound; i++) {
		r3_entries[i].idxSelf  = (uint16_t)i;
		r3_entries[i].idxStats = UINT16_MAX;
	}

	/* replace old arrays with new ones */
	delete vm.iom.s.paMmioLookup;
	delete vm.iom.s.paMmioRegs;

	vm.iom.s.paMmioRegs   = r3_entries;
	vm.iom.s.paMmioLookup = lookup_entries;
	vm.iom.s.cMmioAlloc   = new_bound;

	return VINF_SUCCESS;
}


static int vmmr0_pdm_device_create(PDMDEVICECREATEREQ &request)
{
	/*
	 * Allocate all PDM device ingredients as a single contiguous memory block.
	 *
	 * 1. The actual PDMDEVINSR3 structure (Head)
	 *
	 * 2. The CC (current context) size is passed as 'cbInstanceR3'
	 *    request argument. Its backing store must immediately follow the
	 *    PDMDEVINSR3 structure because the PDMDEVINSR3 last member
	 *    'achInstanceData[0]' is expected to correspond to the InstanceCC
	 *    object.
	 *
	 * 3. The shared state of the device instance. The size of this
	 *    object is known only by the respective device model and passed as
	 *    'cbInstanceShared' request argument.
	 *
	 * 4. Backing store of the objects referenced by the 'PDMDEVINSR3' (Tail)
	 *
	 * PDMDevHlp.cpp tests for certain allocation patterns, e.g., in
	 * pdmR3DevHlp_SetDeviceCritSect, there is the following assertion:
	 *
	 *   Assert((uintptr_t)pOldCritSect - (uintptr_t)pDevIns < pDevIns->cbRing3);
	 */
	struct Head
	{
		PDMDEVINSR3 pdmdev { };
	};

	size_t const r3_size = request.cbInstanceR3;

	/*
	 * The 'pvInstanceDataForR3' backing store is used for the R3 device state,
	 * e.g., DEVPCIROOT for the PCI bus, or KBDSTATE for the PS2 keyboard.
	 */
	size_t const shared_size = request.cbInstanceShared;

	struct Tail
	{
		PDMCRITSECT critsect { };

		enum { NUM_PCI_DEVS = sizeof(PDMDEVINSR3::apPciDevs) /
		                      sizeof(PDMDEVINSR3::apPciDevs[0]) };

		PDMPCIDEV pcidevs[NUM_PCI_DEVS] { };
	};

	size_t const alloc_size = sizeof(Head) + r3_size + shared_size + sizeof(Tail);
	char * const alloc_ptr  = (char *)RTMemPageAllocZ(alloc_size);

	/* define placement of head, r3 instance object, and tail */
	Head &head = *(Head *)alloc_ptr;

	char * const r3_instance_ptr     = (char *)&head.pdmdev.achInstanceData[0];
	char * const shared_instance_ptr = (char *)&head + sizeof(Head) + r3_size;

	Tail &tail = *(Tail *)(shared_instance_ptr + shared_size);

	/* initialize PDMDEVINSR3 */
	{
		PDMDEVINSR3 &pdmdev = head.pdmdev;

		pdmdev.pvInstanceDataForR3 = r3_instance_ptr;
		pdmdev.pvInstanceDataR3    = shared_instance_ptr;
		pdmdev.pCritSectRoR3       = &tail.critsect;
		pdmdev.cbRing3             = alloc_size;

		/* needed for PDMDEV_CALC_PPCIDEV */
		pdmdev.cPciDevs = Tail::NUM_PCI_DEVS;
		pdmdev.cbPciDev = sizeof(PDMPCIDEV);

		for (size_t i = 0; i < Tail::NUM_PCI_DEVS; i++) {

			PDMPCIDEV &pcidev = tail.pcidevs[i];

			pcidev.cbConfig        = _4K;
			/* PDMR0Device.cpp also initializes pcidev.cbMsixState here */
			pcidev.Int.s.idxSubDev = i;
			pcidev.idxSubDev       = i;
			pcidev.u32Magic        = PDMPCIDEV_MAGIC;

			pdmdev.apPciDevs[i] = &pcidev;
		}

		pdmdev.fR0Enabled           = true;
		pdmdev.Internal.s.fIntFlags = PDMDEVINSINT_FLAGS_R0_ENABLED;
		pdmdev.u32Version           = PDM_DEVINS_VERSION;

		request.pDevInsR3 = &pdmdev;
	}

	return VINF_SUCCESS;
}


static int vmmr0_pdm_device_gen_call(PDMDEVICEGENCALLREQ &request)
{
	if (false)
		warning("PDMDEVICEGENCALLREQ PDMDEVICEGENCALL=", (int)request.enmCall, " not implemented");

	return VINF_SUCCESS;
}


static int vmmr0_pgm_allocate_handy_pages(PVMR0 pvmr0)
{
	Sup::Vm &vm = *(Sup::Vm *)pvmr0;

	uint32_t const start_idx = vm.pgm.s.cHandyPages;
	uint32_t const stop_idx  = RT_ELEMENTS(vm.pgm.s.aHandyPages);

	using Vmm_addr = Sup::Gmm::Vmm_addr;
	using Page_id  = Sup::Gmm::Page_id;

	for (unsigned i = start_idx; i < stop_idx; ++i) {

		Vmm_addr const vmm_addr = sup_drv->gmm().alloc_from_reservation( Sup::Gmm::Pages { 1 } );

		GMMPAGEDESC &page = vm.pgm.s.aHandyPages[i];

		Vmm_addr const page_addr { vmm_addr.value };
		Page_id  const page_id = sup_drv->gmm().page_id(page_addr);

		page.HCPhysGCPhys = page_addr.value;
		page.idPage       = sup_drv->gmm().page_id_as_uint32(page_id);
		page.idSharedPage = NIL_GMM_PAGEID;
	}

	vm.pgm.s.cHandyPages = stop_idx;

	return VINF_SUCCESS;
}


static int vmmr0_vmmr0_init(PVMR0 pvmr0)
{
	/* produces
	 *
	 * [init -> vbox1] EMT      VMM: Thread-context hooks unavailable
	 * [init -> vbox1] EMT      VMM: Warning! RTThreadPreemptIsPending() cannot be trusted!  Need to update kernel info?
	 * [init -> vbox1] EMT      VMM: Kernel preemption is not possible it seems
	 */

	return VINF_SUCCESS;
}


static int vmmr0_vmmr0_init_emt(PVMR0 pvmr0, uint32_t cpu)
{
	return VINF_SUCCESS;
}


static void ioctl(SUPCALLVMMR0 &request)
{
	auto &rc = request.Hdr.rc;

	VMMR0OPERATION const operation = VMMR0OPERATION(request.u.In.uOperation);

	switch (operation) {

	case VMMR0_DO_GVMM_CREATE_VM:
		rc = vmmr0_gvmm_create_vm(*(GVMMCREATEVMREQ *)request.abReqPkt);
		return;

	case VMMR0_DO_GVMM_REGISTER_VMCPU:
		rc = vmmr0_gvmm_register_vcpu(request.u.In.pVMR0, request.u.In.idCpu);
		return;

	case VMMR0_DO_GVMM_SCHED_HALT:
		rc = vmmr0_gvmm_sched_halt(request.u.In.pVMR0, request.u.In.idCpu, request.u.In.u64Arg);
		return;

	case VMMR0_DO_GVMM_SCHED_WAKE_UP:
		rc = vmmr0_gvmm_sched_wake_up(request.u.In.pVMR0, request.u.In.idCpu);
		return;

	case VMMR0_DO_GVMM_SCHED_POLL:
		rc = vmmr0_gvmm_sched_poll(request.u.In.pVMR0, request.u.In.idCpu, !!request.u.In.u64Arg);
		return;

	case VMMR0_DO_GMM_INITIAL_RESERVATION:
		rc = vmmr0_gmm_initial_reservation(*(GMMINITIALRESERVATIONREQ *)request.abReqPkt);
		return;

	case VMMR0_DO_GMM_UPDATE_RESERVATION:
		rc = vmmr0_gmm_update_reservation(*(GMMUPDATERESERVATIONREQ *)request.abReqPkt);
		return;

	case VMMR0_DO_GMM_ALLOCATE_PAGES:
		rc = vmmr0_gmm_allocate_pages(*(GMMALLOCATEPAGESREQ *)request.abReqPkt);
		return;

	case VMMR0_DO_GMM_FREE_PAGES:
		rc = vmmr0_gmm_free_pages(*(GMMFREEPAGESREQ *)request.abReqPkt);
		return;

	case VMMR0_DO_GMM_MAP_UNMAP_CHUNK:
		rc = vmmr0_gmm_map_unmap_chunk(*(GMMMAPUNMAPCHUNKREQ *)request.abReqPkt);
		return;

	case VMMR0_DO_IOM_GROW_IO_PORTS:
		rc = vmmr0_iom_grow_io_ports(request.u.In.pVMR0, request.u.In.u64Arg);
		return;

	case VMMR0_DO_IOM_GROW_MMIO_REGS:
		rc = vmmr0_iom_grow_mmio_regs(request.u.In.pVMR0, request.u.In.u64Arg);
		return;

	case VMMR0_DO_PDM_DEVICE_CREATE:
		rc = vmmr0_pdm_device_create(*(PDMDEVICECREATEREQ *)request.abReqPkt);
		return;

	case VMMR0_DO_PDM_DEVICE_GEN_CALL:
		rc = vmmr0_pdm_device_gen_call(*(PDMDEVICEGENCALLREQ *)request.abReqPkt);
		return;

	case VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES:
		rc = vmmr0_pgm_allocate_handy_pages(request.u.In.pVMR0);
		return;

	case VMMR0_DO_VMMR0_INIT:
		rc = vmmr0_vmmr0_init(request.u.In.pVMR0);
		return;

	case VMMR0_DO_VMMR0_INIT_EMT:
		rc = vmmr0_vmmr0_init_emt(request.u.In.pVMR0, request.u.In.idCpu);
		return;

	/* XXX ignore ioctls called during poweroff */
	case VMMR0_DO_GVMM_DEREGISTER_VMCPU:
	case VMMR0_DO_VMMR0_TERM:
	case VMMR0_DO_PGM_FLUSH_HANDY_PAGES:
	case VMMR0_DO_GMM_BALLOONED_PAGES:
	case VMMR0_DO_GMM_RESET_SHARED_MODULES:
		rc = VINF_SUCCESS;
		return;

	default:
		error(__func__, " operation=", (int)operation);
		rc = VERR_NOT_IMPLEMENTED;
		STOP
	}
}


static void ioctl(SUPGETHWVIRTMSRS &request)
{
	with_inout_ioctl(request, [&] (auto const &in, auto &, auto &) {

		warning("SUPGETHWVIRTMSRS fForce=", in.fForce);
	});
}


static void ioctl(SUPUCODEREV &request)
{
	with_out_ioctl(request, [&] (auto &out, auto &) {

		out.MicrocodeRev = ~0u;
	});
}


static void ioctl(SUPGETPAGINGMODE &request)
{
	with_out_ioctl(request, [&] (auto &out, auto &) {

		out.enmMode = sizeof(long) == 32 ? SUPPAGINGMODE_32_BIT_GLOBAL
		                                 : SUPPAGINGMODE_AMD64_GLOBAL_NX;
	});
}


static void ioctl(SUPPAGEFREE &request)
{
	using Vmm_addr = Sup::Gmm::Vmm_addr;

	Vmm_addr const vmm_addr { (addr_t)request.u.In.pvR3 };

	sup_drv->gmm().free(vmm_addr);

	request.Hdr.rc = VINF_SUCCESS;
}


static void ioctl(SUPPAGEALLOCEX &request)
{
	/*
	 * PGMR3PhysMMIORegister() allocates RAM pages for use as MMIO pages in
	 * guests via MMHyperAlloc(). The actual guest mappings are created via
	 * nemHCNativeNotifyPhysPageProtChanged(). Therefore, we allocate also
	 * MMHyper page allocations from GMM.
	 */

	with_inout_ioctl(request, [&] (auto const &in, auto &out, auto &) {

		Sup::Gmm::Pages pages { in.cPages };

		using Vmm_addr = Sup::Gmm::Vmm_addr;

		Vmm_addr const vmm_addr = sup_drv->gmm().alloc_ex(pages);

		out.pvR3 = (R3PTRTYPE(void *))vmm_addr.value;
		out.pvR0 = (R0PTRTYPE(void *))vmm_addr.value;

		for (unsigned i = 0; i < pages.value; i++)
			out.aPages[i] = vmm_addr.value + i*PAGE_SIZE;
	});
}


static void ioctl(SUPSETVMFORFAST &request)
{
	request.Hdr.rc = VINF_SUCCESS;
}


/*********************************
 ** VirtualBox suplib interface **
 *********************************/

int suplibOsInit(PSUPLIBDATA pThis, bool fPreInited, bool fUnrestricted,
                 SUPINITOP *penmWhat, PRTERRINFO pErrInfo)
{
	/* set hDevice to !NIL_RTFILE - checked by SUPR3PageAllocEx() */
	pThis->hDevice       = !NIL_RTFILE;
	pThis->fUnrestricted = fUnrestricted;

	return VINF_SUCCESS;
}


int suplibOsTerm(PSUPLIBDATA) { return VINF_SUCCESS; }


int suplibOsInstall(void) TRACE(VERR_NOT_IMPLEMENTED)


int suplibOsUninstall(void) TRACE(VERR_NOT_IMPLEMENTED)


int suplibOsIOCtl(PSUPLIBDATA pThis, uintptr_t opcode, void *req, size_t len)
{
	bool not_implemented = false;

	switch (SUP_CTL_CODE_NO_SIZE(opcode)) {

	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_COOKIE):               ioctl(*(SUPCOOKIE        *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_QUERY_FUNCS()):        ioctl(*(SUPQUERYFUNCS    *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GIP_MAP):              ioctl(*(SUPGIPMAP        *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_VT_CAPS):              ioctl(*(SUPVTCAPS        *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CALL_VMMR0_NO_SIZE()): ioctl(*(SUPCALLVMMR0     *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GET_HWVIRT_MSRS):      ioctl(*(SUPGETHWVIRTMSRS *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_UCODE_REV):            ioctl(*(SUPUCODEREV      *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GET_PAGING_MODE):      ioctl(*(SUPGETPAGINGMODE *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_ALLOC_EX):        ioctl(*(SUPPAGEALLOCEX   *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_SET_VM_FOR_FAST):      ioctl(*(SUPSETVMFORFAST  *)req); break;
	case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_FREE):            ioctl(*(SUPPAGEFREE      *)req); break;

	default:

		/*
		 * Ioctl not handled, print diagnostic info and spin.
		 * opcode number in lowest 7 bits
		 */
		error(__func__, " function=", opcode & 0x7f);
		not_implemented = true;
		STOP
	}

	if (((SUPREQHDR *)req)->rc == VERR_NOT_IMPLEMENTED)
		not_implemented = true;

	return not_implemented ? VERR_NOT_IMPLEMENTED : VINF_SUCCESS;
}


int suplibOsIOCtlFast(PSUPLIBDATA pThis, uintptr_t uFunction,
                      uintptr_t idCpu) STOP


int suplibOsPageAlloc(PSUPLIBDATA pThis, size_t cPages, void **ppvPages) STOP


int suplibOsPageFree(PSUPLIBDATA pThis, void *pvPages, size_t cPages) STOP

