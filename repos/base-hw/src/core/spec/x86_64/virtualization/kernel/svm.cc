/*
 * \brief  SVM specific implementations
 * \author Benjamin Lamowski
 * \date   2022-10-14
 */

/*
 * Copyright (C) 2022-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/page_size.h>
#include <base/log.h>
#include <hw/spec/x86_64/x86_64.h>
#include <kernel/cpu.h>
#include <platform.h>
#include <spec/x86_64/virtualization/svm.h>
#include <util/mmio.h>

using namespace Genode;

using Kernel::Cpu;
using Kernel::Vm;
using Board::Vmcb;
using Board::Vmcb_buf;


Vmcb_buf::Vmcb_buf(addr_t vmcb_page_addr, uint32_t id)
:
	Mmio({(char *)vmcb_page_addr, Mmio::SIZE})
{
	memset((void *) vmcb_page_addr, 0, get_page_size());

	write<Guest_asid>(id);
	write<Msrpm_base_pa>(dummy_msrpm());
	write<Iopm_base_pa>(dummy_iopm());

	/*
	 * Set the guest PAT register to the default value.
	 * See: AMD Vol.2 7.8 Page-Attribute Table Mechanism
	 */
	write<G_pat>(0x0007040600070406ULL);
}


Vmcb::Vmcb(Vcpu_data &vcpu_data, uint32_t id)
:
	Board::Virt_interface(vcpu_data),
	v((addr_t) vcpu_data.virt_area + get_page_size(), id)
{
}


Vmcb_buf &Vmcb::host_vmcb(size_t cpu_id)
{
	static uint8_t host_vmcb_pages[get_page_size() * NR_OF_CPUS];
	static Constructible<Vmcb_buf> host_vmcb[NR_OF_CPUS];

	if (!host_vmcb[cpu_id].constructed()) {
		host_vmcb[cpu_id].construct(
				(addr_t) host_vmcb_pages +
				get_page_size() * cpu_id,
				Asid_host);
	}
	return *host_vmcb[cpu_id];
}

void Vmcb::initialize(Kernel::Cpu &cpu, addr_t page_table_phys_addr,
                      Core::Cpu::Context &)
{
	using Cpu = Hw::X86_64_cpu;

	Cpu::Ia32_efer::access_t ia32_efer_msr = Cpu::Ia32_efer::read();
	Cpu::Ia32_efer::Svme::set(ia32_efer_msr, 1);
	Cpu::Ia32_efer::write(ia32_efer_msr);

	Cpu::Amd_vm_syscvg::access_t amd_vm_syscvg_msr =
	    Cpu::Amd_vm_syscvg::read();
	Cpu::Amd_vm_syscvg::Nested_paging::set(amd_vm_syscvg_msr, 1);
	Cpu::Amd_vm_syscvg::write(amd_vm_syscvg_msr);

	root_vmcb_phys =
	    Core::Platform::core_phys_addr(host_vmcb(cpu.id()).base());
	asm volatile ("vmsave" : : "a" (root_vmcb_phys) : "memory");
	Cpu::Amd_vm_hsavepa::write((Cpu::Amd_vm_hsavepa::access_t) root_vmcb_phys);

	/*
	 * enable nested paging
	 */
	v.write<Vmcb_buf::Npt_control::Np_enable>(1);
	v.write<Vmcb_buf::N_cr3>(page_table_phys_addr);

	v.write<Vmcb_buf::Int_control::V_intr_mask>(1); /* See 15.2 */
	v.write<Vmcb_buf::Intercept_ex::Vectors>(17);   /* AC */

	enforce_intercepts();
}


/*
 * Enforce SVM intercepts
 */
void Vmcb::enforce_intercepts(uint32_t desired_primary, uint32_t desired_secondary)
{
	v.write<Vmcb_buf::Intercept_misc1>(
		desired_primary                           |
		Vmcb_buf::Intercept_misc1::Intr::bits(1)      |
		Vmcb_buf::Intercept_misc1::Nmi::bits(1)       |
		Vmcb_buf::Intercept_misc1::Init::bits(1)      |
		Vmcb_buf::Intercept_misc1::Invd::bits(1)      |
		Vmcb_buf::Intercept_misc1::Hlt::bits(1)       |
		Vmcb_buf::Intercept_misc1::Ioio_prot::bits(1) |
		Vmcb_buf::Intercept_misc1::Msr_prot::bits(1)  |
		Vmcb_buf::Intercept_misc1::Shutdown::bits(1)
	);
	v.write<Vmcb_buf::Intercept_misc2>(
		desired_secondary                      |
		Vmcb_buf::Intercept_misc2::Vmload::bits(1) |
		Vmcb_buf::Intercept_misc2::Vmsave::bits(1) |
		Vmcb_buf::Intercept_misc2::Clgi::bits(1)   |
		Vmcb_buf::Intercept_misc2::Skinit::bits(1)
	);
}


