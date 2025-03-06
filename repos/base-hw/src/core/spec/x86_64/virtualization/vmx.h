/*
 * \brief   VMX data structure
 * \author  Benjamin Lamowski
 * \date    2023-09-26
 */

/*
 * Copyright (C) 2023-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PC__VMX_H_
#define _INCLUDE__SPEC__PC__VMX_H_

#include <base/internal/page_size.h>
#include <cpu.h>
#include <cpu/vcpu_state.h>
#include <cpu/vcpu_state_virtualization.h>
#include <hw/assert.h>
#include <hw/spec/x86_64/cpu.h>
#include <spec/x86_64/virtualization/virt_interface.h>
#include <util/register.h>
#include <util/string.h>

using Genode::addr_t;
using Genode::uint16_t;
using Genode::uint32_t;
using Genode::uint64_t;

namespace Kernel { class Cpu; }

namespace Board
{
	struct Vmcs;
	struct Vmcs_buf;
	struct Msr_store_area;
	struct Virtual_apic_state;
}

/*
 * VMX exitcodes, incomplete list.
 *
 * For details, see Vol. 3C of the Intel SDM (September 2023):
 * Table C-1. Basic Exit Reasons
 */
enum Vmx_exitcodes : uint32_t {
	VMX_EXIT_NMI      =  0,
	VMX_EXIT_INTR     =  1,
	VMX_EXIT_INVGUEST = 33,
};


/*
 * MSR-store area
 *
 * For details, see Vol. 3C of the Intel SDM (September 2023):
 * 25.7.2 VM-Exit Controls for MSRs
 */
struct
alignas(16)
Board::Msr_store_area
{
	struct Msr_entry
	{
		uint32_t msr_index = 0U;
		uint32_t _reserved = 0U;
		uint64_t msr_data  = 0U;

		void set(uint64_t data)
		{
			msr_data = data;
		}

		uint64_t get()
		{
			return msr_data;
		}
	} __attribute__((packed));

	Msr_entry  star          { 0xC0000081 };
	Msr_entry lstar          { 0xC0000082 };
	Msr_entry cstar          { 0xC0000083 };
	Msr_entry fmask          { 0xC0000084 };
	Msr_entry kernel_gs_base { 0xC0000102 };

	static constexpr Core::size_t get_count()
	{
		return sizeof(Msr_store_area) / sizeof(Msr_entry);
	}
};


/*
 * Physical VMCS buffer
 */
struct
alignas(Genode::get_page_size())
Board::Vmcs_buf
{
	union {
		uint32_t rev;
		Genode::uint8_t  pad[Genode::get_page_size()];
	};

	Vmcs_buf(uint32_t rev);
};


/*
 * VMCS
 *
 * See Intel SDM (September 2023) Vol. 3C, section 24.2.
 */
