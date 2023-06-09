/*
 * \brief  Genode specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>
#include <base/semaphore.h>
#include <base/sleep.h>
#include <util/flex_iterator.h>
#include <rom_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <trace/timestamp.h>
#include <util/bit_allocator.h>
#include <util/retry.h>

/* Genode's VirtualBox includes */
#include "EMInternal.h" /* enable access to em.s.* */
#include "vmm.h"
#include "vcpu.h"
#include "vcpu_svm.h"
#include "vcpu_vmx.h"

/* libc memory allocator */
#include <internal/mem_alloc.h>

/* Genode libc pthread binding */
#include <internal/thread_create.h>

/* libc */
#include <pthread.h>
#include <sched.h> /* sched_yield */

#include "mm.h"

/* VirtualBox includes */
#include <iprt/uint128.h>
#include <iprt/timer.h>

extern "C" void PGMUnmapMemoryGenode(void *, RTGCPHYS, ::size_t);
extern "C" void PGMFlushVMMemory();

enum { VERBOSE_PGM = false };

/*
 * Tracking required to fulfill VMM allocation requests of VM memory
 */
enum {
	CHUNKID_PAGE_START = 1,
	CHUNKID_PAGE_END   = 2,
	CHUNKID_START      = CHUNKID_PAGE_END + 1,

	ONE_PAGE_SIZE      = 4096,
	PAGES_SUPERPAGE    = GMM_CHUNK_SIZE / ONE_PAGE_SIZE,
	HANDY_PAGES        = PAGES_SUPERPAGE * (CHUNKID_PAGE_END - CHUNKID_PAGE_START + 1),

	MAX_VM_MEMORY      = 16ULL * 1024 * 1024 * 1024, /* 16 Gb */
	MAX_CHUNK_IDS      = MAX_VM_MEMORY / GMM_CHUNK_SIZE,
};

typedef Genode::Bit_allocator<HANDY_PAGES>   Page_ids;
typedef Genode::Bit_array<PAGES_SUPERPAGE>   Free_ids;

class Chunk_ids: public Genode::Bit_allocator<MAX_CHUNK_IDS>
{
	public:

		void reserve(Genode::addr_t bit_start, size_t const num) {
			_reserve(bit_start, num); };
};

static Page_ids  page_ids;
static Chunk_ids chunk_ids;

struct Region : Genode::List<Region>::Element
{
	Genode::uint64_t vmm_local;
	Genode::uint64_t size;
	Genode::Ram_dataspace_capability cap;

	Region(uint64_t gp, uint64_t gs, Genode::Ram_dataspace_capability c)
	: vmm_local(gp), size(gs), cap(c) { }

	bool contains(Genode::uint64_t p)
	{
		return (vmm_local <= p) && (p < vmm_local + size);
	}
};

static Genode::List<Region> regions;

static Genode::Allocator & heap()
{
	static Genode::Heap heap(genode_env().ram(), genode_env().rm());
	return heap;
}


static Sub_rm_connection &vm_memory(Genode::uint64_t vm_size = 0)
{
	/* memory used by the VM in any order as the VMM asks for allocations */
	static Sub_rm_connection vm_memory(genode_env(), vm_size);

	if (!vm_size)
		return vm_memory;

	using namespace Genode;

	/* create iterator for aligned allocation and attachment of memory */
	addr_t const vmm_local = vm_memory.local_addr(0);
	Flexpage_iterator fli(vmm_local, vm_size, 0, ~0UL, 0);

	/* start iteration */
	Flexpage memory = fli.page();
	while (memory.valid()) {
		addr_t const memory_size = 1UL << memory.log2_order;
		addr_t allocated  = 0;

		addr_t alloc_size = 128 * 1024 * 1024;
		if (alloc_size > memory_size)
			alloc_size = memory_size;

		while (allocated < memory_size) {
			Ram_dataspace_capability ds = genode_env().ram().alloc(alloc_size);

			addr_t to = vm_memory.attach_rwx(ds, memory.addr + allocated - vmm_local,
			                                 alloc_size);
			Assert(to == vm_memory.local_addr(memory.addr + allocated - vmm_local));
			allocated += alloc_size;

			regions.insert(new (heap()) Region(to, alloc_size, ds));

			if (memory_size - allocated < alloc_size)
				alloc_size = memory_size - allocated;
		}

		/* request next aligned memory range to be allocated and attached */
		memory = fli.page();
	}

	/* reserve chunkids which are special or unused */
	chunk_ids.reserve(0, CHUNKID_START);
	addr_t const unused_id = CHUNKID_START + vm_size / GMM_CHUNK_SIZE;
	addr_t const unused_count = MAX_CHUNK_IDS - unused_id - 1;
	chunk_ids.reserve(unused_id, unused_count);

	return vm_memory;
}


static Genode::Vm_connection &vm_connection(long prio = 0)
{
	static Genode::Vm_connection vm_session(genode_env(), "VBox vCPUs", prio);
	return vm_session;
}


/*
 * VCPU handling
 */

static Genode::List<Vcpu_handler> &vcpu_handler_list()
{
	static Genode::List<Vcpu_handler> _inst;
	return _inst;
}


static Vcpu_handler *lookup_vcpu_handler(unsigned int cpu_id)
{
	for (Vcpu_handler *vcpu_handler = vcpu_handler_list().first();
	     vcpu_handler;
	     vcpu_handler = vcpu_handler->next())
		if (vcpu_handler->cpu_id() == cpu_id)
			return vcpu_handler;

	return 0;
}


HRESULT genode_setup_machine(ComObjPtr<Machine> machine)
{
	ULONG memory_vbox;
	HRESULT rc = machine->COMGETTER(MemorySize)(&memory_vbox);
	if (FAILED(rc))
		return rc;

	/*
	 * Extra memory because of:
	 * - first chunkid (0) can't be used (VBox don't like chunkid 0)
	 * - second chunkid (1..2) is reserved for handy pages allocation
	 * - another chunkid is used additional for handy pages but as large page
	 */
	size_t const vmm_memory = 1024ULL * 1024 * (memory_vbox + 16) +
	                          (CHUNKID_START + 1) * GMM_CHUNK_SIZE;
	HRESULT ret = genode_check_memory_config(machine, vmm_memory);
	if (ret == VINF_SUCCESS)
		vm_memory(vmm_memory);

	return ret;
};


