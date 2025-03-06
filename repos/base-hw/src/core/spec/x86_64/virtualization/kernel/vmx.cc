/*
 * VMX virtualization
 * \author  Benjamin Lamowski
 * \date    2023-10-04
 */

/*
 * Copyright (C) 2023-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <cpu/cpu_state.h>
#include <hw/spec/x86_64/x86_64.h>
#include <kernel/cpu.h>
#include <platform.h>
#include <spec/x86_64/kernel/panic.h>
#include <spec/x86_64/virtualization/vmx.h>
#include <util/construct_at.h>
#include <util/mmio.h>

using Genode::addr_t;
using Kernel::Cpu;
using Kernel::Vm;
using Board::Vmcs;
using Board::Vmcs_buf;

extern "C"
{
	extern Genode::addr_t _kernel_entry;
}

Genode::uint32_t Vmcs::system_rev              = 0U;
Genode::uint32_t Vmcs::pinbased_allowed_0      = 0U;
Genode::uint32_t Vmcs::pinbased_allowed_1      = 0U;
Genode::uint32_t Vmcs::pri_exit_allowed_0      = 0U;
Genode::uint32_t Vmcs::pri_exit_allowed_1      = 0U;
Genode::uint32_t Vmcs::vm_entry_allowed_0      = 0U;
Genode::uint32_t Vmcs::vm_entry_allowed_1      = 0U;
Genode::uint32_t Vmcs::pri_procbased_allowed_0 = 0U;
Genode::uint32_t Vmcs::pri_procbased_allowed_1 = 0U;
Genode::uint32_t Vmcs::sec_procbased_allowed_0 = 0U;
Genode::uint32_t Vmcs::sec_procbased_allowed_1 = 0U;
Genode::uint64_t Vmcs::cr0_fixed0              = 0U;
Genode::uint64_t Vmcs::cr0_fixed1              = 0U;
Genode::uint64_t Vmcs::cr0_mask                = 0U;
Genode::uint64_t Vmcs::cr4_fixed0              = 0U;
Genode::uint64_t Vmcs::cr4_fixed1              = 0U;
Genode::uint64_t Vmcs::cr4_mask                = 0U;
extern int       __idt;

Vmcs * current_vmcs[Hw::Pc_board::NR_OF_CPUS]  = { nullptr };

/*
 * We need to push the artifical TRAP_VMEXIT value
 * to trapno after returning from vmlauch and before
 * jumping to _kernel_entry
 */
void kernel_entry_push_trap()
{
	asm volatile(
	      "pushq %[trap_val];" /* make the stack point to trapno, the right place */
	      "jmp _kernel_entry;"
	      :
	      : [trap_val] "i"(Board::TRAP_VMEXIT)
	      : "memory");
}

Vmcs_buf::Vmcs_buf(Genode::uint32_t system_rev)
{
	Genode::memset((void *) this, 0, sizeof(Vmcs_buf));
	rev = system_rev;
}

Vmcs::Vmcs(Genode::Vcpu_data &vcpu_data)
:
	Board::Virt_interface(vcpu_data)
{
	if (!system_rev)
		setup_vmx_info();

	Genode::construct_at<Vmcs_buf>((void *)(((addr_t) vcpu_data.virt_area)
	                                         + get_page_size()), system_rev);
}


void Vmcs::construct_host_vmcs()
{
	static Genode::Constructible<Vmcs_buf> host_vmcs_buf[NR_OF_CPUS];

	if (!host_vmcs_buf[_cpu_id].constructed()) {
		host_vmcs_buf[_cpu_id].construct(system_rev);

		Genode::addr_t host_vmcs_phys =
			Core::Platform::core_phys_addr(
				(addr_t)& host_vmcs_buf[_cpu_id]);

		vmxon(host_vmcs_phys);
	}
}

/*
 * Setup static VMX information. This only works well as long as Intel's E and P
 * cores report the same feature set.
 */
