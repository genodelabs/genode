/*
 * \brief   SVM specific implementations
 * \author  Benjamin Lamowski
 * \date    2022-10-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <hw/spec/x86_64/x86_64.h>
#include <kernel/cpu.h>
#include <platform.h>
#include <spec/x86_64/virtualization/svm.h>
#include <util/mmio.h>

using Genode::addr_t;
using Kernel::Cpu;
using Kernel::Vm;
using Board::Vmcb;


Vmcb::Vmcb(Genode::uint32_t id, Genode::addr_t addr)
:
	Mmio((Genode::addr_t)this)
{
	write<Guest_asid>(id);
	write<Msrpm_base_pa>(dummy_msrpm());
	write<Iopm_base_pa>(dummy_iopm());
	phys_addr = addr;

	/*
	 * Set the guest PAT register to the default value.
	 * See: AMD Vol.2 7.8 Page-Attribute Table Mechanism
	 */
	g_pat = 0x0007040600070406ULL;
}


Vmcb & Vmcb::host_vmcb(Genode::size_t cpu_id)
{
	static Genode::Constructible<Vmcb> host_vmcb[NR_OF_CPUS];

	if (!host_vmcb[cpu_id].constructed()) {
		host_vmcb[cpu_id].construct(Vmcb::Asid_host);
	}
	return *host_vmcb[cpu_id];
}


void Vmcb::init(Genode::size_t cpu_id, void * table_ptr)
{
	using Cpu = Hw::X86_64_cpu;

	root_vmcb_phys = Core::Platform::core_phys_addr((addr_t)
	                                                &host_vmcb(cpu_id));
	asm volatile ("vmsave" : : "a" (root_vmcb_phys) : "memory");
	Cpu::Amd_vm_hsavepa::write((Cpu::Amd_vm_hsavepa::access_t) root_vmcb_phys);

	/*
	 * enable nested paging
	 */
	write<Npt_control::Np_enable>(1);
	write<N_cr3>((Genode::addr_t) table_ptr);

	write<Int_control::V_intr_mask>(1); /* See 15.2 */
	write<Intercept_ex::Vectors>(17);   /* AC */

	enforce_intercepts();
}


/*
 * Enforce SVM intercepts
 */
void Vmcb::enforce_intercepts(Genode::uint32_t desired_primary, Genode::uint32_t desired_secondary)
{
	write<Vmcb::Intercept_misc1>(
		desired_primary                           |
		Vmcb::Intercept_misc1::Intr::bits(1)      |
		Vmcb::Intercept_misc1::Nmi::bits(1)       |
		Vmcb::Intercept_misc1::Init::bits(1)      |
		Vmcb::Intercept_misc1::Invd::bits(1)      |
		Vmcb::Intercept_misc1::Hlt::bits(1)       |
		Vmcb::Intercept_misc1::Ioio_prot::bits(1) |
		Vmcb::Intercept_misc1::Msr_prot::bits(1)  |
		Vmcb::Intercept_misc1::Shutdown::bits(1)
	);
	write<Vmcb::Intercept_misc2>(
		desired_secondary                      |
		Vmcb::Intercept_misc2::Vmload::bits(1) |
		Vmcb::Intercept_misc2::Vmsave::bits(1) |
		Vmcb::Intercept_misc2::Clgi::bits(1)   |
		Vmcb::Intercept_misc2::Skinit::bits(1)
	);
}


/*
 * AMD Vol.2 15.11: MSR Permissions Map
 * All set to 1 since we want all MSRs to be intercepted.
 */
Genode::addr_t Vmcb::dummy_msrpm()
{
	static Genode::Constructible<Board::Msrpm> msrpm;
	if (!msrpm.constructed())
		msrpm.construct();

	return Core::Platform::core_phys_addr((addr_t) & *msrpm);
}


/*
 * AMD Vol.2 15.10.1 I/O Permissions Map
 * All set to 1 since we want all IO port accesses to be intercepted.
 */
Genode::addr_t Vmcb::dummy_iopm()
{
	static Genode::Constructible<Board::Iopm> iopm;
	if (!iopm.constructed())
		iopm.construct();

	return Core::Platform::core_phys_addr((addr_t) &*iopm);
}


Board::Msrpm::Msrpm()
{
	Genode::memset(this, 0xFF, sizeof(*this));
}


Board::Iopm::Iopm()
{
	Genode::memset(this, 0xFF, sizeof(*this));
}


void Board::Vcpu_context::initialize_svm(Kernel::Cpu & cpu, void * table)
{
	using Cpu = Hw::X86_64_cpu;

	Cpu::Ia32_efer::access_t ia32_efer_msr =  Cpu::Ia32_efer::read();
	Cpu::Ia32_efer::Svme::set(ia32_efer_msr, 1);
	Cpu::Ia32_efer::write(ia32_efer_msr);

	Cpu::Amd_vm_syscvg::access_t amd_vm_syscvg_msr = Cpu::Amd_vm_syscvg::read();
	Cpu::Amd_vm_syscvg::Nested_paging::set(amd_vm_syscvg_msr, 1);
	Cpu::Amd_vm_syscvg::write(amd_vm_syscvg_msr);

	vmcb.init(cpu.id(), table);
}