/*
 * AMD Vol.2 15.11: MSR Permissions Map
 * All set to 1 since we want all MSRs to be intercepted.
 */
addr_t Vmcb_buf::dummy_msrpm()
{
	static Board::Msrpm msrpm;

	return Core::Platform::core_phys_addr((addr_t) &msrpm);
}


/*
 * AMD Vol.2 15.10.1 I/O Permissions Map
 * All set to 1 since we want all IO port accesses to be intercepted.
 */
addr_t Vmcb_buf::dummy_iopm()
{
	static Board::Iopm iopm;

	return Core::Platform::core_phys_addr((addr_t) &iopm);
}


Board::Msrpm::Msrpm()
{
	memset(this, 0xFF, sizeof(*this));
}


Board::Iopm::Iopm()
{
	memset(this, 0xFF, sizeof(*this));
}


void Vmcb::write_vcpu_state(Vcpu_state &state)
{
	state.ax.charge(v.read<Vmcb_buf::Rax>());
	state.ip.charge(v.read<Vmcb_buf::Rip>());
	/*
	 * SVM doesn't use ip_len, so just leave the old value.
	 * We still have to charge it when charging ip.
	 */
	state.ip_len.set_charged();

	state.flags.charge(v.read<Vmcb_buf::Rflags>());
	state.sp.charge(v.read<Vmcb_buf::Rsp>());

	state.dr7.charge(v.read<Vmcb_buf::Dr7>());

	state.cr0.charge(v.read<Vmcb_buf::Cr0>());
	state.cr2.charge(v.read<Vmcb_buf::Cr2>());
	state.cr3.charge(v.read<Vmcb_buf::Cr3>());
	state.cr4.charge(v.read<Vmcb_buf::Cr4>());

	state.cs.charge(Vcpu_state::Segment {
		.sel = v.cs.read<Vmcb_buf::Segment::Sel>(),
		.ar = v.cs.read<Vmcb_buf::Segment::Ar>(),
		.limit = v.cs.read<Vmcb_buf::Segment::Limit>(),
		.base = v.cs.read<Vmcb_buf::Segment::Base>(),
	});

	state.ss.charge(Vcpu_state::Segment {
		.sel = v.ss.read<Vmcb_buf::Segment::Sel>(),
		.ar = v.ss.read<Vmcb_buf::Segment::Ar>(),
		.limit = v.ss.read<Vmcb_buf::Segment::Limit>(),
		.base = v.ss.read<Vmcb_buf::Segment::Base>(),
	});

	state.es.charge(Vcpu_state::Segment {
		.sel = v.es.read<Vmcb_buf::Segment::Sel>(),
		.ar = v.es.read<Vmcb_buf::Segment::Ar>(),
		.limit = v.es.read<Vmcb_buf::Segment::Limit>(),
		.base = v.es.read<Vmcb_buf::Segment::Base>(),
	});

	state.ds.charge(Vcpu_state::Segment {
		.sel = v.ds.read<Vmcb_buf::Segment::Sel>(),
		.ar = v.ds.read<Vmcb_buf::Segment::Ar>(),
		.limit = v.ds.read<Vmcb_buf::Segment::Limit>(),
		.base = v.ds.read<Vmcb_buf::Segment::Base>(),
	});

	state.fs.charge(Vcpu_state::Segment {
		.sel = v.fs.read<Vmcb_buf::Segment::Sel>(),
		.ar = v.fs.read<Vmcb_buf::Segment::Ar>(),
		.limit = v.fs.read<Vmcb_buf::Segment::Limit>(),
		.base = v.fs.read<Vmcb_buf::Segment::Base>(),
	});

	state.gs.charge(Vcpu_state::Segment {
		.sel = v.gs.read<Vmcb_buf::Segment::Sel>(),
		.ar = v.gs.read<Vmcb_buf::Segment::Ar>(),
		.limit = v.gs.read<Vmcb_buf::Segment::Limit>(),
		.base = v.gs.read<Vmcb_buf::Segment::Base>(),
	});

	state.tr.charge(Vcpu_state::Segment {
		.sel = v.tr.read<Vmcb_buf::Segment::Sel>(),
		.ar = v.tr.read<Vmcb_buf::Segment::Ar>(),
		.limit = v.tr.read<Vmcb_buf::Segment::Limit>(),
		.base = v.tr.read<Vmcb_buf::Segment::Base>(),
	});

	state.ldtr.charge(Vcpu_state::Segment {
		.sel = v.ldtr.read<Vmcb_buf::Segment::Sel>(),
		.ar = v.ldtr.read<Vmcb_buf::Segment::Ar>(),
		.limit = v.ldtr.read<Vmcb_buf::Segment::Limit>(),
		.base = v.ldtr.read<Vmcb_buf::Segment::Base>(),
	});

	state.gdtr.charge(Vcpu_state::Range {
		.limit = v.gdtr.read<Vmcb_buf::Segment::Limit>(),
		.base = v.gdtr.read<Vmcb_buf::Segment::Base>(),
	});

	state.idtr.charge(Vcpu_state::Range {
		.limit = v.idtr.read<Vmcb_buf::Segment::Limit>(),
		.base = v.idtr.read<Vmcb_buf::Segment::Base>(),
	});


	state.sysenter_cs.charge(v.read<Vmcb_buf::Sysenter_cs>());
	state.sysenter_sp.charge(v.read<Vmcb_buf::Sysenter_esp>());
	state.sysenter_ip.charge(v.read<Vmcb_buf::Sysenter_eip>());

	state.qual_primary.charge(v.read<Vmcb_buf::Exitinfo1>());
	state.qual_secondary.charge(v.read<Vmcb_buf::Exitinfo2>());

	/* Charging ctrl_primary and ctrl_secondary breaks Virtualbox 6 */

	state.inj_info.charge(v.read<Vmcb_buf::Exitintinfo>() & 0xFFFFFFFF);
	state.inj_error.charge(
	    (uint32_t)(v.read<Vmcb_buf::Exitintinfo>() >> 32));

	/* Guest is in an interrupt shadow, see 15.21.5 */
	state.intr_state.charge(
	    (unsigned)v.read<Vmcb_buf::Int_control_ext::Int_shadow>());
	/* Guest activity state (actv) not used by SVM */
	state.actv_state.set_charged();

	state.tsc.charge(Hw::Lapic::rdtsc());
	state.tsc_offset.charge(v.read<Vmcb_buf::Tsc_offset>());

	state.efer.charge(v.read<Vmcb_buf::Efer>());

	/* pdpte not used by SVM */

	state.star.charge(v.read<Vmcb_buf::Star>());
	state.lstar.charge(v.read<Vmcb_buf::Lstar>());
	state.cstar.charge(v.read<Vmcb_buf::Cstar>());
	state.fmask.charge(v.read<Vmcb_buf::Sfmask>());
	state.kernel_gs_base.charge(v.read<Vmcb_buf::Kernel_gs_base>());

	/* Task Priority Register, see 15.24 */
	state.tpr.charge((unsigned)v.read<Vmcb_buf::Int_control::V_tpr>());
	/* TPR threshold not used by SVM */
}