/* Genode specific function */

Genode::Xml_node platform_rom()
{
	static Genode::Attached_rom_dataspace const platform(genode_env(),
	                                                     "platform_info");
	return platform.xml().sub_node("hardware");
}


void SUPR3QueryHWACCLonGenodeSupport(VM * pVM)
{
	try {
		Genode::Xml_node const features = platform_rom().sub_node("features");
		pVM->hm.s.svm.fSupported = features.attribute_value("svm", false);
		pVM->hm.s.vmx.fSupported = features.attribute_value("vmx", false);

		if (pVM->hm.s.svm.fSupported || pVM->hm.s.vmx.fSupported) {
			Genode::log("Using ", pVM->hm.s.svm.fSupported ? "SVM" : "VMX",
			            " virtualization extension.");
			return;
		}
	} catch (...) { /* if we get an exception let hardware support off */ }

	Genode::warning("No virtualization hardware acceleration available");
}


/* VirtualBox SUPLib interface */
int SUPR3QueryVTxSupported(void) { return VINF_SUCCESS; }


int SUPR3CallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation, VMCPUID idCpu)
{
	switch (uOperation) {
	case SUP_VMMR0_DO_HM_RUN:
		Vcpu_handler *vcpu_handler = lookup_vcpu_handler(idCpu);
		Assert(vcpu_handler);
		return vcpu_handler->run_hw(pVMR0);
	}
	return VERR_INTERNAL_ERROR;
}

int SUPR3PageAllocEx(::size_t cPages, uint32_t fFlags, void **ppvPages,
                     PRTR0PTR pR0Ptr, PSUPPAGE paPages)
{
	Assert(ppvPages);
	Assert(!fFlags);

	using Genode::Attached_ram_dataspace;
	Attached_ram_dataspace * ds = new (heap()) Attached_ram_dataspace(genode_env().ram(),
	                                                                  genode_env().rm(),
	                                                                  cPages * ONE_PAGE_SIZE);

	Genode::addr_t const vmm_local = reinterpret_cast<Genode::addr_t>(ds->local_addr<void>());

	regions.insert(new (heap()) Region(vmm_local, cPages * ONE_PAGE_SIZE, ds->cap()));

	*ppvPages = ds->local_addr<void>();
	if (pR0Ptr)
		*pR0Ptr = vmm_local;

	if (!paPages)
		return VINF_SUCCESS;

	for (unsigned iPage = 0; iPage < cPages; iPage++)
	{
		paPages[iPage].uReserved = 0;
		paPages[iPage].Phys = vmm_local + iPage * ONE_PAGE_SIZE;
	}

	return VINF_SUCCESS;
}

enum { MAX_TRACKING = 4 };
static struct {
	Free_ids free;
	unsigned freed;
	unsigned chunkid;
} track_free[MAX_TRACKING];

static void partial_free_large_page(unsigned chunkid, unsigned page_id)
{
	unsigned pos = 0;

	/* lookup if already exist */
	for (; pos < MAX_TRACKING; pos++)
	{
		if (track_free[pos].chunkid == chunkid)
			break;
	}

	/* if not exist find free place */
	if (pos >= MAX_TRACKING) {
		for (int i = 0; i < MAX_TRACKING; i++) {
			if (track_free[i].chunkid)
				continue;

			track_free[i].chunkid = chunkid;
			track_free[i].freed   = 0;
			pos = i;
			break;
		}

		/* too many chunkids in use ? */
		Assert (pos < MAX_TRACKING);
		if (pos >= MAX_TRACKING)
			return;
	}

	try {
		/* mark as in use */
		track_free[pos].free.set(page_id, 1);
		track_free[pos].freed += 1;

		if (track_free[pos].freed >= 512) {
			/* slow ? optimize ? XXX */
			for (unsigned i = 0; i < 512; i++) {
				if (!track_free[pos].free.get(i, 1))
					throw 1;
				track_free[pos].free.clear(i, 1);
			}

			track_free[pos].chunkid = 0;
			track_free[pos].freed   = 0;

			chunk_ids.free(chunkid);
		}
	} catch (...) {
		Genode::error(__func__," ", __LINE__, " allocation failed ", pos, ":",
		              chunkid, ":", page_id);
		throw;
	}
}

