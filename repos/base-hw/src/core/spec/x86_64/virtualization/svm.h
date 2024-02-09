/*
 * \brief   VMCB data structure
 * \author  Benjamin Lamowski
 * \date    2022-12-21
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PC__SVM_H_
#define _INCLUDE__SPEC__PC__SVM_H_

#include <base/internal/page_size.h>
#include <base/stdint.h>
#include <cpu.h>
#include <cpu/vcpu_state.h>
#include <cpu/vcpu_state_virtualization.h>
#include <util/mmio.h>
#include <util/string.h>

using Genode::addr_t;
using Genode::size_t;
using Genode::uint8_t;
using Genode::uint32_t;
using Genode::uint64_t;
using Genode::Mmio;
using Genode::Vcpu_data;
using Genode::Vcpu_state;
using Genode::get_page_size;
using Genode::memset;

namespace Kernel
{
class Cpu;
}

namespace Board
{
	struct Msrpm;
	struct Iopm;
	struct Vmcb;
}


struct alignas(get_page_size()) Board::Msrpm
{
	uint8_t pad[8192];

	Msrpm();
};

struct
alignas(get_page_size())
Board::Iopm
{
	uint8_t pad[12288];

	Iopm();
};


/*
 * VMCB data structure
 * See: AMD Manual Vol. 2, Appendix B Layout of VMCB
 */