struct
Board::Vmcs
:
	public Board::Virt_interface
{
	static uint32_t system_rev;
	static uint32_t pinbased_allowed_0;
	static uint32_t pinbased_allowed_1;
	static uint32_t vm_entry_allowed_0;
	static uint32_t vm_entry_allowed_1;
	static uint32_t pri_exit_allowed_0;
	static uint32_t pri_exit_allowed_1;
	static uint32_t pri_procbased_allowed_0;
	static uint32_t pri_procbased_allowed_1;
	static uint32_t sec_procbased_allowed_0;
	static uint32_t sec_procbased_allowed_1;
	static uint64_t cr0_fixed0;
	static uint64_t cr0_fixed1;
	static uint64_t cr0_mask;
	static uint64_t cr4_fixed0;
	static uint64_t cr4_fixed1;
	static uint64_t cr4_mask;
	static uint64_t vpid;

	Msr_store_area guest_msr_store_area { };
	/* XXX only needed per vCPU */
	Msr_store_area host_msr_store_area { };
	uint64_t cr2 { 0 };

	Genode::size_t _cpu_id { };

	addr_t msr_phys_addr(Msr_store_area *msr_ptr)
	{
		Genode::size_t offset =
		    (Genode::size_t)msr_ptr - (Genode::size_t)this;
		return vcpu_data.phys_addr + offset;
	}

	/*
	 * VMCS field encodings
	 *
	 * See Intel SDM (September 2023) Vol. 3D, appendix B.
	 */
	enum Field_encoding : uint64_t {
		/*
		 * B.1 16-Bit Fields
		 */

		/* B.1.2 16-Bit Guest-State Fields */
		E_GUEST_ES_SELECTOR               = 0x00000800,
		E_GUEST_CS_SELECTOR               = 0x00000802,
		E_GUEST_SS_SELECTOR               = 0x00000804,
		E_GUEST_DS_SELECTOR               = 0x00000806,
		E_GUEST_FS_SELECTOR               = 0x00000808,
		E_GUEST_GS_SELECTOR               = 0x0000080A,
		E_GUEST_LDTR_SELECTOR             = 0x0000080C,
		E_GUEST_TR_SELECTOR               = 0x0000080E,

		/* B.1.3 16-Bit Host-State Fields */
		E_HOST_CS_SELECTOR                = 0x00000C02,
		E_HOST_FS_SELECTOR                = 0x00000C08,
		E_HOST_GS_SELECTOR                = 0x00000C0A,
		E_HOST_TR_SELECTOR                = 0x00000C0C,


		/*
		 * B.2 64-Bit Fields
		 */

		/* B.2.1 64-Bit Control Fields */
		E_VM_EXIT_MSR_STORE_ADDRESS       = 0x00002006,
		E_VM_EXIT_MSR_LOAD_ADDRESS        = 0x00002008,
		E_VM_ENTRY_MSR_LOAD_ADDRESS       = 0x0000200A,
		E_TSC_OFFSET                      = 0x00002010,
		E_VIRTUAL_APIC_ADDRESS            = 0x00002012,
		E_EPT_POINTER                     = 0x0000201A,

		/* B.2.2 64-Bit Read-Only Data Field */
		E_GUEST_PHYSICAL_ADDRESS          = 0x00002400,

		/* B.2.3 64-Bit Guest-State Fields */
		E_VMCS_LINK_POINTER               = 0x00002800,
		E_GUEST_IA32_EFER                 = 0x00002806,
		E_GUEST_PDPTE0                    = 0x0000280A,
		E_GUEST_PDPTE1                    = 0x0000280C,
		E_GUEST_PDPTE2                    = 0x0000280E,
		E_GUEST_PDPTE3                    = 0x00002810,

		/* B.2.4 64-Bit Host-State Fields */
		E_HOST_IA32_EFER                  = 0x00002C02,


		/*
		 * B.3 32-Bit Fields
		 */

		/* B.3.1 32-Bit Control Fields */
		E_PIN_BASED_VM_EXECUTION_CTRL     = 0x00004000,
		E_PRI_PROC_BASED_VM_EXEC_CTRL     = 0x00004002,
		E_EXCEPTION_BITMAP                = 0x00004004,
		E_PAGE_FAULT_ERROR_CODE_MASK      = 0x00004006,
		E_PAGE_FAULT_ERROR_CODE_MATCH     = 0x00004008,
		E_CR3_TARGET_COUNT                = 0x0000400A,
		E_PRIMARY_VM_EXIT_CONTROLS        = 0x0000400C,
		E_VM_EXIT_MSR_STORE_COUNT         = 0x0000400E,
		E_VM_EXIT_MSR_LOAD_COUNT          = 0x00004010,
		E_VM_ENTRY_CONTROLS               = 0x00004012,
		E_VM_ENTRY_MSR_LOAD_COUNT         = 0x00004014,
		E_VM_ENTRY_INTERRUPT_INFO_FIELD   = 0x00004016,
		E_VM_ENTRY_EXCEPTION_ERROR_CODE   = 0x00004018,
		E_VM_ENTRY_INSTRUCTION_LENGTH     = 0x0000401A,
		E_TPR_THRESHOLD                   = 0x0000401C,
		E_SEC_PROC_BASED_VM_EXEC_CTRL     = 0x0000401E,

		/* B.3.2 32-Bit Read-Only Data Fields */
		E_VM_INSTRUCTION_ERROR            = 0x00004400,
		E_EXIT_REASON                     = 0x00004402,
		E_IDT_VECTORING_INFORMATION_FIELD = 0x00004408,
		E_IDT_VECTORING_ERROR_CODE        = 0x0000440A,
		E_VM_EXIT_INSTRUCTION_LENGTH      = 0x0000440C,

		/* B.3.3 32-Bit Guest-State Fields */
		E_GUEST_ES_LIMIT                  = 0x00004800,
		E_GUEST_CS_LIMIT                  = 0x00004802,
		E_GUEST_SS_LIMIT                  = 0x00004804,
		E_GUEST_DS_LIMIT                  = 0x00004806,
		E_GUEST_FS_LIMIT                  = 0x00004808,
		E_GUEST_GS_LIMIT                  = 0x0000480A,
		E_GUEST_LDTR_LIMIT                = 0x0000480C,
		E_GUEST_TR_LIMIT                  = 0x0000480E,
		E_GUEST_GDTR_LIMIT                = 0x00004810,
		E_GUEST_IDTR_LIMIT                = 0x00004812,
		E_GUEST_ES_ACCESS_RIGHTS          = 0x00004814,
		E_GUEST_CS_ACCESS_RIGHTS          = 0x00004816,
		E_GUEST_SS_ACCESS_RIGHTS          = 0x00004818,
		E_GUEST_DS_ACCESS_RIGHTS          = 0x0000481A,
		E_GUEST_FS_ACCESS_RIGHTS          = 0x0000481C,
		E_GUEST_GS_ACCESS_RIGHTS          = 0x0000481E,
		E_GUEST_LDTR_ACCESS_RIGHTS        = 0x00004820,
		E_GUEST_TR_ACCESS_RIGHTS          = 0x00004822,
		E_GUEST_INTERRUPTIBILITY_STATE    = 0x00004824,
		E_GUEST_ACTIVITY_STATE            = 0x00004826,
		E_IA32_SYSENTER_CS                = 0x0000482A,

		/* B.3.3 32-Bit Host-State Field */
		E_HOST_IA32_SYSENTER_CS           = 0x00004C00,


		/*
		 * B.4 Natural-Width Fields
		 */

		/* B.4.1 Natural-Width Control Fields */
		E_CR0_GUEST_HOST_MASK             = 0x00006000,
		E_CR4_GUEST_HOST_MASK             = 0x00006002,
		E_CR0_READ_SHADOW                 = 0x00006004,
		E_CR4_READ_SHADOW                 = 0x00006006,

		/* B.4.2 Natural-Width Read-Only Data Fields */
		E_EXIT_QUALIFICATION              = 0x00006400,

		/* B.4.3 Natural-Width Guest-State Fields */
		E_GUEST_CR0                       = 0x00006800,
		E_GUEST_CR3                       = 0x00006802,
		E_GUEST_CR4                       = 0x00006804,
		E_GUEST_ES_BASE                   = 0x00006806,
		E_GUEST_CS_BASE                   = 0x00006808,
		E_GUEST_SS_BASE                   = 0x0000680A,
		E_GUEST_DS_BASE                   = 0x0000680C,
		E_GUEST_FS_BASE                   = 0x0000680E,
		E_GUEST_GS_BASE                   = 0x00006810,
		E_GUEST_LDTR_BASE                 = 0x00006812,
		E_GUEST_TR_BASE                   = 0x00006814,
		E_GUEST_GDTR_BASE                 = 0x00006816,
		E_GUEST_IDTR_BASE                 = 0x00006818,
		E_GUEST_DR7                       = 0x0000681A,
		E_GUEST_RSP                       = 0x0000681C,
		E_GUEST_RIP                       = 0x0000681E,
		E_GUEST_RFLAGS                    = 0x00006820,
		E_GUEST_IA32_SYSENTER_ESP         = 0x00006824,
		E_GUEST_IA32_SYSENTER_EIP         = 0x00006826,

		/* B.4.4 Natural-Width Host-State Fields */
		E_HOST_CR0                        = 0x00006C00,
		E_HOST_CR3                        = 0x00006C02,
		E_HOST_CR4                        = 0x00006C04,
		E_HOST_TR_BASE                    = 0x00006C0A,
		E_HOST_GDTR_BASE                  = 0x00006C0C,
		E_HOST_IDTR_BASE                  = 0x00006C0E,
		E_HOST_IA32_SYSENTER_ESP          = 0x00006C10,
		E_HOST_IA32_SYSENTER_EIP          = 0x00006C12,
		E_HOST_RSP                        = 0x00006C14,
		E_HOST_RIP                        = 0x00006C16,
	};

	static void vmxon(addr_t phys_addr)
	{
		bool success = false;
		asm volatile(
			"vmxon %[vmcs];"
			/* the command succeeded if CF = 0 and ZF = 0 */
			: "=@cca"(success)
			: [vmcs] "m"(phys_addr)
			: "cc");
		assert(success && "vmxon failed");
	}

	static void vmptrld(addr_t phys_addr)
	{
		bool success = false;
		asm volatile(
			"vmptrld %[vmcs];"
			/* the command succeeded if CF = 0 and ZF = 0 */
			: "=@cca"(success)
			: [vmcs] "m"(phys_addr)
			: "cc");
		assert(success && "vmptrld failed");
	}

	static uint64_t read(uint32_t enc)
	{
		uint64_t val;
		asm volatile(
			"vmread %[enc], %[val];"
			: [val] "=rm"(val)
			: [enc] "rm"(static_cast<uint64_t>(enc))
			: "cc");
		return val;
	}

	static void vmclear(addr_t phys_addr)
	{
		bool success = false;
		asm volatile(
			"vmclear %[vmcs];"
			/* the command succeeded if CF = 0 and ZF = 0 */
			: "=@cca"(success)
			: [vmcs] "m"(phys_addr)
			: "cc");
		assert(success && "vmclear failed");
	}

	static void write(uint32_t enc, uint64_t val)
	{
		/* Genode::raw("VMWRITE: ", Genode::Hex(enc), " val: ", Genode::Hex(val)); */
		bool success = false;
		asm volatile(
			"vmwrite %[val], %[enc];"
			/* the command succeeded if CF = 0 and ZF = 0 */
			: "=@cca"(success)
			: [enc]"rm"(static_cast<uint64_t>(enc)), [val] "r"(val)
			: "cc");
		assert(success && "vmwrite failed");
	}

	Vmcs(Genode::Vcpu_data &vcpu_data);
	Virt_type virt_type() override
	{
		return Virt_type::VMX;
	}

	static inline uint32_t _ar_convert_to_intel(uint16_t ar) {
		return ((ar << 4) & 0x1F000) | (ar & 0xFF);
	}

	static inline uint16_t _ar_convert_to_genode(uint64_t ar) {
		return ((ar >> 4) & 0x1F00) | (ar & 0xFF);
	}

	void initialize(Kernel::Cpu &cpu, addr_t page_table_phys) override;
	void write_vcpu_state(Genode::Vcpu_state &state) override;
	void read_vcpu_state(Genode::Vcpu_state &state) override;
	void switch_world(Core::Cpu::Context &regs, addr_t) override;
	uint64_t handle_vm_exit() override;

	void save_host_msrs();
	void             prepare_vmcs();
	void             setup_vmx_info();
	static void      enforce_execution_controls(uint32_t desired_primary,
	                                            uint32_t desired_secondary);
	void _load_pointer();
	void construct_host_vmcs();
};