int SUPR3CallVMMR0Ex(PVMR0 pVMR0, VMCPUID idCpu, unsigned uOperation,
                     uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{

	switch (uOperation) {

	case VMMR0_DO_GVMM_CREATE_VM:
	{
		genode_VMMR0_DO_GVMM_CREATE_VM(pReqHdr);

		GVMMCREATEVMREQ &req = reinterpret_cast<GVMMCREATEVMREQ &>(*pReqHdr);
		SUPR3QueryHWACCLonGenodeSupport(reinterpret_cast<VM *>(req.pVMR3));

		return VINF_SUCCESS;
	}
	case VMMR0_DO_GVMM_REGISTER_VMCPU:
		genode_VMMR0_DO_GVMM_REGISTER_VMCPU(pVMR0, idCpu);
		return VINF_SUCCESS;

	case VMMR0_DO_GVMM_SCHED_HALT:
	{
		uint64_t const u64NowGip = RTTimeNanoTS();
		uint64_t const ns_diff = u64Arg > u64NowGip ? u64Arg - u64NowGip : 0;

		if (!ns_diff)
			return VINF_SUCCESS;

		using namespace Genode;

		if (ns_diff > RT_NS_1SEC)
			warning(" more than 1 sec vcpu halt ", ns_diff, " ns");

		::Vcpu_handler *vcpu_handler = lookup_vcpu_handler(idCpu);
		Assert(vcpu_handler);
		vcpu_handler->halt(ns_diff);

		return VINF_SUCCESS;
	}

	case VMMR0_DO_GVMM_SCHED_WAKE_UP:
	{
		Vcpu_handler *vcpu_handler = lookup_vcpu_handler(idCpu);
		Assert(vcpu_handler);

		vcpu_handler->wake_up();
		return VINF_SUCCESS;
	}

	/* called by 'vmR3HaltGlobal1Halt' */
	case VMMR0_DO_GVMM_SCHED_POLL:
		return VINF_SUCCESS;

	case VMMR0_DO_VMMR0_INIT:
		return VINF_SUCCESS;

	case VMMR0_DO_GVMM_DESTROY_VM:
	case VMMR0_DO_VMMR0_TERM:
	case VMMR0_DO_HM_SETUP_VM:
		return VINF_SUCCESS;

	case VMMR0_DO_HM_ENABLE:
		return VINF_SUCCESS;

	case VMMR0_DO_GVMM_SCHED_POKE:
	{
		PVM pVM = reinterpret_cast<PVM>(pVMR0);
		Vcpu_handler *vcpu_handler = lookup_vcpu_handler(idCpu);
		Assert(vcpu_handler);
		if (vcpu_handler)
			vcpu_handler->recall(pVM);
		return VINF_SUCCESS;
	}
	case VMMR0_DO_GMM_ALLOCATE_PAGES:
	{
		/*
		 * VMM is asking for some host virtual memory pages without
		 * allocating the backing store actually. The backing store allocation
		 * takes place via VMMR0_DO_GMM_MAP_UNMAP_CHUNK. The assignment of the
		 * guest addresses is known at this point.
		 */

		static_assert(PGM_HANDY_PAGES * ONE_PAGE_SIZE == GMM_CHUNK_SIZE,
		              "Don't do that - you're going to waste tons of memory");
		Assert(pReqHdr->u32Magic == SUPVMMR0REQHDR_MAGIC);

		/* XXX VMM/VMMR0/GMMR0.cpp check there XXX */

		PGMMALLOCATEPAGESREQ req = reinterpret_cast<PGMMALLOCATEPAGESREQ>(pReqHdr);

		for (unsigned i = 0; i < req->cPages; i++) {
			RTHCPHYS guest_addr = req->aPages[i].HCPhysGCPhys;
			unsigned long page_idx = 0;
			unsigned long chunk_id = 0;

			try {
				page_idx = page_ids.alloc();
				chunk_id = CHUNKID_PAGE_START + page_idx / PAGES_SUPERPAGE;
			} catch (...) {
				Genode::error(__func__," ", __LINE__, " allocation failed");
				throw;
			}

			Assert (page_idx <= GMM_PAGEID_IDX_MASK);

			req->aPages[i].idPage = (chunk_id << GMM_CHUNKID_SHIFT) | page_idx;
			req->aPages[i].HCPhysGCPhys = vm_memory().local_addr((chunk_id * GMM_CHUNK_SIZE) | (page_idx * ONE_PAGE_SIZE));
			Assert(vm_memory().contains(req->aPages[i].HCPhysGCPhys));

			#if 0
			Genode::log("cPages ", Genode::Hex(req->cPages), " "
			            "chunkID=", req->aPages[i].idPage >> GMM_CHUNKID_SHIFT, " "
			            "pageIDX=", req->aPages[i].idPage & GMM_PAGEID_IDX_MASK, " "
			            "idPage=", Genode::Hex(req->aPages[i].idPage), " "
			            "GCPhys=", Genode::Hex(guest_addr), " "
			            "HCPhys=", Genode::Hex(req->aPages[i].HCPhysGCPhys), " "
			            "(", Genode::Hex(chunk_id * GMM_CHUNK_SIZE), " "
			            "| ", Genode::Hex(page_idx * ONE_PAGE_SIZE), ") pageidx=", page_idx, " "
			            "start_vm=", vm_memory().local_addr(0));
			#endif

		}

		return VINF_SUCCESS;
	}
	case VMMR0_DO_GMM_MAP_UNMAP_CHUNK:
	{
		PGMMMAPUNMAPCHUNKREQ req = reinterpret_cast<PGMMMAPUNMAPCHUNKREQ>(pReqHdr);

		Assert(pReqHdr->u32Magic == SUPVMMR0REQHDR_MAGIC);
		Assert(req->idChunkUnmap == NIL_GMM_CHUNKID);
		Assert(req->idChunkMap   != NIL_GMM_CHUNKID);

		Genode::addr_t local_addr_offset = (uintptr_t)req->idChunkMap << GMM_CHUNK_SHIFT;
		Genode::addr_t to = vm_memory().local_addr(local_addr_offset);

		req->pvR3 = reinterpret_cast<RTR3PTR>(to);

		return VINF_SUCCESS;
	}
	case VMMR0_DO_GMM_QUERY_MEM_STATS:
	{
		PGMMMEMSTATSREQ req = reinterpret_cast<PGMMMEMSTATSREQ>(pReqHdr);
		req->cAllocPages = 0;
		req->cMaxPages = 0;
		req->cBalloonedPages = 0;
		return VINF_SUCCESS;
	}
	case VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES:
	{
		/*
		 * VMM is asking for some host virtual memory pages without
		 * allocating the backing store actually. The backing store allocation
		 * takes place via VMMR0_DO_GMM_MAP_UNMAP_CHUNK. The assignment of the
		 * guest addresses to these host pages is unknown at this point.
		 */

		PVM pVM = reinterpret_cast<PVM>(pVMR0);

		/* based on PGMR0PhysAllocateHandyPages() in VMM/VMMR0/PGMR0.cpp - start */
		uint32_t iFirst = pVM->pgm.s.cHandyPages;
		uint32_t cPages = RT_ELEMENTS(pVM->pgm.s.aHandyPages) - iFirst;
		uint32_t cPagesToUpdate = cPages;
		uint32_t cPagesToAlloc  = cPages;
		/* based on PGMR0PhysAllocateHandyPages() in VMM/VMMR0/PGMR0.cpp - end */

		/* based on GMMR0AllocateHandyPages in VMM/VMMR0/GMMR0.cpp - start */
		unsigned iPage = 0;
		for (; iPage < cPagesToUpdate; iPage++)
		{
			AssertMsgReturn(    (    pVM->pgm.s.aHandyPages[iFirst + iPage].HCPhysGCPhys <= GMM_GCPHYS_LAST
			                     && !(pVM->pgm.s.aHandyPages[iFirst + iPage].HCPhysGCPhys & PAGE_OFFSET_MASK))
			                ||  pVM->pgm.s.aHandyPages[iFirst + iPage].HCPhysGCPhys == NIL_RTHCPHYS
			                ||  pVM->pgm.s.aHandyPages[iFirst + iPage].HCPhysGCPhys == GMM_GCPHYS_UNSHAREABLE,
			                ("#%#x: %RHp\n", iFirst + iPage, pVM->pgm.s.aHandyPages[iFirst + iPage].HCPhysGCPhys),
			                VERR_INVALID_PARAMETER);
			AssertMsgReturn(    pVM->pgm.s.aHandyPages[iFirst + iPage].idPage <= GMM_PAGEID_LAST
			                /*||  pVM->pgm.s.aHandyPages[iFirst + iPage].idPage == NIL_GMM_PAGEID*/,
			                ("#%#x: %#x\n", iFirst + iPage, pVM->pgm.s.aHandyPages[iFirst + iPage].idPage), VERR_INVALID_PARAMETER);
			AssertMsgReturn(    pVM->pgm.s.aHandyPages[iFirst + iPage].idPage <= GMM_PAGEID_LAST
			                /*||  pVM->pgm.s.aHandyPages[iFirst + iPage].idSharedPage == NIL_GMM_PAGEID*/,
			                ("#%#x: %#x\n", iFirst + iPage, pVM->pgm.s.aHandyPages[iFirst + iPage].idSharedPage), VERR_INVALID_PARAMETER);
		}

		for (; iPage < cPagesToAlloc; iPage++)
		{
			AssertMsgReturn(pVM->pgm.s.aHandyPages[iFirst + iPage].HCPhysGCPhys == NIL_RTHCPHYS,   ("#%#x: %RHp\n", iFirst + iPage, pVM->pgm.s.aHandyPages[iFirst + iPage].HCPhysGCPhys), VERR_INVALID_PARAMETER);
			AssertMsgReturn(pVM->pgm.s.aHandyPages[iFirst + iPage].idPage       == NIL_GMM_PAGEID, ("#%#x: %#x\n", iFirst + iPage, pVM->pgm.s.aHandyPages[iFirst + iPage].idPage),        VERR_INVALID_PARAMETER);
			AssertMsgReturn(pVM->pgm.s.aHandyPages[iFirst + iPage].idSharedPage == NIL_GMM_PAGEID, ("#%#x: %#x\n", iFirst + iPage, pVM->pgm.s.aHandyPages[iFirst + iPage].idSharedPage),  VERR_INVALID_PARAMETER);
		}

		Genode::uint64_t chunkid = 0;

		try {
			chunkid = chunk_ids.alloc();
		} catch (...) {
			Genode::error(__func__," ", __LINE__, " allocation failed");
			throw;
		}

		if (cPagesToAlloc != GMM_CHUNK_SIZE / ONE_PAGE_SIZE)
			Genode::log("special chunkid=", chunkid, " "
			            "toupdate=", cPagesToUpdate, " "
			            "toalloc=", cPagesToAlloc, " "
			            "virt=", Genode::Hex(vm_memory().local_addr(chunkid << GMM_CHUNK_SHIFT)));

		for (unsigned i = 0; i < cPagesToUpdate; i++) {
			if (pVM->pgm.s.aHandyPages[iFirst + i].idPage != NIL_GMM_PAGEID)
			{
				pVM->pgm.s.aHandyPages[iFirst + i].idPage = NIL_GMM_PAGEID;
				pVM->pgm.s.aHandyPages[iFirst + i].HCPhysGCPhys = NIL_RTHCPHYS;
			}

			if (pVM->pgm.s.aHandyPages[iFirst + i].idSharedPage != NIL_GMM_PAGEID)
				AssertMsgReturn(false, ("%s %u - not implemented", __func__, __LINE__), VERR_GENERAL_FAILURE);
		}

		for (unsigned i = 0; i < cPagesToAlloc; i++)
		{
			Assert(pVM->pgm.s.aHandyPages[iFirst + i].HCPhysGCPhys  == NIL_RTHCPHYS);
			Assert(pVM->pgm.s.aHandyPages[iFirst + i].idPage        == NIL_GMM_PAGEID);
			Assert(pVM->pgm.s.aHandyPages[iFirst + i].idSharedPage  == NIL_GMM_PAGEID);
		}

		for (unsigned i = 0; i < cPagesToUpdate; i++) {
			unsigned reverse = i; //cPagesToUpdate - 1 - i;
			Assert (pVM->pgm.s.aHandyPages[iFirst + i].HCPhysGCPhys == NIL_RTHCPHYS);
			{
				pVM->pgm.s.aHandyPages[iFirst + i].idPage = (chunkid << GMM_CHUNKID_SHIFT) | (iFirst + reverse);
				pVM->pgm.s.aHandyPages[iFirst + i].idSharedPage = NIL_GMM_PAGEID;

				pVM->pgm.s.aHandyPages[iFirst + i].HCPhysGCPhys = vm_memory().local_addr((chunkid << GMM_CHUNK_SHIFT) | ((iFirst + reverse) * ONE_PAGE_SIZE));
			}
		}
		/* based on GMMR0AllocateHandyPages in VMM/VMMR0/GMMR0.cpp - end */

		/* based on PGMR0PhysAllocateHandyPages() in VMM/VMMR0/PGMR0.cpp - start */
		pVM->pgm.s.cHandyPages = RT_ELEMENTS(pVM->pgm.s.aHandyPages);

		for (uint32_t i = 0; i < RT_ELEMENTS(pVM->pgm.s.aHandyPages); i++)
		{
			Assert(pVM->pgm.s.aHandyPages[i].idPage != NIL_GMM_PAGEID);
			Assert(pVM->pgm.s.aHandyPages[i].idPage <= GMM_PAGEID_LAST);
			Assert(pVM->pgm.s.aHandyPages[i].idSharedPage == NIL_GMM_PAGEID);
			Assert(pVM->pgm.s.aHandyPages[i].HCPhysGCPhys != NIL_RTHCPHYS);
			Assert(!(pVM->pgm.s.aHandyPages[i].HCPhysGCPhys & ~X86_PTE_PAE_PG_MASK));
		}
		/* based on PGMR0PhysAllocateHandyPages() in VMM/VMMR0/PGMR0.cpp - end */

		return VINF_SUCCESS;
	}
	case VMMR0_DO_PGM_ALLOCATE_LARGE_HANDY_PAGE:
	{
		/*
		 * VMM is asking for some host large virtual memory pages without
		 * allocating the backing store actually. The backing store allocation
		 * takes place via VMMR0_DO_GMM_MAP_UNMAP_CHUNK. The assignment of the
		 * guest addresses to these host pages is unknown at this point.
		 */

		PVM pVM = reinterpret_cast<PVM>(pVMR0);

		Assert(pVM);
		Assert(pVM->pgm.s.cLargeHandyPages == 0);

		try {
			Genode::uint64_t chunkid = chunk_ids.alloc();

			pVM->pgm.s.aLargeHandyPage[0].idPage = (chunkid << GMM_CHUNKID_SHIFT);
			pVM->pgm.s.aLargeHandyPage[0].HCPhysGCPhys = vm_memory().local_addr(chunkid << GMM_CHUNK_SHIFT);

			pVM->pgm.s.cLargeHandyPages = 1;
		} catch (...) {
			Genode::error(__func__," ", __LINE__, " allocation failed");
			throw;
		}

		return VINF_SUCCESS;
	}
	case VMMR0_DO_GMM_BALLOONED_PAGES:
	case VMMR0_DO_GMM_RESET_SHARED_MODULES:
	case VMMR0_DO_PGM_FLUSH_HANDY_PAGES:
	{
		PVM    const pVM   = reinterpret_cast<PVM>(pVMR0);
		PVMCPU const pVCpu = &pVM->aCpus[idCpu];

		/* if not in VM shutdown - complain - bug ahead */
		if (pVCpu->em.s.enmState != EMSTATE_TERMINATING)
			Genode::error("unexpected call of type ", uOperation, ", "
			              "em state=", (int)pVCpu->em.s.enmState);

		return VINF_SUCCESS;
	}
	case VMMR0_DO_GMM_FREE_PAGES:
	{
		if (u64Arg)
			return VERR_INVALID_PARAMETER;

		PVM pVM = reinterpret_cast<PVM>(pVMR0);
		PGMMFREEPAGESREQ pReq = reinterpret_cast<PGMMFREEPAGESREQ>(pReqHdr);

		AssertPtrReturn(pVM, VERR_INVALID_POINTER);
		AssertPtrReturn(pReq, VERR_INVALID_POINTER);
		AssertMsgReturn(pReq->Hdr.cbReq >= RT_UOFFSETOF(GMMFREEPAGESREQ, aPages[0]),
		                ("%#x < %#x\n", pReq->Hdr.cbReq, RT_UOFFSETOF(GMMFREEPAGESREQ, aPages[0])),
		                VERR_INVALID_PARAMETER);
		AssertMsgReturn(pReq->Hdr.cbReq == RT_UOFFSETOF(GMMFREEPAGESREQ, aPages[pReq->cPages]),
		                ("%#x != %#x\n", pReq->Hdr.cbReq, RT_UOFFSETOF(GMMFREEPAGESREQ, aPages[pReq->cPages])),
		                VERR_INVALID_PARAMETER);

		uint32_t cPages = pReq->cPages;
		PGMMFREEPAGEDESC paPages = &pReq->aPages[0];
		GMMACCOUNT enmAccount = pReq->enmAccount;

		AssertPtrReturn(paPages, VERR_INVALID_PARAMETER);
		AssertMsgReturn(enmAccount > GMMACCOUNT_INVALID && enmAccount < GMMACCOUNT_END, ("%d\n", enmAccount), VERR_INVALID_PARAMETER);
		AssertMsgReturn(cPages > 0 && cPages < RT_BIT(32 - PAGE_SHIFT), ("%#x\n", cPages), VERR_INVALID_PARAMETER);

		for (unsigned iPage = 0; iPage < cPages; iPage++)
			AssertMsgReturn(    paPages[iPage].idPage <= GMM_PAGEID_LAST
			                /*||  paPages[iPage].idPage == NIL_GMM_PAGEID*/,
			                ("#%#x: %#x\n", iPage, paPages[iPage].idPage), VERR_INVALID_PARAMETER);

		for (uint32_t last_chunk = ~0U, iPage = 0; iPage < cPages; iPage++)
		{
			uint32_t const idPage   = paPages[iPage].idPage;
			uint32_t const page_idx = idPage & GMM_PAGEID_IDX_MASK;
			uint32_t const chunkid  = idPage >> GMM_CHUNKID_SHIFT;

			if (last_chunk != chunkid) {
				/* revoke mapping from guest VM */
				PGMUnmapMemoryGenode(nullptr, (0UL + chunkid) << GMM_CHUNK_SHIFT, GMM_CHUNK_SIZE);

				last_chunk = chunkid;
			}

			if (CHUNKID_PAGE_START <= chunkid && chunkid <= CHUNKID_PAGE_END) {
				try {
					page_ids.free((chunkid - CHUNKID_PAGE_START) * PAGES_SUPERPAGE + page_idx);
				} catch (...) {
					Genode::error(__func__," ", __LINE__, " clearing failed");
					throw;
				}
			}

			partial_free_large_page(chunkid, page_idx);

			paPages[iPage].idPage = NIL_GMM_PAGEID;
		}

		return VINF_SUCCESS;
	}
	case VMMR0_DO_GMM_INITIAL_RESERVATION:
		return VINF_SUCCESS;
	case VMMR0_DO_GMM_UPDATE_RESERVATION:
		return VINF_SUCCESS;
	default:
		Genode::error("SUPR3CallVMMR0Ex: unhandled uOperation ", uOperation,
		              " ", (int)VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES, " ",
		              (int)VMMR0_DO_GMM_QUERY_MEM_STATS);
		return VERR_GENERAL_FAILURE;
	}
}


/**
 * Various support stuff.
 */
uint64_t genode_cpu_hz()
{
	static uint64_t cpu_freq = 0;

	if (!cpu_freq) {
		try {
			platform_rom().with_optional_sub_node("tsc", [&] (Genode::Xml_node const &tsc) {
				cpu_freq = tsc.attribute_value("freq_khz", cpu_freq); });
			cpu_freq *= 1000ULL;
		} catch (...) { }

		if (cpu_freq == 0) {
			Genode::error("could not read out CPU frequency");
			Genode::sleep_forever();
		}
	}

	return cpu_freq;
}


void PGMUnmapMemoryGenode(void *, RTGCPHYS guest_phys, ::size_t size)
{
	vm_connection().detach(guest_phys, size);
}

extern "C" void PGMFlushVMMemory()
{
	/* XXX PGMUnmapMemoryGenode on vm_session does not flush all caps */
	PGMUnmapMemoryGenode(nullptr, 0, MAX_VM_MEMORY);
}


extern "C" int sched_yield(void)
{
	static unsigned long counter = 0;

	if (++counter % 50000 == 0)
		Genode::warning(__func__, " called ", counter, " times");

	return -1;
}


bool create_emt_vcpu(pthread_t * thread, ::size_t stack_size,
                     void *(*start_routine)(void *), void *arg,
                     Genode::Cpu_connection * cpu_connection,
                     Genode::Affinity::Location location,
                     unsigned int cpu_id, const char * name, long prio)
{
	Genode::Xml_node const features = platform_rom().sub_node("features");
	bool const svm = features.attribute_value("svm", false);
	bool const vmx = features.attribute_value("vmx", false);

	if (!svm && !vmx)
		return false;

	Vcpu_handler *vcpu_handler = 0;

	if (vmx)
		vcpu_handler = new (heap()) Vcpu_handler_vmx(genode_env(),
		                                           stack_size,
		                                           location,
		                                           cpu_id,
		                                           vm_connection(prio),
		                                           heap());

	if (svm)
		vcpu_handler = new (heap()) Vcpu_handler_svm(genode_env(),
		                                           stack_size,
		                                           location,
		                                           cpu_id,
		                                           vm_connection(prio),
		                                           heap());

	vcpu_handler_list().insert(vcpu_handler);

	Libc::pthread_create_from_session(thread, start_routine, arg,
	                                  stack_size, name, cpu_connection, location);

	return true;
}

static int _map_memory(Genode::Vm_connection &vm_session,
                       RTGCPHYS const GCPhys,
                       RTHCPHYS const vmm_local,
                       size_t const mapping_size,
                       bool writeable)
{
	for (Region *region = regions.first(); region; region = region->next())
	{
		if (!region->contains(vmm_local))
			continue;

		bool retry = false;

		do {
			Genode::addr_t const offset = vmm_local - region->vmm_local;

			try {
				vm_session.with_upgrade([&]() {
					vm_session.attach(region->cap, GCPhys,
					                  { .offset     = offset,
					                    .size       = mapping_size,
					                    .executable = true,
					                    .writeable  = writeable });
				});
			} catch (Genode::Vm_session::Region_conflict) {
				/* XXX PGMUnmapMemoryGenode on vm_session does not flush caps */
				vm_session.detach(GCPhys, mapping_size);
				if (retry) {
					Genode::log("region conflict - ", Genode::Hex(GCPhys),
					            " ", Genode::Hex(mapping_size), " vmm_local=",
					            Genode::Hex(vmm_local), " ", region->cap,
					            " region=", Genode::Hex(region->vmm_local),
					            "+", Genode::Hex(region->size));

					size_t detach_size = mapping_size;
					while (detach_size) {
						size_t const size = 4096;
						vm_session.detach(GCPhys + (mapping_size - detach_size), size);
						detach_size -= detach_size > size ? size : detach_size;
					}

					return VERR_PGM_DYNMAP_FAILED;
				}

				if (!retry) {
					retry = true;
					continue;
				}
			}
			retry = false;
		} while (retry);

		return VINF_SUCCESS;
	}
	Genode::error(" no mapping ?");
	return VERR_PGM_DYNMAP_FAILED;
}

class Pgm_guard
{
	private:
		VM &_vm;

	public:
		Pgm_guard(VM &vm) : _vm(vm) { pgmLock(&_vm); }
		~Pgm_guard() { pgmUnlock(&_vm); }
};

#include "PGMInline.h"

Genode::Vm_connection::Exit_config const Vcpu_handler::_exit_config { /* ... */ };

int Vcpu_handler::map_memory(Genode::Vm_connection &vm_session,
                             RTGCPHYS const GCPhys, RTGCUINT vbox_fault_reason)
{
	_ept_fault_addr_type = PGMPAGETYPE_INVALID;

	PPGMRAMRANGE const pRam = pgmPhysGetRangeAtOrAbove(_vm, GCPhys);
	if (!pRam)
		return VERR_PGM_DYNMAP_FAILED;

	RTGCPHYS off = GCPhys - pRam->GCPhys;
	if (off >= pRam->cb)
		return VERR_PGM_DYNMAP_FAILED;

	unsigned iPage = off >> PAGE_SHIFT;
	PPGMPAGE pPage = &pRam->aPages[iPage];

	_ept_fault_addr_type = PGM_PAGE_GET_TYPE(pPage);

	/*
	 * If page is not allocated (== zero page) and no MMIO or active page, allocate and map it
	 * immediately. Important do not do this if A20 gate is disabled, A20 gate
	 * is handled by IEM/REM in this case.
	 */
	if (PGM_PAGE_IS_ZERO(pPage)
	    && !PGM_PAGE_IS_ALLOCATED(pPage)
	    && !PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage)
	    && !PGM_PAGE_IS_SPECIAL_ALIAS_MMIO(pPage)
	    && PGM_A20_IS_ENABLED(_vcpu))
	{
		Pgm_guard guard(*_vm);
		pgmPhysPageMakeWritable(_vm, pPage, GCPhys);
	}

	if (PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage) ||
	    PGM_PAGE_IS_SPECIAL_ALIAS_MMIO(pPage) ||
	    PGM_PAGE_IS_ZERO(pPage)) {

		if (PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO &&
		    !PGM_PAGE_IS_ZERO(pPage)) {

			Genode::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " ",
			            PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage), " ",
			            PGM_PAGE_IS_SPECIAL_ALIAS_MMIO(pPage), " ",
			            PGM_PAGE_IS_ZERO(pPage), " "
			            " vbox_fault_reason=", Genode::Hex(vbox_fault_reason));
			Genode::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " "
			            "host=", Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), " "
			            "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)), " "
			            "writeable=true "
			            "state=", Genode::Hex(PGM_PAGE_GET_STATE(pPage)));
		}
		return VERR_PGM_DYNMAP_FAILED;
	}

	if (!PGM_PAGE_IS_ALLOCATED(pPage))
		Genode::log("unknown page state ", Genode::Hex(PGM_PAGE_GET_STATE(pPage)),
		            " GCPhys=", Genode::Hex(GCPhys));
	Assert(PGM_PAGE_IS_ALLOCATED(pPage));

	if (PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_RAM   &&
	    PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO2 &&
	    PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_ROM)
	{
		if (VERBOSE_PGM)
			Genode::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " "
			            "vbox_fault_reason=", Genode::Hex(vbox_fault_reason), " "
			            "host=", Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), " "
			            "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)), " "
			            "state=", Genode::Hex(PGM_PAGE_GET_STATE(pPage)));
		return VERR_PGM_DYNMAP_FAILED;
	}

	Assert(!PGM_PAGE_IS_ZERO(pPage));

	/* write fault on a ROM region */
	if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_ROM &&
	    vbox_fault_reason & VMX_EXIT_QUALIFICATION_EPT_DATA_WRITE) {
		Genode::warning(__func__, " - write fault on ROM region!? gp=",
		                Genode::Hex(GCPhys));
		return VERR_PGM_DYNMAP_FAILED;
	}

	/* nothing should be mapped - otherwise we get endless overmap loops */
	Assert(!(vbox_fault_reason & VMX_EXIT_QUALIFICATION_EPT_ENTRY_PRESENT));

	bool const writeable = PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_ROM;

	PPGMPHYSHANDLER handler = pgmHandlerPhysicalLookup(_vm, GCPhys);

	if (VERBOSE_PGM && PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2 &&
	    !handler)
		Genode::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " ",
		            "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)), " "
		            "state=", Genode::Hex(PGM_PAGE_GET_STATE(pPage)), " "
		            "- MMIO2 w/o handler");

	if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2 && handler) {
		PFNPGMPHYSHANDLER pfnHandler = PGMPHYSHANDLER_GET_TYPE(_vm, handler)->CTX_SUFF(pfnHandler);
		if (!pfnHandler) {
			Genode::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " "
			            "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)));
			return VERR_PGM_DYNMAP_FAILED;
		}
		void *pvUser = handler->CTX_SUFF(pvUser);
		if (!pvUser) {
			Genode::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " "
			         "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)));
			return VERR_PGM_DYNMAP_FAILED;
		}

		PGMACCESSTYPE access_type = (vbox_fault_reason & VMX_EXIT_QUALIFICATION_EPT_DATA_WRITE) ? PGMACCESSTYPE_WRITE : PGMACCESSTYPE_READ;

		VBOXSTRICTRC rcStrict = pfnHandler(_vm, _vcpu, GCPhys, nullptr, nullptr, 0, access_type, PGMACCESSORIGIN_HM, pvUser);
		if (rcStrict != VINF_PGM_HANDLER_DO_DEFAULT) {
			Genode::log(__LINE__, " nodefault GCPhys=", Genode::Hex(GCPhys), " "
			         "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)), " "
			         "pfnHandler=", pfnHandler);
			return VERR_PGM_DYNMAP_FAILED;
		}
	}