void Vmcb::read_vcpu_state(Vcpu_state &state)
{
	if (state.ax.charged())    v.write<Vmcb_buf::Rax>(state.ax.value());
	if (state.flags.charged()) v.write<Vmcb_buf::Rflags>(state.flags.value());
	if (state.sp.charged())    v.write<Vmcb_buf::Rsp>(state.sp.value());
	if (state.ip.charged())    v.write<Vmcb_buf::Rip>(state.ip.value());
	/* ip_len not used by SVM */
	if (state.dr7.charged())   v.write<Vmcb_buf::Dr7>(state.dr7.value());

	if (state.cr0.charged()) v.write<Vmcb_buf::Cr0>(state.cr0.value());
	if (state.cr2.charged()) v.write<Vmcb_buf::Cr2>(state.cr2.value());
	if (state.cr3.charged()) v.write<Vmcb_buf::Cr3>(state.cr3.value());
	if (state.cr4.charged()) v.write<Vmcb_buf::Cr4>(state.cr4.value());

	if (state.cs.charged()) {
		v.cs.write<Vmcb_buf::Segment::Sel>(state.cs.value().sel);
		v.cs.write<Vmcb_buf::Segment::Ar>(state.cs.value().ar);
		v.cs.write<Vmcb_buf::Segment::Limit>(state.cs.value().limit);
		v.cs.write<Vmcb_buf::Segment::Base>(state.cs.value().base);
	}

	if (state.ss.charged()) {
		v.ss.write<Vmcb_buf::Segment::Sel>(state.ss.value().sel);
		v.ss.write<Vmcb_buf::Segment::Ar>(state.ss.value().ar);
		v.ss.write<Vmcb_buf::Segment::Limit>(state.ss.value().limit);
		v.ss.write<Vmcb_buf::Segment::Base>(state.ss.value().base);
	}

	if (state.es.charged()) {
		v.es.write<Vmcb_buf::Segment::Sel>(state.es.value().sel);
		v.es.write<Vmcb_buf::Segment::Ar>(state.es.value().ar);
		v.es.write<Vmcb_buf::Segment::Limit>(state.es.value().limit);
		v.es.write<Vmcb_buf::Segment::Base>(state.es.value().base);
	}

	if (state.ds.charged()) {
		v.ds.write<Vmcb_buf::Segment::Sel>(state.ds.value().sel);
		v.ds.write<Vmcb_buf::Segment::Ar>(state.ds.value().ar);
		v.ds.write<Vmcb_buf::Segment::Limit>(state.ds.value().limit);
		v.ds.write<Vmcb_buf::Segment::Base>(state.ds.value().base);
	}

	if (state.fs.charged()) {
		v.gs.write<Vmcb_buf::Segment::Sel>(state.gs.value().sel);
		v.gs.write<Vmcb_buf::Segment::Ar>(state.gs.value().ar);
		v.gs.write<Vmcb_buf::Segment::Limit>(state.gs.value().limit);
		v.gs.write<Vmcb_buf::Segment::Base>(state.gs.value().base);
	}

	if (state.gs.charged()) {
		v.fs.write<Vmcb_buf::Segment::Sel>(state.fs.value().sel);
		v.fs.write<Vmcb_buf::Segment::Ar>(state.fs.value().ar);
		v.fs.write<Vmcb_buf::Segment::Limit>(state.fs.value().limit);
		v.fs.write<Vmcb_buf::Segment::Base>(state.fs.value().base);
	}

	if (state.tr.charged()) {
		v.tr.write<Vmcb_buf::Segment::Sel>(state.tr.value().sel);
		v.tr.write<Vmcb_buf::Segment::Ar>(state.tr.value().ar);
		v.tr.write<Vmcb_buf::Segment::Limit>(state.tr.value().limit);
		v.tr.write<Vmcb_buf::Segment::Base>(state.tr.value().base);
	}

	if (state.ldtr.charged()) {
		v.ldtr.write<Vmcb_buf::Segment::Sel>(state.ldtr.value().sel);
		v.ldtr.write<Vmcb_buf::Segment::Ar>(state.ldtr.value().ar);
		v.ldtr.write<Vmcb_buf::Segment::Limit>(state.ldtr.value().limit);
		v.ldtr.write<Vmcb_buf::Segment::Base>(state.ldtr.value().base);
	}

	if (state.gdtr.charged()) {
		v.gdtr.write<Vmcb_buf::Segment::Limit>(state.gdtr.value().limit);
		v.gdtr.write<Vmcb_buf::Segment::Base>(state.gdtr.value().base);
	}

	if (state.idtr.charged()) {
		v.idtr.write<Vmcb_buf::Segment::Limit>(state.idtr.value().limit);
		v.idtr.write<Vmcb_buf::Segment::Base>(state.idtr.value().base);
	}

	if (state.sysenter_cs.charged())
		v.write<Vmcb_buf::Sysenter_cs>(state.sysenter_cs.value());

	if (state.sysenter_sp.charged())
		v.write<Vmcb_buf::Sysenter_esp>(state.sysenter_sp.value());

	if (state.sysenter_ip.charged())
		v.write<Vmcb_buf::Sysenter_eip>(state.sysenter_ip.value());

	if (state.ctrl_primary.charged() || state.ctrl_secondary.charged()) {
		enforce_intercepts(state.ctrl_primary.value(),
			           state.ctrl_secondary.value());
	}

	if (state.inj_info.charged() || state.inj_error.charged()) {
		/* Honor special signaling bit */
		if (state.inj_info.value() & 0x1000) {
			v.write<Vmcb_buf::Int_control::V_irq>(1);
			v.write<Vmcb_buf::Int_control::V_ign_tpr>(1);
			v.write<Vmcb_buf::Intercept_misc1::Vintr>(1);
		} else {
			v.write<Vmcb_buf::Int_control::V_irq>(0);
			v.write<Vmcb_buf::Int_control::V_ign_tpr>(0);
			v.write<Vmcb_buf::Intercept_misc1::Vintr>(0);
		}

		v.write<Vmcb_buf::Eventinj>(
			/* Filter out special signaling bits */
			(state.inj_info.value()     &
			(uint32_t) ~0x3000) |
			(((uint64_t) state.inj_error.value()) << 32)
		);
	}

	if (state.intr_state.charged()) {
		v.write<Vmcb_buf::Int_control_ext::Int_shadow>(
		    state.intr_state.value());
	}
	/* Guest activity state (actv) not used by SVM */

	if (state.tsc_offset.charged()) {
		/* state.tsc not used by SVM */
		v.write<Vmcb_buf::Tsc_offset>(v.read<Vmcb_buf::Tsc_offset>() +
			                      state.tsc_offset.value());
	}

	if (state.efer.charged()) {
		v.write<Vmcb_buf::Efer>(state.efer.value());
	}

	/* pdpte not used by SVM */

	if (state.star.charged())  v.write<Vmcb_buf::Star>(state.star.value());
	if (state.cstar.charged()) v.write<Vmcb_buf::Cstar>(state.cstar.value());
	if (state.lstar.charged()) v.write<Vmcb_buf::Lstar>(state.lstar.value());
	if (state.fmask.charged()) v.write<Vmcb_buf::Sfmask>(state.lstar.value());
	if (state.kernel_gs_base.charged())
		v.write<Vmcb_buf::Kernel_gs_base>(state.kernel_gs_base.value());

	if (state.tpr.charged())
		v.write<Vmcb_buf::Int_control::V_tpr>(state.tpr.value());
	/* TPR threshold not used on AMD */
}