struct Board::Vmcb
:
	public Mmio<get_page_size()>
{
	enum {
		Asid_host =    0,
		State_off = 1024,
	};


	addr_t root_vmcb_phys = { 0 };

	Vmcb(addr_t vmcb_page_addr, uint32_t id);
	static Vmcb & host_vmcb(size_t cpu_id);
	static addr_t dummy_msrpm();
	void enforce_intercepts(uint32_t desired_primary = 0U, uint32_t desired_secondary = 0U);
	static addr_t dummy_iopm();

	void initialize(Kernel::Cpu &cpu, addr_t page_table_phys_addr);
	void write_vcpu_state(Vcpu_state &state);
	void read_vcpu_state(Vcpu_state &state);
	void switch_world(addr_t vmcb_phys_addr, Core::Cpu::Context &regs);
	uint64_t get_exitcode();

	/*
	 * AMD Manual Vol. 2, Table B-1: VMCB Layout, Control Area
	 */
	struct Intercept_cr       : Register<0x000, 32> {
		struct Reads          : Bitfield< 0,16> { };
		struct Writes         : Bitfield<16,16> { };
	};

	struct Intercept_dr       : Register<0x004, 32> {
		struct Reads          : Bitfield< 0,16> { };
		struct Writes         : Bitfield<16,16> { };
	};

	struct Intercept_ex       : Register<0x008, 32> {
		struct Vectors        : Bitfield<0,32> { };
	};

	struct Intercept_misc1    : Register<0x00C, 32> {
		struct Intr           : Bitfield< 0, 1> { };
		struct Nmi            : Bitfield< 1, 1> { };
		struct Smi            : Bitfield< 2, 1> { };
		struct Init           : Bitfield< 3, 1> { };
		struct Vintr          : Bitfield< 4, 1> { };
		struct Cr0            : Bitfield< 5, 1> { };
		struct Read_Idtr      : Bitfield< 6, 1> { };
		struct Read_Gdtr      : Bitfield< 7, 1> { };
		struct Read_Ldtr      : Bitfield< 8, 1> { };
		struct Read_Tr        : Bitfield< 9, 1> { };
		struct Write_Idtr     : Bitfield<10, 1> { };
		struct Write_Gdtr     : Bitfield<11, 1> { };
		struct Write_Ldtr     : Bitfield<12, 1> { };
		struct Write_Tr       : Bitfield<13, 1> { };
		struct Rdtsc          : Bitfield<14, 1> { };
		struct Rdpmc          : Bitfield<15, 1> { };
		struct Pushf          : Bitfield<16, 1> { };
		struct Popf           : Bitfield<17, 1> { };
		struct Cpuid          : Bitfield<18, 1> { };
		struct Rsm            : Bitfield<19, 1> { };
		struct Iret           : Bitfield<20, 1> { };
		struct Int            : Bitfield<21, 1> { };
		struct Invd           : Bitfield<22, 1> { };
		struct Pause          : Bitfield<23, 1> { };
		struct Hlt            : Bitfield<24, 1> { };
		struct Invlpg         : Bitfield<25, 1> { };
		struct INvlpga        : Bitfield<26, 1> { };
		struct Ioio_prot      : Bitfield<27, 1> { };
		struct Msr_prot       : Bitfield<28, 1> { };
		struct Task_switch    : Bitfield<29, 1> { };
		struct Ferr_freeze    : Bitfield<30, 1> { };
		struct Shutdown       : Bitfield<31, 1> { };
	};

	struct Intercept_misc2    : Register<0x010, 32> {
		struct Vmrun          : Bitfield< 0, 1> { };
		struct Vmcall         : Bitfield< 1, 1> { };
		struct Vmload         : Bitfield< 2, 1> { };
		struct Vmsave         : Bitfield< 3, 1> { };
		struct Stgi           : Bitfield< 4, 1> { };
		struct Clgi           : Bitfield< 5, 1> { };
		struct Skinit         : Bitfield< 6, 1> { };
		struct Rdtscp         : Bitfield< 7, 1> { };
		struct Icebp          : Bitfield< 8, 1> { };
		struct Wbinvd         : Bitfield< 9, 1> { };
		struct Monitor        : Bitfield<10, 1> { };
		struct Mwait_uncon    : Bitfield<11, 1> { };
		struct Mwait_armed    : Bitfield<12, 1> { };
		struct Xsetbv         : Bitfield<13, 1> { };
		struct Rdpru          : Bitfield<14, 1> { };
		struct Efer           : Bitfield<15, 1> { };
		struct Cr             : Bitfield<16,16> { };
	};

	struct Intercept_misc3    : Register<0x014, 32> {
		struct Invlpgb_all    : Bitfield< 0, 1> { };
		struct Invlpgb_inv    : Bitfield< 1, 1> { };
		struct Invpcid        : Bitfield< 2, 1> { };
		struct Mcommit        : Bitfield< 3, 1> { };
		struct Stgi           : Bitfield< 4, 1> { };
	};

	struct Pause_filter_thres : Register<0x03C,16> { };
	struct Pause_filter_count : Register<0x03E,16> { };
	struct Iopm_base_pa       : Register<0x040,64> { };
	struct Msrpm_base_pa      : Register<0x048,64> { };
	struct Tsc_offset         : Register<0x050,64> { };

	/*
	 * The following two registers are documented as one 64bit register in the
	 * manual but since this is rather tedious to work with, just split them
	 * into two.
	 */
	struct Guest_asid         : Register<0x058,32> { };
	struct Tlb                : Register<0x05C,32> {
		struct Tlb_control    : Bitfield< 0, 8> {};
	};

	struct Int_control        : Register<0x060, 64> {
		struct V_tpr          : Bitfield< 0, 8> { };
		struct V_irq          : Bitfield< 8, 1> { };
		struct Vgif           : Bitfield< 9, 1> { };
		struct V_intr_prio    : Bitfield<16, 4> { };
		struct V_ign_tpr      : Bitfield<20, 1> { };
		struct V_intr_mask    : Bitfield<24, 1> { };
		struct Amd_virt_gif   : Bitfield<25, 1> { };
		struct Avic_enable    : Bitfield<31, 1> { };
		struct V_intr_vector  : Bitfield<33, 8> { };
	};

	struct Int_control_ext    : Register<0x068, 64> {
		struct Int_shadow     : Bitfield< 0, 1> { };
		struct Guest_int_mask : Bitfield< 1, 1> { };
	};

	struct Exitcode           : Register<0x070,64> { };
	struct Exitinfo1          : Register<0x078,64> { };
	struct Exitinfo2          : Register<0x080,64> { };
	struct Exitintinfo        : Register<0x088,64> { };

	struct Npt_control        : Register<0x090, 64> {
		struct Np_enable      : Bitfield< 0, 1> { };
		struct Enable_sev     : Bitfield< 1, 1> { };
		struct Sev_enc_state  : Bitfield< 2, 1> { };
		struct Guest_md_ex_tr : Bitfield< 3, 1> { };
		struct Sss_check_en   : Bitfield< 4, 1> { };
		struct Virt_trans_enc : Bitfield< 5, 1> { };
		struct Enable_invlpgb : Bitfield< 7, 1> { };
	};

	struct Avic               : Register<0x098, 64> {
		struct Avic_apic_bar  : Bitfield< 0,52> { };
	};

	struct Ghcb_gpe           : Register<0x0A0,64> { };
	struct Eventinj           : Register<0x0A8,64> { };
	struct N_cr3              : Register<0x0B0,64> { };

	struct Virt_extra         : Register<0x0B8, 64> {
		struct Lbr_virt       : Bitfield< 0, 1> { };
		struct Virt_vmload    : Bitfield< 1, 1> { };
	};

	struct Vmcb_clean         : Register<0x0C0, 64> {
		struct Clean_bits     : Bitfield< 0,32> { };
	};

	struct Nrip               : Register<0x0C8,64> { };

	/*
	 * This is a 128bit field in the documentation.
	 * Split it up into two parts for the time being.
	 */
	struct Fetch_part_1       : Register<0x0D0,64> {
		struct Nr_bytes       : Bitfield< 0,  8> { };
		struct Guest_inst_lo : Bitfield< 8,56> { };
	};
	struct Fetch_part_2       : Register<0x0D8,64> {
		struct Guest_inst_hi  : Bitfield< 0,64> { };
	};

	struct Avic_1             : Register<0x0E0,64> {
		struct Apic_page_ptr  : Bitfield< 0,52> { };
	};

	struct Avic_2             : Register<0x0F0,64> {
		struct Avic_log_table : Bitfield<12,52> { };
	};

	struct Avic_3             : Register<0x0F8,64> {
		struct Avic_max_idx   : Bitfield< 0, 8> { };
		struct Avic_phys_ptr  : Bitfield<12,52> { };
	};

	struct Vmsa               : Register<0x108,64> {
		struct Vmsa_ptr       : Bitfield<12,52> { };
	};


	/*
	 * AMD Manual Vol. 2, Table B-2: VMCB Layout, State Save Area
	 */

	/*
	 * Segments are 128bit in size and therefore cannot be represented with
	 * the current Register Framework.
	 */
	struct Segment : public Mmio<128>
	{
		using Mmio<128>::Mmio;

		struct Sel   : Register<0x0,16> { };
		struct Ar    : Register<0x2,16> { };
		struct Limit : Register<0x4,32> { };
		struct Base  : Register<0x8,64> { };
	};

	Segment   es { range_at(State_off +  0x0) };
	Segment   cs { range_at(State_off + 0x10) };
	Segment   ss { range_at(State_off + 0x20) };
	Segment   ds { range_at(State_off + 0x30) };
	Segment   fs { range_at(State_off + 0x40) };
	Segment   gs { range_at(State_off + 0x50) };
	Segment gdtr { range_at(State_off + 0x60) };
	Segment ldtr { range_at(State_off + 0x70) };
	Segment idtr { range_at(State_off + 0x80) };
	Segment   tr { range_at(State_off + 0x90) };

	struct Efer           : Register<State_off +  0xD0,64> { };
	struct Cr4            : Register<State_off + 0x148,64> { };
	struct Cr3            : Register<State_off + 0x150,64> { };
	struct Cr0            : Register<State_off + 0x158,64> { };
	struct Dr7            : Register<State_off + 0x160,64> { };
	struct Rflags         : Register<State_off + 0x170,64> { };
	struct Rip            : Register<State_off + 0x178,64> { };
	struct Rsp            : Register<State_off + 0x1D8,64> { };
	struct Rax            : Register<State_off + 0x1F8,64> { };
	struct Star           : Register<State_off + 0x200,64> { };
	struct Lstar          : Register<State_off + 0x208,64> { };
	struct Cstar          : Register<State_off + 0x210,64> { };
	struct Sfmask         : Register<State_off + 0x218,64> { };
	struct Kernel_gs_base : Register<State_off + 0x220,64> { };
	struct Sysenter_cs    : Register<State_off + 0x228,64> { };
	struct Sysenter_esp   : Register<State_off + 0x230,64> { };
	struct Sysenter_eip   : Register<State_off + 0x238,64> { };
	struct Cr2            : Register<State_off + 0x240,64> { };
	struct G_pat          : Register<State_off + 0x268,64> { };
};

#endif /* _INCLUDE__SPEC__PC__SVM_H_ */
