/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
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
#include <base/attached_rom_dataspace.h>
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
#include "vmm.h"
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
		const uint64_t u64NowGip = RTTimeNanoTS();
		const uint64_t ns_diff = u64Arg > u64NowGip ? u64Arg - u64NowGip : 0;

		if (!ns_diff)
			return VINF_SUCCESS;

		uint64_t const tsc_offset = genode_cpu_hz() * ns_diff / (1000*1000*1000);
		uint64_t const tsc_abs    = Genode::Trace::timestamp() + tsc_offset;

		using namespace Genode;

		if (ns_diff > RT_NS_1SEC)
			warning(" more than 1 sec vcpu halt ", ns_diff, " ns");

		Vcpu_handler *vcpu_handler = lookup_vcpu_handler(idCpu);
		Assert(vcpu_handler);
		vcpu_handler->halt(tsc_abs);

		return VINF_SUCCESS;
	}

	case VMMR0_DO_GVMM_SCHED_WAKE_UP:
	{
		Vcpu_handler *vcpu_handler = lookup_vcpu_handler(idCpu);
		Assert(vcpu_handler);

		/* don't wake the currently running thread again */
		if (vcpu_handler->utcb() == Genode::Thread::myself()->utcb())
			return VINF_SUCCESS;

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
			platform_rom().sub_node("tsc").attribute("freq_khz").value(&cpu_freq);
			cpu_freq *= 1000ULL;
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


void *operator new (__SIZE_TYPE__ size, int log2_align)
{
	static Libc::Mem_alloc_impl heap(genode_env().rm(), genode_env().ram());
	return heap.alloc(size, log2_align);
}


bool create_emt_vcpu(pthread_t * pthread, size_t stack,
                     void *(*start_routine)(void *), void *arg,
                     Genode::Cpu_session * cpu_session,
                     Genode::Affinity::Location location,
                     unsigned int cpu_id, const char * name)
{
	Genode::Xml_node const features = platform_rom().sub_node("features");
	bool const svm = features.attribute_value("svm", false);
	bool const vmx = features.attribute_value("vmx", false);

	if (!svm && !vmx)
		return false;

	static Genode::Pd_connection pd_vcpus(genode_env(), "VM");

	Vcpu_handler *vcpu_handler = 0;

	if (vmx)
		vcpu_handler = new (0x10) Vcpu_handler_vmx(genode_env(),
		                                           stack, start_routine,
		                                           arg, cpu_session, location,
		                                           cpu_id, name, pd_vcpus);

	if (svm)
		vcpu_handler = new (0x10) Vcpu_handler_svm(genode_env(),
		                                           stack, start_routine,
		                                           arg, cpu_session, location,
		                                           cpu_id, name, pd_vcpus);

	Assert(!(reinterpret_cast<unsigned long>(vcpu_handler) & 0xf));

	vcpu_handler_list().insert(vcpu_handler);

	*pthread = &vcpu_handler->pthread_obj();
	return true;
}