void Vmcs::setup_vmx_info()
{
	using Cpu = Hw::X86_64_cpu;


	/* Get revision */
	Cpu::Ia32_vmx_basic::access_t vmx_basic = Cpu::Ia32_vmx_basic::read();
	system_rev = Cpu::Ia32_vmx_basic::Rev::get(vmx_basic);


	/* Get pin-based controls */
	bool clear_controls =
		Cpu::Ia32_vmx_basic::Clear_controls::get(vmx_basic);


	Genode::uint64_t pinbased_ctls { };

	if (clear_controls)
		pinbased_ctls = Cpu::Ia32_vmx_true_pinbased_ctls::read();
	else
		pinbased_ctls = Cpu::Ia32_vmx_pinbased_ctls::read();

	pinbased_allowed_0 =
		Cpu::Ia32_vmx_pinbased_ctls::Allowed_0_settings::get(pinbased_ctls);

	/*
	 * Vol. 3C of the Intel SDM (September 2023):
	 * 25.6.1 Pin-Based VM-Execution Controls
	 * "Logical processors that support the 0-settings of any of these bits
	 * will support the VMX capability MSR IA32_VMX_TRUE_PIN- BASED_CTLS
	 * MSR, and software should consult this MSR to discover support for the
	 * 0-settings of these bits. Software that is not aware of the
	 * functionality of any one of these bits should set that bit to 1.
	 */
	Pin_based_execution_controls::Bit_1::set(pinbased_allowed_0);
	Pin_based_execution_controls::Bit_2::set(pinbased_allowed_0);
	Pin_based_execution_controls::Bit_4::set(pinbased_allowed_0);
	pinbased_allowed_1 =
	    Cpu::Ia32_vmx_pinbased_ctls::Allowed_1_settings::get(pinbased_ctls);


	/* Get entry controls */
	Genode::uint64_t vm_entry_ctls { };

	if (clear_controls)
		vm_entry_ctls = Cpu::Ia32_vmx_true_entry_ctls::read();
	else
		vm_entry_ctls = Cpu::Ia32_vmx_entry_ctls::read();

	vm_entry_allowed_0 =
		Cpu::Ia32_vmx_entry_ctls::Allowed_0_settings::get(vm_entry_ctls);
	vm_entry_allowed_1 =
		Cpu::Ia32_vmx_entry_ctls::Allowed_1_settings::get(vm_entry_ctls);


	/* Get primary exit controls */
	Genode::uint64_t pri_exit_ctls { };

	if (clear_controls)
		pri_exit_ctls = Cpu::Ia32_vmx_true_exit_ctls::read();
	else
		pri_exit_ctls = Cpu::Ia32_vmx_exit_ctls::read();

	pri_exit_allowed_0 =
		Cpu::Ia32_vmx_exit_ctls::Allowed_0_settings::get(pri_exit_ctls);
	pri_exit_allowed_1 =
		Cpu::Ia32_vmx_exit_ctls::Allowed_1_settings::get(pri_exit_ctls);

	/* Get primary proc-based exit controls */
	Genode::uint64_t pri_procbased_ctls { };

	if (clear_controls)
		pri_procbased_ctls = Cpu::Ia32_vmx_true_procbased_ctls::read();
	else
		pri_procbased_ctls = Cpu::Ia32_vmx_procbased_ctls::read();

	pri_procbased_allowed_0 =
		Cpu::Ia32_vmx_procbased_ctls::Allowed_0_settings::get(
			pri_procbased_ctls);
	pri_procbased_allowed_1 =
		Cpu::Ia32_vmx_procbased_ctls::Allowed_1_settings::get(
			pri_procbased_ctls);

	/*
	 * Make sure that required features are available.
	 * At this point the VM session is already established.
	 * To our knowledge the required feature set should be available on any
	 * x86_64 processor that indicated VMX support, so we resolve to panic
	 * if the required feature set is not available.
	 */
	if (!Primary_proc_based_execution_controls::
	        Activate_secondary_controls::get(pri_procbased_allowed_1)) {
		Kernel::panic("Processor does not support secondary controls");
	}

	/* Get secondary proc-based exec controls */
	Cpu::Ia32_vmx_procbased_ctls2::access_t sec_procbased_ctls =
		Cpu::Ia32_vmx_procbased_ctls2::read();
	sec_procbased_allowed_0 =
		Cpu::Ia32_vmx_procbased_ctls::Allowed_0_settings::get(
			sec_procbased_ctls);
	sec_procbased_allowed_1 =
		Cpu::Ia32_vmx_procbased_ctls::Allowed_1_settings::get(
			sec_procbased_ctls);

	if (!Secondary_proc_based_execution_controls::Enable_ept::get(
		sec_procbased_allowed_1)) {
		Kernel::panic("Processor does not support nested page tables");
	}

	if (!Secondary_proc_based_execution_controls::Unrestricted_guest::get(
		sec_procbased_allowed_1)) {
		Kernel::panic("Processor does not support Unrestricted guest mode");
	}

	if (!Primary_proc_based_execution_controls::Use_tpr_shadow::get(
		pri_procbased_allowed_1)) {
		Kernel::panic("Processor does not support VTPR");
	}

	/* CR0 and CR4 fixed values */
	cr0_fixed0 = Cpu::Ia32_vmx_cr0_fixed0::read();
	cr0_fixed1 = Cpu::Ia32_vmx_cr0_fixed1::read();

	/*
	 * We demand that unrestriced guest mode is used, hence don't force PE
	 * and PG
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 24.8 Restrictions on VMX Operation
	 * Yes, forced-to-one bits are in fact read from IA32_VMX_CR0_FIXED0.
	 */
	Cpu::Cr0::Pe::clear(cr0_fixed0);
	Cpu::Cr0::Pg::clear(cr0_fixed0);

	cr0_mask = ~cr0_fixed1 | cr0_fixed0;
	Cpu::Cr0::Cd::set(cr0_mask);
	Cpu::Cr0::Nw::set(cr0_mask);

	cr4_fixed0 = Cpu::Ia32_vmx_cr4_fixed0::read();
	cr4_fixed1 = Cpu::Ia32_vmx_cr4_fixed1::read();
	cr4_mask   = ~cr4_fixed1 | cr4_fixed0;
}

