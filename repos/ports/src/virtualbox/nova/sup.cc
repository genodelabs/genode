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
#include <base/printf.h>
#include <base/semaphore.h>
#include <base/flex_iterator.h>
#include <rom_session/connection.h>
#include <timer_session/connection.h>
#include <os/attached_rom_dataspace.h>

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


static Vcpu_handler *vcpu_handler = 0;


static Genode::Semaphore *r0_halt_sem()
{
	static Genode::Semaphore sem;
	return &sem;
}


/* Genode specific function */

static Genode::Attached_rom_dataspace hip_rom("hypervisor_info_page");

void SUPR3QueryHWACCLonGenodeSupport(VM * pVM)
{
	try {
		Nova::Hip * hip = hip_rom.local_addr<Nova::Hip>();

		pVM->hm.s.svm.fSupported = hip->has_feature_svm();
		pVM->hm.s.vmx.fSupported = hip->has_feature_vmx();

		PINF("support svm %u vmx %u", hip->has_feature_svm(), hip->has_feature_vmx());
	} catch (...) {
		PWRN("No hardware acceleration available - execution will be slow!");
	} /* if we get an exception let hardware support off */
}


/* VirtualBox SUPLib interface */
int SUPR3QueryVTxSupported(void) { return VINF_SUCCESS; }


int SUPR3CallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation, VMCPUID idCpu)
{
	switch (uOperation) {
	case SUP_VMMR0_DO_HM_RUN:
		return vcpu_handler->run_hw(pVMR0, idCpu);
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

	case VMMR0_DO_GVMM_SCHED_HALT:
		r0_halt_sem()->down();
		return VINF_SUCCESS;

	case VMMR0_DO_GVMM_SCHED_WAKE_UP:
		r0_halt_sem()->up();
		return VINF_SUCCESS;

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
		vcpu_handler->recall();
		return VINF_SUCCESS;

	default:
		PERR("SUPR3CallVMMR0Ex: unhandled uOperation %d", uOperation);
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
			PERR("could not read out CPU frequency.");
			Genode::Lock lock;
			lock.lock();
		}
	}

	return cpu_freq;
}


bool Vmm_memory::revoke_from_vm(Region *r)
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
/*
	char _name[64];
	Genode::Thread_base::myself()->name(_name, sizeof(_name));
	PERR("pthread_yield %p - '%s'", __builtin_return_address(0), _name);
	Assert(!"pthread_yield called");
*/
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
                     Genode::Affinity::Location location)
{
	Nova::Hip * hip = hip_rom.local_addr<Nova::Hip>();

	if (!hip->has_feature_vmx() && !hip->has_feature_svm())
		return false;

	if (hip->has_feature_vmx())
		vcpu_handler = new (0x10) Vcpu_handler_vmx(stack, attr, start_routine,
		                                           arg, cpu_session, location);

	if (hip->has_feature_svm())
		vcpu_handler = new (0x10) Vcpu_handler_svm(stack, attr, start_routine,
		                                           arg, cpu_session, location);

	Assert(!(reinterpret_cast<unsigned long>(vcpu_handler) & 0xf));

	*pthread = vcpu_handler;
	return true;
}