uint64_t Vmcb::handle_vm_exit()
{
	enum Svm_exitcodes : uint64_t
	{
		SVM_EXIT_INVALID = -1ULL,
		SVM_VMEXIT_INTR  =  0x60,
		SVM_VMEXIT_NPF   = 0x400,
	};

	uint64_t exitcode = v.read<Vmcb_buf::Exitcode>();
	switch (exitcode) {
		case SVM_EXIT_INVALID:
			error("VM: invalid SVM state!");
			break;
		case 0x40 ... 0x5f:
			error("VM: unhandled SVM exception ",
			              Hex(exitcode));
			break;
		case SVM_VMEXIT_INTR:
			exitcode = EXIT_PAUSED;
			break;
		case SVM_VMEXIT_NPF:
			exitcode = EXIT_NPF;
			break;
		default:
			break;
	}

	return exitcode;
}

void Vmcb::switch_world(Core::Cpu::Context &regs)
{
	/*
	 * We push the host context's physical address to trapno so that
	 * we can pop it later
	 */
	regs.trapno = root_vmcb_phys;
	asm volatile(
	    "fxrstor (%[fpu_context]);"
	    "mov %[guest_state], %%rax;"
	    "mov  %[regs], %%rsp;"
	    "popq %%r8;"
	    "popq %%r9;"
	    "popq %%r10;"
	    "popq %%r11;"
	    "popq %%r12;"
	    "popq %%r13;"
	    "popq %%r14;"
	    "popq %%r15;"
	    "add $8, %%rsp;" /* don't pop rax */
	    "popq %%rbx;"
	    "popq %%rcx;"
	    "popq %%rdx;"
	    "popq %%rdi;"
	    "popq %%rsi;"
	    "popq %%rbp;"
	    "clgi;"
	    "sti;"
	    "vmload;"
	    "vmrun;"
	    "vmsave;"
	    "popq %%rax;" /* get the physical address of the host VMCB from
	                     the stack */
	    "vmload;"
	    "stgi;" /* maybe enter the kernel to handle an external interrupt
	               that occured ... */
	    "nop;"
	    "cli;"        /* ... otherwise, just disable interrupts again */
	    "pushq %[trap_vmexit];" /* make the stack point to trapno, the right place
	                     to jump to _kernel_entry. We push 256 because
	                     this is outside of the valid range for interrupts
	                   */
	    "jmp _kernel_entry;" /* jump to _kernel_entry to save the
	                            GPRs without breaking any */
	    :
	    : [regs]        "r"(&regs.r8), [fpu_context] "r"(regs.fpu_context()),
	      [guest_state] "r"(vcpu_data.phys_addr + get_page_size()),
	      [trap_vmexit] "i"(TRAP_VMEXIT)
	    : "rax", "memory");
}