void Vmcs::initialize(Kernel::Cpu &cpu, Genode::addr_t page_table_phys)
{
	using Cpu = Hw::X86_64_cpu;

	/* Enable VMX */
	Cpu::Ia32_feature_control::access_t feature_control =
	    Cpu::Ia32_feature_control::read();
	if (!Cpu::Ia32_feature_control::Vmx_no_smx::get(feature_control)) {
		Genode::log("Enabling VMX.");
		if (!Cpu::Ia32_feature_control::Lock::get(feature_control)) {
			Cpu::Ia32_feature_control::Vmx_no_smx::set(feature_control, 1);
			Cpu::Ia32_feature_control::Lock::set(feature_control, 1);
			Cpu::Ia32_feature_control::write(feature_control);
		} else {
			/*
			 * Since the lock condition has been checked in
			 * Hw::Virtualization_support::has_vmx(), this should never happen.
			 */
			Kernel::panic("VMX feature disabled");
		}
	}

	Cpu::Cr4::access_t cr4 = Cpu::Cr4::read();
	Cpu::Cr4::Vmxe::set(cr4);
	Cpu::Cr4::write(cr4);

	_cpu_id =  cpu.id();

	construct_host_vmcs();

	Genode::construct_at<Virtual_apic_state>(
			(void *)(((addr_t) vcpu_data.virt_area) + 2 * get_page_size()));


	vmclear(vcpu_data.phys_addr + get_page_size());
	_load_pointer();

	prepare_vmcs();

	/*
	 * Set the VMCS link pointer to ~0UL according to spec
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 25.4.2 Guest Non-Register State: vmcs link pointer
	 */
	write(E_VMCS_LINK_POINTER, ~0ULL);

	/*
	 * Set up the Embedded Page Table Pointer
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 25.6.11 Extended-Page-Table Pointer (EPTP)
	 */
	struct Ept_ptr : Genode::Register<64> {
		struct Memory_type             : Bitfield< 0,3> { };
		struct Ept_walk_length_minus_1 : Bitfield< 3,3> { };
		struct Phys_addr_4k_aligned    : Bitfield<12,51> { };
	};

	enum Memory_type {
		EPT_UNCACHEABLE = 0,
		EPT_WRITEBACK   = 6,
	};

	/* We have a 4 level page table */
	const uint8_t ept_length = 4;
	Ept_ptr::access_t ept_ptr { 0 };
	Ept_ptr::Memory_type::set(ept_ptr, EPT_WRITEBACK);
	Ept_ptr::Ept_walk_length_minus_1::set(ept_ptr, ept_length - 1);
	Ept_ptr::Phys_addr_4k_aligned::set(ept_ptr, page_table_phys >> 12);
	write(E_EPT_POINTER, ept_ptr);


	write(E_HOST_IA32_EFER, Cpu::Ia32_efer::read());

	/*
	 * If this looks the wrong way around then you are in good company.
	 * For details, and a nonchalant explanation of this cursed interface,
	 * see Vol. 3D of the Intel SDM (September 2023):
	 * A.7 VMX-Fixed Bits in CR0
	 */
	Genode::uint64_t cr0 = Cpu::Cr0::read();

	cr0 = (cr0 & cr0_fixed1) | cr0_fixed0;
	/* NW and CD shouln'd be set by hw in the first place, but to be sure. */
	Cpu::Cr0::Nw::clear(cr0);
	Cpu::Cr0::Cd::clear(cr0);
	Cpu::Cr0::write(cr0);
	write(E_HOST_CR0, cr0);
	write(E_CR0_GUEST_HOST_MASK, cr0_mask);

	write(E_HOST_CR3, Cpu::Cr3::read());

	/* See above */
	cr4 = (cr4 & cr4_fixed1) | cr4_fixed0;
	Cpu::Cr4::write(cr4);
	write(E_HOST_CR4, cr4);
	write(E_CR4_GUEST_HOST_MASK, cr4_mask);


	/* offsets from GDT in src/core/spec/x86_64/cpu.h */
	write(E_HOST_CS_SELECTOR, 0x8);

	write(E_HOST_FS_SELECTOR, 0);
	write(E_HOST_GS_SELECTOR, 0);

	write(E_HOST_TR_BASE, reinterpret_cast<Genode::addr_t>(&(cpu.tss)));
	/* see Cpu::Tss::init() / the tss_descriptor is in slot 5 of the GDT */
	write(E_HOST_TR_SELECTOR, 0x28);
	write(E_HOST_GDTR_BASE, reinterpret_cast<Genode::addr_t>(&(cpu.gdt)));
	write(E_HOST_IDTR_BASE, reinterpret_cast<Genode::addr_t>(&__idt));

	write(E_HOST_IA32_SYSENTER_ESP, reinterpret_cast<Genode::addr_t>(&(cpu.tss.rsp[0])));
	write(E_HOST_IA32_SYSENTER_CS, 0x8);
	write(E_HOST_IA32_SYSENTER_EIP, reinterpret_cast<Genode::uint64_t>(&kernel_entry_push_trap));

	/*
	 * Set the RSP to trapno, so that _kernel_entry will save the registers
	 * into the right fields.
	 */
	write(E_HOST_RSP, cpu.stack_start() - 568);
	write(E_HOST_RIP, reinterpret_cast<Genode::uint64_t>(&kernel_entry_push_trap));
}


/*
 * Enforce VMX intercepts
 */