/*
	if (VERBOSE_PGM)
		Genode::log(Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)),
		            "->", Genode::Hex(GCPhys),
		            " type=", PGM_PAGE_GET_TYPE(pPage),
		            " state=", PGM_PAGE_GET_STATE(pPage),
		            " pde_type=", PGM_PAGE_GET_PDE_TYPE(pPage),
		            PGM_PAGE_GET_PDE_TYPE(pPage) == PGM_PAGE_PDE_TYPE_PDE ? "(is pde)" : "(not pde)",
		            " iPage=", iPage,
		         " range_start=", Genode::Hex(pRam->GCPhys),
		         " range_size=", Genode::Hex(pRam->cb),
		         " pages=", pRam->cb >> PAGE_SHIFT
		);
*/

	if (PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE)
		return _map_memory(vm_session, GCPhys, PGM_PAGE_GET_HCPHYS(pPage), 4096, writeable); /* one page mapping */

	Genode::addr_t const superpage_log2 = 21;
	Genode::addr_t const max_pages = pRam->cb >> PAGE_SHIFT;
	Genode::addr_t const superpage_pages = (1UL << superpage_log2) / 4096;
	Genode::addr_t const mask  = (1UL << superpage_log2) - 1;
	Genode::addr_t const super_gcphys = GCPhys & ~mask;

	RTGCPHYS max_off = super_gcphys - pRam->GCPhys;
	if (max_off > pRam->cb)
		return _map_memory(vm_session, GCPhys, PGM_PAGE_GET_HCPHYS(pPage), 4096, writeable); /* one page mapping */

	Genode::addr_t const super_hcphys = PGM_PAGE_GET_HCPHYS(pPage) & ~mask;

	unsigned const i_s = max_off >> PAGE_SHIFT;

	if (i_s + superpage_pages > max_pages)
		return _map_memory(vm_session, GCPhys, PGM_PAGE_GET_HCPHYS(pPage), 4096, writeable); /* one page mapping */

	if (VERBOSE_PGM)
		Genode::log(Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), "->",
		            Genode::Hex(GCPhys), " - iPage ", iPage, " [",
		            i_s, ",", i_s + superpage_pages, ")", " "
		            "range_size=", Genode::Hex(pRam->cb));

	/* paranoia sanity checks */
	for (Genode::addr_t i = i_s; i < i_s + superpage_pages; i++) {
		PPGMPAGE page = &pRam->aPages[i];

		Genode::addr_t const gcpage = pRam->GCPhys + (i << PAGE_SHIFT);

		if (!(super_hcphys == (PGM_PAGE_GET_HCPHYS(page) & ~mask)) ||
		    !(super_gcphys == (gcpage & ~mask)) ||
		    !(PGM_PAGE_GET_PDE_TYPE(page) == PGM_PAGE_PDE_TYPE_PDE) ||
		    !(PGM_PAGE_GET_TYPE(page) == PGM_PAGE_GET_TYPE(pPage)) ||
		    !(PGM_PAGE_GET_STATE(page) == PGM_PAGE_GET_STATE(pPage)))
		{
			if (VERBOSE_PGM)
				Genode::error(Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), "->",
				              Genode::Hex(GCPhys), " - iPage ", iPage, " i ", i, " [",
				              i_s, ",", i_s + superpage_pages, ")", " "
				              "range_size=", Genode::Hex(pRam->cb), " "
				              "super_hcphys=", Genode::Hex(super_hcphys), "?=", Genode::Hex((PGM_PAGE_GET_HCPHYS(page) & ~mask)), " "
				              "super_gcphys=", Genode::Hex(super_gcphys), "?=", Genode::Hex((gcpage & ~mask)), " ",
				              (int)(PGM_PAGE_GET_PDE_TYPE(page)), "?=", (int)PGM_PAGE_PDE_TYPE_PDE, " ",
				              (int)(PGM_PAGE_GET_TYPE(page)), "?=", (int)PGM_PAGE_GET_TYPE(pPage), " ",
				              (int)(PGM_PAGE_GET_STATE(page)), "?=", (int)PGM_PAGE_GET_STATE(pPage));
			return _map_memory(vm_session, GCPhys, PGM_PAGE_GET_HCPHYS(pPage), 4096, writeable); /* one page mapping */
		}
	}

	/* XXX revoke of old mappings required ? */
	/* super page mapping */
	return _map_memory(vm_session, super_gcphys, super_hcphys, 1UL << superpage_log2, writeable);
}


