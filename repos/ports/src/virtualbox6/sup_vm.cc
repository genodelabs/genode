/*
 * \brief  Suplib VM implementation
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

/* VirtualBox includes */
#include <VBox/vmm/cpum.h> /* must be included before CPUMInternal.h */
#include <CPUMInternal.h>  /* enable access to cpum.s.* */
#include <HMInternal.h>    /* enable access to hm.s.* */
#include <NEMInternal.h>   /* enable access to nem.s.* */
#include <iprt/mem.h>

/* local includes */
#include <sup_vm.h>
#include <sup_vcpu.h>


static size_t gvm_size(Sup::Cpu_count cpu_count)
{
	/* expected size of GVM structure (taken from GVMMR0.cpp) */
	return RT_ALIGN_32(RT_UOFFSETOF_DYN(GVM, aCpus[cpu_count.value]), PAGE_SIZE);
}


void Sup::Vm::init(PSUPDRVSESSION psession, Cpu_count cpu_count)
{
	/* alloc and emulate R0MEMOBJ */
	size_t    const num_pages = gvm_size(cpu_count) / PAGE_SIZE;
	SUPPAGE * const pages     = (SUPPAGE *)RTMemAllocZ(sizeof(SUPPAGE)*num_pages);

	for (size_t i = 0; i < num_pages; ++i)
		pages[i] = { .Phys = (RTHCPHYS)this + i*PAGE_SIZE, .uReserved = 0 };

	/*
	 * Some members of VM also exist in GVM (e.g., pSession) therefore we
	 * explicitly qualify which one is used.
	 */

	VM::enmVMState       = VMSTATE_CREATING;
	VM::paVMPagesR3      = (R3PTRTYPE(PSUPPAGE))pages;
	VM::pVMR0ForCall     = (PVMR0)this;
	VM::pSession         = psession;
	VM::cbSelf           = sizeof(VM);
	VM::cbVCpu           = sizeof(VMCPU);
	VM::cCpus            = cpu_count.value;
	VM::uCpuExecutionCap = 100;  /* expected by 'vmR3CreateU()' */
	VM::nem.s.fEnabled   = true;

	for (uint32_t i = 0; i < cpu_count.value; ++i) {
		VMCPU &cpu = GVM::aCpus[i];

		cpu.idCpu           = i;
		cpu.pVMR3           = this;
		cpu.idHostCpu       = NIL_RTCPUID;
		cpu.hNativeThread   = NIL_RTNATIVETHREAD;
		cpu.hNativeThreadR0 = NIL_RTNATIVETHREAD;
		cpu.enmState        = VMCPUSTATE_STOPPED;

		VM::apCpusR3[i] = &cpu;
	}
}


Sup::Vm & Sup::Vm::create(PSUPDRVSESSION psession, Cpu_count cpu_count)
{
	/*
	 * Allocate and initialize VM struct
	 *
	 * The original R0 GVM struct inherits VM and is also followed by the
	 * variable-sized array of GVMCPU objects. We only allocate and maintain
	 * the R3 VM struct, which must be page-aligned and contains an array of
	 * VMCPU pointers in apCpusR3.
	 */
	Vm *vm_ptr = (Vm *)RTMemPageAllocZ(gvm_size(cpu_count));

	vm_ptr->init(psession, cpu_count);

	return *vm_ptr;
}


void Sup::Vm::register_vcpu(Cpu_index cpu_index, Vcpu &vcpu)
{
	if (cpu_index.value >= VM::cCpus)
		throw Cpu_index_out_of_range();

	VMCPU &vmcpu = GVM::aCpus[cpu_index.value];

	/*
	 * We misuse the pVCpuR0ForVtg member for storing the pointer
	 * to the CPU's corresponding Vcpu.
	 */
	vmcpu.pVCpuR0ForVtg = (RTR0PTR)&vcpu;
}