void Vmcs::enforce_execution_controls(Genode::uint32_t desired_primary,
                                      Genode::uint32_t desired_secondary)
{
	/*
	 * Processor-Based VM-Execution Controls
	 *
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 25.6.2 Processor-Based VM-Execution Controls
	 */

	/* Exit on HLT instruction */
	Primary_proc_based_execution_controls::Hlt_exiting::set(desired_primary);

	/* Enforce use of nested paging */
	Primary_proc_based_execution_controls::Invlpg_exiting::clear(desired_primary);
	Primary_proc_based_execution_controls::Cr3_load_exiting::clear(desired_primary);
	Primary_proc_based_execution_controls::Cr3_store_exiting::clear(desired_primary);
	Primary_proc_based_execution_controls::Activate_secondary_controls::set(desired_primary);
	Secondary_proc_based_execution_controls::Enable_ept::set(desired_secondary);
	Secondary_proc_based_execution_controls::Unrestricted_guest::set(desired_secondary);
	Secondary_proc_based_execution_controls::Enable_vpid::clear(desired_secondary);

	/*
	 * Always exit on IO and MSR accesses.
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 26.1.3 Instructions That Cause VM Exits Conditionally
	 */
	Primary_proc_based_execution_controls::Unconditional_io_exiting::set(desired_primary);
	Primary_proc_based_execution_controls::Use_io_bitmaps::clear(desired_primary);
	Primary_proc_based_execution_controls::Use_msr_bitmaps::clear(desired_primary);

	/*
	 * Force a Virtual task-priority register (VTPR)
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 30.1.1 Virtualized APIC Registers
	 */
	Primary_proc_based_execution_controls::Use_tpr_shadow::set(desired_primary);

	Genode::uint32_t pri_procbased_set =
		(desired_primary | pri_procbased_allowed_0)
		& pri_procbased_allowed_1;
	write(E_PRI_PROC_BASED_VM_EXEC_CTRL, pri_procbased_set);

	Genode::uint32_t sec_procbased_set =
		(desired_secondary | sec_procbased_allowed_0)
		& sec_procbased_allowed_1;
	write(E_SEC_PROC_BASED_VM_EXEC_CTRL, sec_procbased_set);
}

void Vmcs::prepare_vmcs()
{
	/*
	 * Pin-Based VM-Execution Controls
	 *
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 25.6.1 Pin-Based VM-Execution Controls
	 */
	Genode::uint32_t pinbased_want = 0U;
	Pin_based_execution_controls::External_interrupt_exiting::set(pinbased_want);
	Pin_based_execution_controls::Nmi_exiting::set(pinbased_want);
	Pin_based_execution_controls::Virtual_nmis::set(pinbased_want);
	Genode::uint32_t pinbased_set = (pinbased_want | pinbased_allowed_0)
	                              & pinbased_allowed_1;
	write(E_PIN_BASED_VM_EXECUTION_CTRL, pinbased_set);

	/*
	 * Primary VM-Exit Controls
	 *
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * Table 25-13. Definitions of Primary VM-Exit Controls
	 */
	Genode::uint32_t primary_exit_want = 0U;
	Primary_vm_exit_controls::Save_debug_controls::set(primary_exit_want);
	Primary_vm_exit_controls::Host_address_space_size::set(primary_exit_want);
	Primary_vm_exit_controls::Ack_interrupt_on_exit::set(primary_exit_want);
	Primary_vm_exit_controls::Save_ia32_efer::set(primary_exit_want);
	Primary_vm_exit_controls::Load_ia32_efer::set(primary_exit_want);
	Genode::uint32_t primary_exit_set =
		(primary_exit_want | pri_exit_allowed_0) & pri_exit_allowed_1;
	write(E_PRIMARY_VM_EXIT_CONTROLS, primary_exit_set);

	/*
	 * VM-Entry Controls
	 *
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * Table 25-13. Definitions of Primary VM-Exit Controls
	 * 25.8.1 VM-Entry Controls
	 */
	Genode::uint32_t vm_entry_want = 0U;
	Vm_entry_controls::Load_debug_controls::set(vm_entry_want);
	Vm_entry_controls::Load_ia32_efer::set(vm_entry_want);
	Genode::uint32_t vm_entry_set =
		(vm_entry_want | vm_entry_allowed_0) & vm_entry_allowed_1;
	write(E_VM_ENTRY_CONTROLS, vm_entry_set);


	enforce_execution_controls(0U, 0U);

	write(E_VM_EXIT_MSR_STORE_ADDRESS, msr_phys_addr(&guest_msr_store_area));
	write(E_VM_EXIT_MSR_STORE_COUNT, Board::Msr_store_area::get_count());
	write(E_VM_ENTRY_MSR_LOAD_ADDRESS, msr_phys_addr(&guest_msr_store_area));
	write(E_VM_ENTRY_MSR_LOAD_COUNT, Board::Msr_store_area::get_count());

	write(E_VM_EXIT_MSR_LOAD_ADDRESS, msr_phys_addr(&host_msr_store_area));
	write(E_VM_EXIT_MSR_LOAD_COUNT, Board::Msr_store_area::get_count());

	write(E_VIRTUAL_APIC_ADDRESS, vcpu_data.phys_addr + 2 * get_page_size());

	/*
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 26.2 Other Causes Of VM Exits: Exceptions
	 */
	write(E_EXCEPTION_BITMAP, (1 << Genode::Cpu_state::ALIGNMENT_CHECK) |
	                          (1 << Genode::Cpu_state::DEBUG));
	write(E_PAGE_FAULT_ERROR_CODE_MASK, 0U);
	write(E_PAGE_FAULT_ERROR_CODE_MATCH, 0U);

	/*
	 * For now, don't use CR3 targets.
	 * For details, see Vol. 3C of the Intel SDM (September 2023):
	 * 25.6.7 CR3-Target Controls
	 */
	write(E_CR3_TARGET_COUNT, 0U);
}

