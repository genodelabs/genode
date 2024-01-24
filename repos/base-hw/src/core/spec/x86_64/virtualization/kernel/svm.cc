/*
 * \brief  SVM specific implementations
 * \author Benjamin Lamowski
 * \date   2022-10-14
 */

/*
 * Copyright (C) 2022-2023 Genode Labs GmbH
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


Vmcb::Vmcb(Genode::uint32_t id)
:
	Mmio((Genode::addr_t)this)
{
	write<Guest_asid>(id);
	write<Msrpm_base_pa>(dummy_msrpm());
	write<Iopm_base_pa>(dummy_iopm());

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


void Board::Vcpu_context::write_vcpu_state(Genode::Vcpu_state &state)
{
	typedef Genode::Vcpu_state::Range Range;

	state.discharge();
	state.exit_reason = (unsigned) exitcode;

	state.fpu.charge([&] (Genode::Vcpu_state::Fpu::State &fpu) {
		memcpy(&fpu, (void *) regs->fpu_context(), sizeof(fpu));
	});

	state.ax.charge(vmcb.rax);
	state.cx.charge(regs->rcx);
	state.dx.charge(regs->rdx);
	state.bx.charge(regs->rbx);

	state.di.charge(regs->rdi);
	state.si.charge(regs->rsi);
	state.bp.charge(regs->rbp);


	state.ip.charge(vmcb.rip);
	/*
	 * SVM doesn't use ip_len, so just leave the old value.
	 * We still have to charge it when charging ip.
	 */
	state.ip_len.set_charged();

	state.flags.charge(vmcb.rflags);
	state.sp.charge(vmcb.rsp);

	state.dr7.charge(vmcb.dr7);

	state. r8.charge(regs->r8);
	state. r9.charge(regs->r9);
	state.r10.charge(regs->r10);
	state.r11.charge(regs->r11);
	state.r12.charge(regs->r12);
	state.r13.charge(regs->r13);
	state.r14.charge(regs->r14);
	state.r15.charge(regs->r15);

	state.cr0.charge(vmcb.cr0);
	state.cr2.charge(vmcb.cr2);
	state.cr3.charge(vmcb.cr3);
	state.cr4.charge(vmcb.cr4);

	state.cs.charge(vmcb.cs);
	state.ss.charge(vmcb.ss);
	state.es.charge(vmcb.es);
	state.ds.charge(vmcb.ds);
	state.fs.charge(vmcb.fs);
	state.gs.charge(vmcb.gs);
	state.tr.charge(vmcb.tr);
	state.ldtr.charge(vmcb.ldtr);
	state.gdtr.charge(Range { .limit = vmcb.gdtr.limit,
	                          .base  = vmcb.gdtr.base });

	state.idtr.charge(Range { .limit = vmcb.idtr.limit,
	                          .base  = vmcb.idtr.base });

	state.sysenter_cs.charge(vmcb.sysenter_cs);
	state.sysenter_sp.charge(vmcb.sysenter_esp);
	state.sysenter_ip.charge(vmcb.sysenter_eip);

	state.qual_primary.charge(vmcb.read<Vmcb::Exitinfo1>());
	state.qual_secondary.charge(vmcb.read<Vmcb::Exitinfo2>());

	state.ctrl_primary.charge(vmcb.read<Vmcb::Intercept_misc1>());
	state.ctrl_secondary.charge(vmcb.read<Vmcb::Intercept_misc2>());

	state.inj_info.charge(vmcb.read<Vmcb::Exitintinfo>()& 0xFFFFFFFF);
	state.inj_error.charge((Genode::uint32_t)
	                       (vmcb.read<Vmcb::Exitintinfo>() >> 32));

	/* Guest is in an interrupt shadow, see 15.21.5 */
	state.intr_state.charge((unsigned)
	                        vmcb.read<Vmcb::Int_control_ext::Int_shadow>());
	/* Guest activity state (actv) not used by SVM */
	state.actv_state.set_charged();

	state.tsc.charge(Hw::Lapic::rdtsc());
	state.tsc_offset.charge(vmcb.read<Vmcb::Tsc_offset>());

	tsc_aux_guest = Cpu::Ia32_tsc_aux::read();
	state.tsc_aux.charge(tsc_aux_guest);
	Cpu::Ia32_tsc_aux::write((Cpu::Ia32_tsc_aux::access_t) tsc_aux_host);

	state.efer.charge(vmcb.efer);

	/* pdpte not used by SVM */

	state.star.charge(vmcb.star);
	state.lstar.charge(vmcb.lstar);
	state.cstar.charge(vmcb.cstar);
	state.fmask.charge(vmcb.sfmask);
	state.kernel_gs_base.charge(vmcb.kernel_gs_base);

	/* Task Priority Register, see 15.24 */
	state.tpr.charge((unsigned) vmcb.read<Vmcb::Int_control::V_tpr>());
	/* TPR threshold not used by SVM */
}