Genode::uint64_t * Vcpu_handler::pdpte_map(VM *pVM, RTGCPHYS cr3)
{
	Pgm_guard guard(*_vm);

	PPGMRAMRANGE pRam = pgmPhysGetRangeAtOrAbove(pVM, cr3);
	Assert (pRam);

	RTGCPHYS off = cr3 - pRam->GCPhys;
	Assert (off < pRam->cb);

	unsigned iPage = off >> PAGE_SHIFT;
	PPGMPAGE pPage = &pRam->aPages[iPage];

/*
	if (VERBOSE_PGM)
		Genode::log(__LINE__, " gcphys=", Genode::Hex(cr3),
		         " host=", Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)),
		         " type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)),
		         " state=",Genode::Hex(PGM_PAGE_GET_STATE(pPage)));
*/

	Genode::uint64_t *pdpte = reinterpret_cast<Genode::uint64_t*>(PGM_PAGE_GET_HCPHYS(pPage) + (cr3 & PAGE_OFFSET_MASK));

	Assert(pdpte != 0);

	return pdpte;
}

static PFNRTTIMER rttimer_func = nullptr;
static void *     rttimer_obj  = nullptr;

enum {
	UPDATE_HZ  = 1000,
	UPDATE_US  = 1000 * 1000 / UPDATE_HZ,
	UPDATE_NS  = UPDATE_US * 1000,
};