void Vmcs::write_vcpu_state(Genode::Vcpu_state &state)
{
	using Range   = Genode::Vcpu_state::Range;
	using Segment = Genode::Vcpu_state::Segment;
	using Cpu     = Hw::X86_64_cpu;

	using Genode::uint16_t;
	using Genode::uint32_t;

	_load_pointer();

	state.ip.charge(read(E_GUEST_RIP));
	state.ip_len.charge(read(E_VM_EXIT_INSTRUCTION_LENGTH));

	state.flags.charge(read(E_GUEST_RFLAGS));
	state.sp.charge(read(E_GUEST_RSP));

	state.dr7.charge(read(E_GUEST_DR7));

	state.cr0.charge(read(E_GUEST_CR0));
	state.cr2.charge(Cpu::Cr2::read());
	state.cr3.charge(read(E_GUEST_CR3));
	state.cr4.charge(read(E_GUEST_CR4));

	state.cs.charge(Segment {
				.sel   = static_cast<uint16_t>(read(E_GUEST_CS_SELECTOR)),
				.ar    = _ar_convert_to_genode(read(E_GUEST_CS_ACCESS_RIGHTS)),
				.limit = static_cast<uint32_t>(read(E_GUEST_CS_LIMIT)),
				.base  = read(E_GUEST_CS_BASE)
			});

	state.ss.charge(Segment {
				.sel   = static_cast<uint16_t>(read(E_GUEST_SS_SELECTOR)),
				.ar    = _ar_convert_to_genode(read(E_GUEST_SS_ACCESS_RIGHTS)),
				.limit = static_cast<uint32_t>(read(E_GUEST_SS_LIMIT)),
				.base  = read(E_GUEST_SS_BASE)
			});

	state.es.charge(Segment {
				.sel   = static_cast<uint16_t>(read(E_GUEST_ES_SELECTOR)),
				.ar    = _ar_convert_to_genode(read(E_GUEST_ES_ACCESS_RIGHTS)),
				.limit = static_cast<uint32_t>(read(E_GUEST_ES_LIMIT)),
				.base  = read(E_GUEST_ES_BASE)
			});

	state.ds.charge(Segment {
				.sel   = static_cast<uint16_t>(read(E_GUEST_DS_SELECTOR)),
				.ar    = _ar_convert_to_genode(read(E_GUEST_DS_ACCESS_RIGHTS)),
				.limit = static_cast<uint32_t>(read(E_GUEST_DS_LIMIT)),
				.base  = read(E_GUEST_DS_BASE)
			});

	state.fs.charge(Segment {
				.sel   = static_cast<uint16_t>(read(E_GUEST_FS_SELECTOR)),
				.ar    = _ar_convert_to_genode(read(E_GUEST_FS_ACCESS_RIGHTS)),
				.limit = static_cast<uint32_t>(read(E_GUEST_FS_LIMIT)),
				.base  = read(E_GUEST_FS_BASE)
			});

	state.gs.charge(Segment {
				.sel   = static_cast<uint16_t>(read(E_GUEST_GS_SELECTOR)),
				.ar    = _ar_convert_to_genode(read(E_GUEST_GS_ACCESS_RIGHTS)),
				.limit = static_cast<uint32_t>(read(E_GUEST_GS_LIMIT)),
				.base  = read(E_GUEST_GS_BASE)
			});

	state.tr.charge(Segment {
				.sel   = static_cast<uint16_t>(read(E_GUEST_TR_SELECTOR)),
				.ar    = _ar_convert_to_genode(read(E_GUEST_TR_ACCESS_RIGHTS)),
				.limit = static_cast<uint32_t>(read(E_GUEST_TR_LIMIT)),
				.base  = read(E_GUEST_TR_BASE)
			});

	state.ldtr.charge(Segment {
				.sel   = static_cast<uint16_t>(read(E_GUEST_LDTR_SELECTOR)),
				.ar    = _ar_convert_to_genode(read(E_GUEST_LDTR_ACCESS_RIGHTS)),
				.limit = static_cast<uint32_t>(read(E_GUEST_LDTR_LIMIT)),
				.base  = read(E_GUEST_LDTR_BASE)
			});

	state.gdtr.charge(Range {
				.limit = static_cast<uint32_t>(read(E_GUEST_GDTR_LIMIT)),
				.base  = read(E_GUEST_GDTR_BASE)
			});

	state.idtr.charge(Range {
				.limit = static_cast<uint32_t>(read(E_GUEST_IDTR_LIMIT)),
				.base  = read(E_GUEST_IDTR_BASE)
			});


	state.sysenter_cs.charge(read(E_IA32_SYSENTER_CS));
	state.sysenter_sp.charge(read(E_GUEST_IA32_SYSENTER_ESP));
	state.sysenter_ip.charge(read(E_GUEST_IA32_SYSENTER_EIP));

	state.qual_primary.charge(read(E_EXIT_QUALIFICATION));
	state.qual_secondary.charge(read(E_GUEST_PHYSICAL_ADDRESS));

	/* Charging ctrl_primary and ctrl_secondary breaks Virtualbox 6 */

	if (state.exit_reason == EXIT_PAUSED || state.exit_reason == VMX_EXIT_INVGUEST) {
		state.inj_info.charge(static_cast<uint32_t>(read(E_VM_ENTRY_INTERRUPT_INFO_FIELD)));
		state.inj_error.charge(static_cast<uint32_t>(read(E_VM_ENTRY_EXCEPTION_ERROR_CODE)));

	} else {
		state.inj_info.charge(static_cast<uint32_t>(read(E_IDT_VECTORING_INFORMATION_FIELD)));
		state.inj_error.charge(static_cast<uint32_t>(read(E_IDT_VECTORING_ERROR_CODE)));
	}

	state.intr_state.charge(
	    static_cast<uint32_t>(read(E_GUEST_INTERRUPTIBILITY_STATE)));
	state.actv_state.charge(
	    static_cast<uint32_t>(read(E_GUEST_ACTIVITY_STATE)));

	state.tsc.charge(Hw::Tsc::rdtsc());
	state.tsc_offset.charge(read(E_TSC_OFFSET));

	state.efer.charge(read(E_GUEST_IA32_EFER));

	state.pdpte_0.charge(read(E_GUEST_PDPTE0));
	state.pdpte_1.charge(read(E_GUEST_PDPTE1));
	state.pdpte_2.charge(read(E_GUEST_PDPTE2));
	state.pdpte_3.charge(read(E_GUEST_PDPTE3));

	state.star.charge(guest_msr_store_area.star.get());
	state.lstar.charge(guest_msr_store_area.lstar.get());
	state.cstar.charge(guest_msr_store_area.cstar.get());
	state.fmask.charge(guest_msr_store_area.fmask.get());
	state.kernel_gs_base.charge(guest_msr_store_area.kernel_gs_base.get());

	Virtual_apic_state *virtual_apic_state =
		reinterpret_cast<Virtual_apic_state *>(
			((addr_t) vcpu_data.virt_area) + 2 * get_page_size());
	state.tpr.charge(virtual_apic_state->get_vtpr());
	state.tpr_threshold.charge(
		static_cast<uint32_t>(read(E_TPR_THRESHOLD)));
}