/*
 * Access controls
 */

/*
 * Pin-Based VM-Execution Controls
 *
 * For details, see Vol. 3C of the Intel SDM (September 2023):
 * 25.6.1 Pin-Based VM-Execution Controls
 */

/* 25-5. Definitions of Pin-Based VM-Execution Controls */
struct Pin_based_execution_controls : Genode::Register<32>
{
	struct External_interrupt_exiting   : Bitfield<0,1> { };
	struct Bit_1                        : Bitfield<1,1> { };
	struct Bit_2                        : Bitfield<2,1> { };
	struct Nmi_exiting                  : Bitfield<3,1> { };
	struct Bit_4                        : Bitfield<4,1> { };
	struct Virtual_nmis                 : Bitfield<5,1> { };
};

/*
 * Primary VM-Exit Controls
 *
 * For details, see Vol. 3C of the Intel SDM (September 2023):
 * Table 25-13. Definitions of Primary VM-Exit Controls
 */
struct Primary_vm_exit_controls : Genode::Register<32>
{
	struct Save_debug_controls        : Bitfield< 2,1> { };
	struct Host_address_space_size    : Bitfield< 9,1> { };
	struct Ack_interrupt_on_exit      : Bitfield<15,1> { };
	struct Save_ia32_efer             : Bitfield<20,1> { };
	struct Load_ia32_efer             : Bitfield<21,1> { };
};