PSUPGLOBALINFOPAGE g_pSUPGlobalInfoPage;


class Periodic_gip
{
	private :

		void update()
		{
			/**
			 * We're using rdtsc here since timer_session->elapsed_ms produces
			 * instable results when the timer service is using the Genode PIC
			 * driver.
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

			/* call the timer function of the RTTimerCreate call */
			if (rttimer_func) {
				Libc::with_libc([&] () {
					rttimer_func(nullptr, rttimer_obj, 0); });
			}
		}

	public:

		Timer::Connection               _timer;
		Genode::Signal_handler<Periodic_gip>   _timer_handler;

		Periodic_gip(Genode::Env &env)
		:
			_timer(env),
			_timer_handler(env.ep(), *this, &Periodic_gip::update)
		{
			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(UPDATE_US);
		}
};


struct Attached_gip : Genode::Attached_ram_dataspace
{
	Attached_gip()
	: Attached_ram_dataspace(genode_env().ram(), genode_env().rm(), PAGE_SIZE)
	{
		g_pSUPGlobalInfoPage = local_addr<SUPGLOBALINFOPAGE>();

		/* checked by TMR3Init */
		g_pSUPGlobalInfoPage->u32Version            = SUPGLOBALINFOPAGE_VERSION;
		g_pSUPGlobalInfoPage->u32Magic              = SUPGLOBALINFOPAGE_MAGIC;
		g_pSUPGlobalInfoPage->u32Mode               = SUPGIPMODE_SYNC_TSC;
		g_pSUPGlobalInfoPage->cCpus                 = 1;
		g_pSUPGlobalInfoPage->cPages                = 1;
		g_pSUPGlobalInfoPage->u32UpdateHz           = UPDATE_HZ;
		g_pSUPGlobalInfoPage->u32UpdateIntervalNS   = UPDATE_NS;
		g_pSUPGlobalInfoPage->cOnlineCpus           = 0;
		g_pSUPGlobalInfoPage->cPresentCpus          = 0;
		g_pSUPGlobalInfoPage->cPossibleCpus         = 0;
		g_pSUPGlobalInfoPage->idCpuMax              = 0;
		g_pSUPGlobalInfoPage->u64CpuHz              = genode_cpu_hz();
		/* evaluated by rtTimeNanoTSInternalRediscover in Runtime/common/time/timesup.cpp */
		g_pSUPGlobalInfoPage->fGetGipCpu            = SUPGIPGETCPU_APIC_ID;

		SUPGIPCPU *cpu = &g_pSUPGlobalInfoPage->aCPUs[0];

		cpu->u32TransactionId        = 0;
		cpu->u32UpdateIntervalTSC    = genode_cpu_hz() / UPDATE_HZ;
		cpu->u64NanoTS               = 0ULL;
		cpu->u64TSC                  = 0ULL;
		cpu->u64CpuHz                = genode_cpu_hz();
		cpu->cErrors                 = 0;
		cpu->iTSCHistoryHead         = 0;
		cpu->u32PrevUpdateIntervalNS = UPDATE_NS;
		cpu->enmState                = SUPGIPCPUSTATE_ONLINE;
		cpu->idCpu                   = 0;
		cpu->iCpuSet                 = 0;
		cpu->idApic                  = 0;

		/* schedule periodic call of GIP update function */
		static Periodic_gip periodic_gip(genode_env());
	}
};


int RTTimerCreate(PRTTIMER *pptimer, unsigned ms, PFNRTTIMER func, void *obj)
{
	if (pptimer)
		*pptimer = NULL;

	/* used solely at one place in TM.cpp */
	Assert(!rttimer_func);

	/*
	 * Ignore (10) ms which is too high for audio. Instead the callback
	 * handler will run at UPDATE_HZ rate.
	 */
	rttimer_func = func;
	rttimer_obj  = obj;

	return VINF_SUCCESS;
}


int RTTimerDestroy(PRTTIMER)
{
	rttimer_obj  = nullptr;
	rttimer_func = nullptr;
	return VINF_SUCCESS;
}


int SUPR3Init(PSUPDRVSESSION *ppSession)
{
	static Attached_gip gip;

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