void Vmcs::read_vcpu_state(Genode::Vcpu_state &state)
{
	_load_pointer();

	if (state.flags.charged()) {
		write(E_GUEST_RFLAGS, state.flags.value());
	}

	if (state.sp.charged()) {
		write(E_GUEST_RSP, state.sp.value());
	}

	if (state.ip.charged()) {
		write(E_GUEST_RIP, state.ip.value());
		write(E_VM_ENTRY_INSTRUCTION_LENGTH, state.ip_len.value());
	}

	if (state.dr7.charged()) {
		write(E_GUEST_DR7, state.dr7.value());
	}

	if (state.cr0.charged() || state.cr2.charged() ||
	    state.cr3.charged() || state.cr4.charged()) {
		write(E_GUEST_CR0, (state.cr0.value() & ~cr0_mask & cr0_fixed1) | cr0_fixed0);
		write(E_CR0_READ_SHADOW, (state.cr0.value() & cr0_fixed1) | cr0_fixed0);
		cr2 = state.cr2.value();
		write(E_GUEST_CR3, state.cr3.value());
		write(E_GUEST_CR4, (state.cr4.value() & cr4_fixed1) | cr4_fixed0);
		write(E_CR4_READ_SHADOW, (state.cr4.value() & cr4_fixed1) | cr4_fixed0);
	}

	if (state.cs.charged() || state.ss.charged()) {
		write(E_GUEST_CS_SELECTOR,        state.cs.value().sel);
		write(E_GUEST_CS_ACCESS_RIGHTS,    _ar_convert_to_intel(state.cs.value().ar));
		write(E_GUEST_CS_LIMIT,           state.cs.value().limit);
		write(E_GUEST_CS_BASE,            state.cs.value().base);

		write(E_GUEST_SS_SELECTOR,        state.ss.value().sel);
		write(E_GUEST_SS_ACCESS_RIGHTS,    _ar_convert_to_intel(state.ss.value().ar));
		write(E_GUEST_SS_LIMIT,           state.ss.value().limit);
		write(E_GUEST_SS_BASE,            state.ss.value().base);
	}

	if (state.es.charged() ||   state.ds.charged()) {
		write(E_GUEST_ES_SELECTOR,        state.es.value().sel);
		write(E_GUEST_ES_ACCESS_RIGHTS,    _ar_convert_to_intel(state.es.value().ar));
		write(E_GUEST_ES_LIMIT,           state.es.value().limit);
		write(E_GUEST_ES_BASE,            state.es.value().base);

		write(E_GUEST_DS_SELECTOR,        state.ds.value().sel);
		write(E_GUEST_DS_ACCESS_RIGHTS,    _ar_convert_to_intel(state.ds.value().ar));
		write(E_GUEST_DS_LIMIT,           state.ds.value().limit);
		write(E_GUEST_DS_BASE,            state.ds.value().base);
	}

	if (state.fs.charged() ||   state.gs.charged()) {
		write(E_GUEST_FS_SELECTOR,        state.fs.value().sel);
		write(E_GUEST_FS_ACCESS_RIGHTS,    _ar_convert_to_intel(state.fs.value().ar));
		write(E_GUEST_FS_LIMIT,           state.fs.value().limit);
		write(E_GUEST_FS_BASE,            state.fs.value().base);

		write(E_GUEST_GS_SELECTOR,        state.gs.value().sel);
		write(E_GUEST_GS_ACCESS_RIGHTS,    _ar_convert_to_intel(state.gs.value().ar));
		write(E_GUEST_GS_LIMIT,           state.gs.value().limit);
		write(E_GUEST_GS_BASE,            state.gs.value().base);
	}

	if (state.tr.charged()) {
		write(E_GUEST_TR_SELECTOR,        state.tr.value().sel);
		write(E_GUEST_TR_ACCESS_RIGHTS,    _ar_convert_to_intel(state.tr.value().ar));
		write(E_GUEST_TR_LIMIT,           state.tr.value().limit);
		write(E_GUEST_TR_BASE,            state.tr.value().base);
	}

	if (state.ldtr.charged()) {
		write(E_GUEST_LDTR_SELECTOR,      state.ldtr.value().sel);
		write(E_GUEST_LDTR_ACCESS_RIGHTS,  _ar_convert_to_intel(state.ldtr.value().ar));
		write(E_GUEST_LDTR_LIMIT,         state.ldtr.value().limit);
		write(E_GUEST_LDTR_BASE,          state.ldtr.value().base);
	}

	if (state.gdtr.charged()) {
		write(E_GUEST_GDTR_LIMIT,         state.gdtr.value().limit);
		write(E_GUEST_GDTR_BASE,          state.gdtr.value().base);
	}

	if (state.idtr.charged()) {
		write(E_GUEST_IDTR_LIMIT,         state.idtr.value().limit);
		write(E_GUEST_IDTR_BASE,          state.idtr.value().base);
	}

	if (state.sysenter_cs.charged() || state.sysenter_sp.charged() ||
	    state.sysenter_ip.charged()) {
		write(E_IA32_SYSENTER_CS, state.sysenter_cs.value());
		write(E_GUEST_IA32_SYSENTER_ESP, state.sysenter_sp.value());
		write(E_GUEST_IA32_SYSENTER_EIP, state.sysenter_ip.value());
	}

	if (state.ctrl_primary.charged() || state.ctrl_secondary.charged()) {
		enforce_execution_controls(state.ctrl_primary.value(),
		                           state.ctrl_secondary.value());
	}

	if (state.inj_info.charged() || state.inj_error.charged()) {
		Genode::uint32_t pri_controls = static_cast<Genode::uint32_t> (read(E_PRI_PROC_BASED_VM_EXEC_CTRL));
		Genode::uint32_t sec_controls = static_cast<Genode::uint32_t> (read(E_SEC_PROC_BASED_VM_EXEC_CTRL));
		bool set_controls = false;

		if (state.inj_info.value() & 0x1000) {
			if (!Primary_proc_based_execution_controls::Interrupt_window_exiting::get(pri_controls)) {
				Primary_proc_based_execution_controls::Interrupt_window_exiting::set(pri_controls);
				set_controls = true;
			}
		} else {
			if (Primary_proc_based_execution_controls::Interrupt_window_exiting::get(pri_controls)) {
				Primary_proc_based_execution_controls::Interrupt_window_exiting::clear(pri_controls);
				set_controls = true;
			}
		}

		if (state.inj_info.value() & 0x2000) {
			if (!Primary_proc_based_execution_controls::Nmi_window_exiting::get(pri_controls)) {
				Primary_proc_based_execution_controls::Nmi_window_exiting::set(pri_controls);
				set_controls = true;
			}
		} else {
			if (Primary_proc_based_execution_controls::Nmi_window_exiting::get(pri_controls)) {
				Primary_proc_based_execution_controls::Nmi_window_exiting::clear(pri_controls);
				set_controls = true;
			}
		}

		if (set_controls)
			enforce_execution_controls(pri_controls, sec_controls);

		write(E_VM_ENTRY_INTERRUPT_INFO_FIELD,
			/* Filter out special signaling bits */
			(state.inj_info.value()     &
			(Genode::uint32_t) ~0x3000));

		write(E_VM_ENTRY_EXCEPTION_ERROR_CODE, state.inj_error.value());
	}

	if (state.intr_state.charged()) {
		write(E_GUEST_INTERRUPTIBILITY_STATE, state.intr_state.value());
	}

	if (state.actv_state.charged()) {
		write(E_GUEST_ACTIVITY_STATE, state.actv_state.value());
	}

	if (state.tsc_offset.charged()) {
		/* state.tsc not used by SVM */
		write(E_TSC_OFFSET, state.tsc_offset.value());
	}

	if (state.efer.charged()) {
		auto efer = state.efer.value();
		write(E_GUEST_IA32_EFER, efer);

		Vm_entry_controls::access_t entry_controls = static_cast<Vm_entry_controls::access_t>(read(E_VM_ENTRY_CONTROLS));
		if (Cpu::Ia32_efer::Lma::get(efer))
			Vm_entry_controls::Ia32e_mode_guest::set(entry_controls);
		else
			Vm_entry_controls::Ia32e_mode_guest::clear(entry_controls);

		write(E_VM_ENTRY_CONTROLS, entry_controls);
	}

	if (state.pdpte_0.charged() || state.pdpte_1.charged() ||
	    state.pdpte_1.charged() || state.pdpte_2.charged()) {
		write(E_GUEST_PDPTE0, state.pdpte_0.value());
		write(E_GUEST_PDPTE1, state.pdpte_1.value());
		write(E_GUEST_PDPTE2, state.pdpte_2.value());
		write(E_GUEST_PDPTE3, state.pdpte_3.value());
	}

	if (state.star.charged() || state.lstar.charged() ||
	    state.cstar.charged() || state.fmask.charged() ||
	    state.kernel_gs_base.charged()) {
		guest_msr_store_area.star.set(state.star.value());
		guest_msr_store_area.lstar.set(state.lstar.value());
		guest_msr_store_area.cstar.set(state.cstar.value());
		guest_msr_store_area.fmask.set(state.fmask.value());
		guest_msr_store_area.kernel_gs_base.set(state.kernel_gs_base.value());
	}

	Virtual_apic_state * virtual_apic_state =
		reinterpret_cast<Virtual_apic_state *>(((addr_t) vcpu_data.virt_area)
		                                       + 2 * get_page_size());

	if (state.tpr.charged()) {
		virtual_apic_state->set_vtpr(state.tpr.value());
		write(E_TPR_THRESHOLD, state.tpr_threshold.value());
	}
}

