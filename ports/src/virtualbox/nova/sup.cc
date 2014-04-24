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
#include <vmm/printf.h>

/* NOVA includes that come with Genode */
#include <nova/syscalls.h>

/* VirtualBox includes */
#include "HWACCMInternal.h" /* enable access to hwaccm.s.* */
#include "CPUMInternal.h" /* enable access to cpum.s.* */
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hwacc_svm.h>
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

		pVM->hwaccm.s.svm.fSupported = hip->has_feature_svm();
		pVM->hwaccm.s.vmx.fSupported = hip->has_feature_vmx();

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
	case SUP_VMMR0_DO_HWACC_RUN:
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

	case VMMR0_DO_HWACC_SETUP_VM:
		return VINF_SUCCESS;

	case VMMR0_DO_HWACC_ENABLE:
		return VINF_SUCCESS;

	/* XXX only do one of it - either recall or up - not both XXX */
	case VMMR0_DO_GVMM_SCHED_POKE:
		vcpu_handler->recall();
		r0_halt_sem()->up();
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


bool Vmm_memory::unmap_from_vm(RTGCPHYS GCPhys)
{
	size_t const size = 1;

	Region *r = _lookup_unsynchronized(GCPhys, size);
	if (!r) return false;

	using Genode::addr_t;
	addr_t const vmm_local = (addr_t)r->local_addr<addr_t>();

	Assert(vmm_local);
	Assert(!((r->size() - 1) & vmm_local));

	using namespace Nova;
	unsigned const order = Genode::log2(r->size() >> PAGE_SIZE_LOG2);

	Rights rwx(true, true, true);
	revoke(Mem_crd(vmm_local >> PAGE_SIZE_LOG2, order, rwx), false);

	return true;
}


extern "C" void pthread_yield(void) { Nova::ec_ctrl(Nova::EC_YIELD); }


extern "C"
bool create_emt_vcpu(pthread_t * pthread, size_t stack,
                     const pthread_attr_t *attr,
                     void *(*start_routine)(void *), void *arg)
{
	Nova::Hip * hip = hip_rom.local_addr<Nova::Hip>();

	if (!hip->has_feature_vmx() && !hip->has_feature_svm())
		return false;

	if (hip->has_feature_vmx())
		vcpu_handler = new Vcpu_handler_vmx(stack, attr, start_routine, arg);

	if (hip->has_feature_svm())
		vcpu_handler = new Vcpu_handler_svm(stack, attr, start_routine, arg);

	*pthread = vcpu_handler;
	return true;
}
