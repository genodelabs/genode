/*
 * \brief  Genode specific VirtualBox SUPLib supplements.
 *         File used by Genode platforms not supporting hardware
 *         virtualisation features.
 * \author Alexander Boettcher
 * \date   2013-11-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/semaphore.h>
#include <timer_session/connection.h>

/* VirtualBox includes */
#include <VBox/vmm/vm.h>
#include <VBox/err.h>

/* Genode's VirtualBox includes */
#include "sup.h"
#include "vmm.h"

/* Libc include */
#include <pthread.h>
#include <sched.h> /* sched_yield */


/* VirtualBox SUPLib interface */

int SUPR3QueryVTxSupported(void)
{
	return VERR_INTERNAL_ERROR;	
}

int SUPR3CallVMMR0Fast(PVMR0 pVMR0, unsigned uOperation, VMCPUID idCpu)
{
	return VERR_INTERNAL_ERROR;	
}


static Genode::Semaphore *r0_halt_sem()
{
	static Genode::Semaphore sem;
	return &sem;
}


int SUPR3CallVMMR0Ex(PVMR0 pVMR0, VMCPUID idCpu, unsigned
                     uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{
	switch(uOperation)
	{
 		case VMMR0_DO_GVMM_CREATE_VM:
			genode_VMMR0_DO_GVMM_CREATE_VM(pReqHdr);
			return VINF_SUCCESS;

		case VMMR0_DO_GVMM_SCHED_HALT:
			r0_halt_sem()->down();
			return VINF_SUCCESS;

		case VMMR0_DO_GVMM_SCHED_WAKE_UP:
			r0_halt_sem()->up();
			return VINF_SUCCESS;

		case VMMR0_DO_VMMR0_INIT:
			return VINF_SUCCESS;

		case VMMR0_DO_GVMM_SCHED_POLL:
			/* called by 'vmR3HaltGlobal1Halt' */
			Genode::log(__func__, ": SUPR3CallVMMR0Ex: VMMR0_DO_GVMM_SCHED_POLL");
			return VINF_SUCCESS;

		default:
			Genode::error("SUPR3CallVMMR0Ex: unhandled uOperation ", (int)uOperation);
			return VERR_GENERAL_FAILURE;
	}
}


bool create_emt_vcpu(pthread_t * thread, size_t stack_size,
                     const pthread_attr_t *attr,
                     void *(*start_routine)(void *), void *arg,
                     Genode::Cpu_session * cpu_session,
                     Genode::Affinity::Location location,
                     unsigned int cpu_id,
                     const char * name)
{
	/* no hardware acceleration support */
	return false;
}


/**
 * Dummies and unimplemented stuff.
 */

uint64_t genode_cpu_hz() {
	return 1000000000ULL; /* XXX fixed 1GHz return value */
}


void genode_update_tsc(void (*update_func)(void), unsigned long update_us)
{
	using namespace Genode;

	Timer::Connection timer(genode_env());
	Signal_context    sig_ctx;
	Signal_receiver   sig_rec;
	Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

	timer.sigh(sig_cap);
	timer.trigger_once(update_us);

	for (;;) {
		Signal s = sig_rec.wait_for_signal();
		update_func();

		timer.trigger_once(update_us);
	}
}


HRESULT genode_setup_machine(ComObjPtr<Machine> machine)
{
	return genode_check_memory_config(machine);
}


extern "C" int sched_yield(void)
{
	Genode::warning(__func__, " unimplemented");
	return -1;
}

int SUPR3PageAllocEx(::size_t cPages, uint32_t fFlags, void **ppvPages,
                     PRTR0PTR pR0Ptr, PSUPPAGE paPages)
{
	Genode::error(__func__, " unimplemented");
	return VERR_GENERAL_FAILURE;
}

extern "C" bool PGMUnmapMemoryGenode(void *, ::size_t)
{
	Genode::error(__func__, " unimplemented");
	return VERR_GENERAL_FAILURE;
}