void Vmcs::switch_world(Core::Cpu::Context &regs, addr_t)
{
	_load_pointer();

	save_host_msrs();

	Cpu::Cr2::write(cr2);

	regs.trapno = TRAP_VMEXIT;
	asm volatile(
	    "fxrstor (%[fpu_context]);"
	    "mov  %[regs], %%rsp;"
	    "popq %%r8;"
	    "popq %%r9;"
	    "popq %%r10;"
	    "popq %%r11;"
	    "popq %%r12;"
	    "popq %%r13;"
	    "popq %%r14;"
	    "popq %%r15;"
	    "popq %%rax;"
	    "popq %%rbx;"
	    "popq %%rcx;"
	    "popq %%rdx;"
	    "popq %%rdi;"
	    "popq %%rsi;"
	    "popq %%rbp;"
	    "vmresume;"
	    "vmlaunch;"
	    :
	    : [regs]           "r"(&regs.r8),
	      [fpu_context]    "r"(&regs.fpu_context())
	    : "memory");
	/*
	 * Usually when exiting guest mode, VMX will jump to the address
	 * provided in E_HOST_RIP; in our case: _kernel_entry.
	 *
	 * Execution continuing after the vmlaunch instruction indicates an
	 * error in setting up VMX that should never happen. If we regularly
	 * return from this method, the vCPU thread will be removed from the
	 * scheduler.
	 *
	 * For error codes, see Intel SDM (September 2023) Vol. 3C
	 * 31.4 Vm Instruction Error Numbers
	 */
	error("VM: execution error: ", Genode::Hex(read(Vmcs::E_VM_INSTRUCTION_ERROR)));
}