/*
 * VM-Entry Controls
 *
 * For details, see Vol. 3C of the Intel SDM (September 2023):
 * Table 25-13. Definitions of Primary VM-Exit Controls
 * 25.8.1 VM-Entry Controls
 */
struct Vm_entry_controls : Genode::Register<32>
{
	struct Load_debug_controls               : Bitfield< 2,1> { };
	struct Ia32e_mode_guest                  : Bitfield< 9,1> { };
	struct Load_ia32_efer                    : Bitfield<15,1> { };
};


/*
 * Processor-Based VM-Execution Controls
 *
 * For details, see Vol. 3C of the Intel SDM (September 2023):
 * 25.6.2 Processor-Based VM-Execution Controls
 */

/* Table 25-6. Definitions of Primary Processor-Based VM-Execution Controls */
struct Primary_proc_based_execution_controls : Genode::Register<32>
{
	struct Interrupt_window_exiting    : Bitfield< 2,1> { };
	struct Hlt_exiting                 : Bitfield< 7,1> { };
	struct Invlpg_exiting              : Bitfield< 9,1> { };
	struct Cr3_load_exiting            : Bitfield<15,1> { };
	struct Cr3_store_exiting           : Bitfield<16,1> { };
	struct Use_tpr_shadow              : Bitfield<21,1> { };
	struct Nmi_window_exiting          : Bitfield<22,1> { };
	struct Unconditional_io_exiting    : Bitfield<24,1> { };
	struct Use_io_bitmaps              : Bitfield<25,1> { };
	struct Use_msr_bitmaps             : Bitfield<28,1> { };
	struct Activate_secondary_controls : Bitfield<31,1> { };
};

/* Table 25-7. Definitions of Secondary Processor-Based VM-Execution Controls */
struct Secondary_proc_based_execution_controls : Genode::Register<32>
{
	struct Enable_ept                             : Bitfield< 1,1> { };
	struct Enable_vpid                            : Bitfield< 5,1> { };
	struct Unrestricted_guest                     : Bitfield< 7,1> { };
	struct Enable_vm_functi                       : Bitfield<13,1> { };
};


/*
 * Virtual Apic State
 *
 * For details, see Vol. 3C of the Intel SDM (September 2023):
 * 30.1 Virtual Apic State
 */
struct Board::Virtual_apic_state
{
	enum {
		VTPR_OFFSET = 0x80,
	};

	Genode::uint8_t pad[4096];

	uint32_t get_vtpr()
	{
		return static_cast<uint32_t>(*(pad + VTPR_OFFSET));
	}

	void set_vtpr(uint32_t vtpr)
	{
		uint32_t *tpr =
			reinterpret_cast<uint32_t *>(pad + VTPR_OFFSET);
		*tpr = vtpr;
	}
};

#endif /* _INCLUDE__SPEC__PC__VMX_H_ */
