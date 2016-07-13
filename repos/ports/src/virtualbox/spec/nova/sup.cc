/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/semaphore.h>
#include <util/flex_iterator.h>
#include <rom_session/connection.h>
#include <timer_session/connection.h>
#include <os/attached_rom_dataspace.h>
#include <trace/timestamp.h>

#include <vmm/vcpu_thread.h>
#include <vmm/vcpu_dispatcher.h>

/* NOVA includes that come with Genode */
#include <nova/syscalls.h>

/* VirtualBox includes */
#include "HMInternal.h" /* enable access to hm.s.* */
#include "CPUMInternal.h" /* enable access to cpum.s.* */
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm_svm.h>
#include <VBox/err.h>

/* Genode's VirtualBox includes */
#include "vcpu.h"
#include "vcpu_svm.h"
#include "vcpu_vmx.h"

/* libc memory allocator */
#include <libc_mem_alloc.h>


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


/* Genode specific function */

static Genode::Attached_rom_dataspace hip_rom("hypervisor_info_page");

void SUPR3QueryHWACCLonGenodeSupport(VM * pVM)
{
	try {
		Nova::Hip * hip = hip_rom.local_addr<Nova::Hip>();

		pVM->hm.s.svm.fSupported = hip->has_feature_svm();
		pVM->hm.s.vmx.fSupported = hip->has_feature_vmx();

		if (hip->has_feature_svm() || hip->has_feature_vmx()) {
			Genode::log("Using ",
			            hip->has_feature_svm() ? "SVM" : "VMX", " "
			            "virtualization extension.");
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


int SUPR3CallVMMR0Ex(PVMR0 pVMR0, VMCPUID idCpu, unsigned
                     uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{
	switch (uOperation) {

	case VMMR0_DO_GVMM_CREATE_VM:
		genode_VMMR0_DO_GVMM_CREATE_VM(pReqHdr);
		return VINF_SUCCESS;

	case VMMR0_DO_GVMM_REGISTER_VMCPU:
		genode_VMMR0_DO_GVMM_REGISTER_VMCPU(pVMR0, idCpu);
		return VINF_SUCCESS;

	case VMMR0_DO_GVMM_SCHED_HALT:
	{
		Vcpu_handler *vcpu_handler = lookup_vcpu_handler(idCpu);
		Assert(vcpu_handler);
		vcpu_handler->halt();
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
		SUPR3QueryHWACCLonGenodeSupport(reinterpret_cast<VM *>(pVMR0));
		return VINF_SUCCESS;

	case VMMR0_DO_GVMM_DESTROY_VM:
	case VMMR0_DO_VMMR0_TERM:
	case VMMR0_DO_HM_SETUP_VM:
		return VINF_SUCCESS;

	case VMMR0_DO_HM_ENABLE:
		return VINF_SUCCESS;

	case VMMR0_DO_GVMM_SCHED_POKE:
	{
		Vcpu_handler *vcpu_handler = lookup_vcpu_handler(idCpu);
		Assert(vcpu_handler);
		if (vcpu_handler)
			vcpu_handler->recall();
		return VINF_SUCCESS;
	}

	default:
		Genode::error("SUPR3CallVMMR0Ex: unhandled uOperation ", uOperation);
		return VERR_GENERAL_FAILURE;
	}
}


/**
 * Various support stuff - base-nova specific.
 */
uint64_t genode_cpu_hz()
{
	static uint64_t cpu_freq = 0;

	if (!cpu_freq) {
		try {
			using namespace Genode;

			Rom_connection hip_rom("hypervisor_info_page");

			Nova::Hip * const hip = env()->rm_session()->attach(hip_rom.dataspace());

			cpu_freq = hip->tsc_freq * 1000;

		} catch (...) {
			Genode::error("could not read out CPU frequency");
			Genode::Lock lock;
			lock.lock();
		}
	}

	return cpu_freq;
}


void genode_update_tsc(void (*update_func)(void), unsigned long update_us)
{
	using namespace Genode;
	using namespace Nova;

	enum { TSC_FACTOR = 1000ULL };

	Genode::addr_t sem = Thread::myself()->native_thread().exc_pt_sel + Nova::SM_SEL_EC;
	unsigned long tsc_khz = (genode_cpu_hz() / 1000) / TSC_FACTOR;

	Trace::Timestamp us_64 = update_us;

	for (;;) {
		update_func();

		Trace::Timestamp now = Trace::timestamp();

		/* block until timeout fires or it gets canceled */
		unsigned long long tsc_absolute = now + us_64 * tsc_khz;
		Genode::uint8_t res = sm_ctrl(sem, SEMAPHORE_DOWN, tsc_absolute);
		if (res != Nova::NOVA_OK && res != Nova::NOVA_TIMEOUT)
			nova_die();
	}
}


HRESULT genode_setup_machine(ComObjPtr<Machine> machine)
{
	return genode_check_memory_config(machine);
};


bool Vmm_memory::revoke_from_vm(Mem_region *r)
{
	Assert(r);

	using namespace Genode;

	addr_t const vmm_local = (addr_t)r->local_addr<addr_t>();
	Assert(vmm_local);

	Flexpage_iterator fli(vmm_local, r->size(), 0, ~0UL, 0);

	Flexpage revoke_page = fli.page();
	while (revoke_page.valid()) {
		Assert(revoke_page.log2_order >= 12);
		Assert(!(((1UL << revoke_page.log2_order) - 1) & revoke_page.addr));

		using namespace Nova;

		Rights const revoke_rwx(true, true, true);
		Crd crd = Mem_crd(revoke_page.addr >> 12, revoke_page.log2_order - 12,
		                  revoke_rwx);
		revoke(crd, false);

		/* request next page(s) to be revoked */
		revoke_page = fli.page();
	}

	return true;
}


extern "C" void pthread_yield(void)
{
	Nova::ec_ctrl(Nova::EC_YIELD);
}


void *operator new (Genode::size_t size, int log2_align)
{
	static Libc::Mem_alloc_impl heap(Genode::env()->rm_session());
	return heap.alloc(size, log2_align);
}


bool create_emt_vcpu(pthread_t * pthread, size_t stack,
                     const pthread_attr_t *attr,
                     void *(*start_routine)(void *), void *arg,
                     Genode::Cpu_session * cpu_session,
                     Genode::Affinity::Location location,
                     unsigned int cpu_id)
{
	Nova::Hip * hip = hip_rom.local_addr<Nova::Hip>();

	if (!hip->has_feature_vmx() && !hip->has_feature_svm())
		return false;

	Vcpu_handler *vcpu_handler = 0;

	if (hip->has_feature_vmx())
		vcpu_handler = new (0x10) Vcpu_handler_vmx(stack, attr, start_routine,
		                                           arg, cpu_session, location,
		                                           cpu_id);

	if (hip->has_feature_svm())
		vcpu_handler = new (0x10) Vcpu_handler_svm(stack, attr, start_routine,
		                                           arg, cpu_session, location,
		                                           cpu_id);

	Assert(!(reinterpret_cast<unsigned long>(vcpu_handler) & 0xf));

	vcpu_handler_list().insert(vcpu_handler);

	*pthread = vcpu_handler;
	return true;
}