/*
 * Store MSRs to the Host MSR Store Area so that VMX restores them on VM exit
 *
 * For details, see Vol. 3C of the Intel SDM (September 2023):
 * 28.6 Loading MSRs
 */
void Vmcs::save_host_msrs()
{
	using Cpu = Hw::X86_64_cpu;

	host_msr_store_area.star.set(Cpu::Ia32_star::read());
	host_msr_store_area.lstar.set(Cpu::Ia32_lstar::read());
	host_msr_store_area.cstar.set(Cpu::Ia32_cstar::read());
	host_msr_store_area.fmask.set(Cpu::Ia32_fmask::read());
	host_msr_store_area.kernel_gs_base.set(
		Cpu::Ia32_kernel_gs_base::read());
}


void Vmcs::_load_pointer()
{
	if (current_vmcs[_cpu_id] == this)
		return;

	current_vmcs[_cpu_id] = this;

	vmptrld(vcpu_data.phys_addr + get_page_size());
}


uint64_t Vmcs::handle_vm_exit()
{
	cr2 = Cpu::Cr2::read();
	uint64_t exitcode = read(E_EXIT_REASON) & 0xFFFF;

	switch (exitcode) {
		case VMX_EXIT_NMI:
			/*
			 * XXX We might need to handle host NMIs encoded in
			 * the VM_EXIT_INTERRUPT_INFORMATION field, so let's
			 * issue a warning.
			 */
			Genode::warning("VMX NMI exit occured");
			break;
		case VMX_EXIT_INTR:
			exitcode = EXIT_PAUSED;
			break;
		default:
			break;
	}

	return exitcode;
}