void Board::Vcpu_context::read_vcpu_state(Genode::Vcpu_state &state)
{
	if (state.ax.charged() || state.cx.charged() ||
	    state.dx.charged() || state.bx.charged()) {
		vmcb.rax   = state.ax.value();
		regs->rcx   = state.cx.value();
		regs->rdx   = state.dx.value();
		regs->rbx   = state.bx.value();
	}

	if (state.bp.charged() || state.di.charged() || state.si.charged()) {
		regs->rdi   = state.di.value();
		regs->rsi   = state.si.value();
		regs->rbp   = state.bp.value();
	}

	if (state.flags.charged()) {
		vmcb.rflags = state.flags.value();
	}

	if (state.sp.charged()) {
		vmcb.rsp = state.sp.value();
	}

	if (state.ip.charged()) {
		vmcb.rip = state.ip.value();
		/* ip_len not used by SVM */
	}

	if (state.dr7.charged()) {
		vmcb.dr7 = state.dr7.value();
	}

	if (state.r8 .charged() || state.r9 .charged() ||
	    state.r10.charged() || state.r11.charged() ||
	    state.r12.charged() || state.r13.charged() ||
	    state.r14.charged() || state.r15.charged()) {

		regs->r8  = state.r8.value();
		regs->r9  = state.r9.value();
		regs->r10 = state.r10.value();
		regs->r11 = state.r11.value();
		regs->r12 = state.r12.value();
		regs->r13 = state.r13.value();
		regs->r14 = state.r14.value();
		regs->r15 = state.r15.value();
	}

	if (state.cr0.charged() || state.cr2.charged() ||
	    state.cr3.charged() || state.cr4.charged()) {
		vmcb.cr0 = state.cr0.value();
		vmcb.cr2 = state.cr2.value();
		vmcb.cr3 = state.cr3.value();
		vmcb.cr4 = state.cr4.value();
	}

	if (state.cs.charged() || state.ss.charged()) {
		vmcb.cs = state.cs.value();
		vmcb.ss = state.ss.value();
	}

	if (state.es.charged() || state.ds.charged()) {
		vmcb.es = state.es.value();
		vmcb.ds = state.ds.value();
	}

	if (state.fs.charged() || state.gs.charged()) {
		vmcb.fs = state.fs.value();
		vmcb.gs = state.gs.value();
	}

	if (state.tr.charged()) {
		vmcb.tr = state.tr.value();
	}

	if (state.ldtr.charged()) {
		vmcb.ldtr = state.ldtr.value();
	}

	if (state.gdtr.charged()) {
		vmcb.gdtr.limit = state.gdtr.value().limit;
		vmcb.gdtr.base  = state.gdtr.value().base;
	}

	if (state.idtr.charged()) {
		vmcb.idtr.limit = state.idtr.value().limit;
		vmcb.idtr.base  = state.idtr.value().base;
	}

	if (state.sysenter_cs.charged() || state.sysenter_sp.charged() ||
	    state.sysenter_ip.charged()) {
		vmcb.sysenter_cs = state.sysenter_cs.value();
		vmcb.sysenter_esp = state.sysenter_sp.value();
		vmcb.sysenter_eip = state.sysenter_ip.value();
	}

	if (state.ctrl_primary.charged() || state.ctrl_secondary.charged()) {
		vmcb.enforce_intercepts(state.ctrl_primary.value(),
			                state.ctrl_secondary.value());
	}

	if (state.inj_info.charged() || state.inj_error.charged()) {
		/* Honor special signaling bit */
		if (state.inj_info.value() & 0x1000) {
			vmcb.write<Vmcb::Int_control::V_irq>(1);
			vmcb.write<Vmcb::Int_control::V_ign_tpr>(1);
			vmcb.write<Vmcb::Intercept_misc1::Vintr>(1);
		} else {
			vmcb.write<Vmcb::Int_control::V_irq>(0);
			vmcb.write<Vmcb::Int_control::V_ign_tpr>(0);
			vmcb.write<Vmcb::Intercept_misc1::Vintr>(0);
		}


		vmcb.write<Vmcb::Eventinj>(
			/* Filter out special signaling bits */
			(state.inj_info.value()     &
			(Genode::uint32_t) ~0x3000) |
			(((Genode::uint64_t) state.inj_error.value()) << 32)
		);
	}

	if (state.intr_state.charged()) {
		vmcb.write<Vmcb::Int_control_ext::Int_shadow>(state.intr_state.value());
	}
	/* Guest activity state (actv) not used by SVM */

	if (state.tsc_offset.charged()) {
		/* state.tsc not used by SVM */
		vmcb.write<Vmcb::Tsc_offset>(vmcb.read<Vmcb::Tsc_offset>() +
		                             state.tsc_offset.value());
	}

	tsc_aux_host = Cpu::Ia32_tsc_aux::read();
	if (state.tsc_aux.charged()) {
		tsc_aux_guest = state.tsc_aux.value();
	}
	Cpu::Ia32_tsc_aux::write((Cpu::Ia32_tsc_aux::access_t) tsc_aux_guest);

	if (state.efer.charged()) {
		vmcb.efer = state.efer.value();
	}

	/* pdpte not used by SVM */

	if (state.star.charged() || state.lstar.charged() ||
	    state.cstar.charged() || state.fmask.charged() ||
	    state.kernel_gs_base.charged()) {
		vmcb.star = state.star.value();
		vmcb.cstar = state.cstar.value();
		vmcb.lstar = state.lstar.value();
		vmcb.sfmask = state.lstar.value();
		vmcb.kernel_gs_base = state.kernel_gs_base.value();
	}

	if (state.tpr.charged()) {
		vmcb.write<Vmcb::Int_control::V_tpr>(state.tpr.value());
		/* TPR threshold not used on AMD */
	}

	if (state.fpu.charged()) {
		state.fpu.with_state([&] (Genode::Vcpu_state::Fpu::State const &fpu) {
			memcpy((void *) regs->fpu_context(), &fpu, sizeof(fpu));
		});
	}
}
