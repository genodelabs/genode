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
#include <cpu/vcpu_state.h>
#include <cpu/vcpu_state_virtualization.h>
#include <util/mmio.h>
#include <util/string.h>

namespace Board
{
	struct Msrpm;
	struct Iopm;
	struct Vmcb_control_area;
	struct Vmcb_reserved_for_host;
	struct Vmcb_state_save_area;
	struct Vmcb;
}


struct alignas(Genode::get_page_size()) Board::Msrpm
{
	Genode::uint8_t pad[8192];

	Msrpm();
};

struct
alignas(Genode::get_page_size())
Board::Iopm
{
	Genode::uint8_t pad[12288];

	Iopm();
};



/*
 * VMCB Control area, excluding the reserved for host part
 */
struct Board::Vmcb_control_area
{
	enum : Genode::size_t {
		total_size      = 1024U,
		used_guest_size = 0x3E0U
	};

	/* The control area is padded and used via Mmio-like accesses. */
	Genode::uint8_t control_area[used_guest_size];

	Vmcb_control_area()
	{
		Genode::memset((void *) this, 0, sizeof(Vmcb_control_area));
	}
};


/*
 * Part of the VMCB control area that is reserved for host data.
 * This uses 16 bytes less to accomodate for the size of the Mmio class.
 */
struct Board::Vmcb_reserved_for_host
{
	/* 64bit used by the inherited Mmio class here */
	Genode::addr_t root_vmcb_phys = 0U;
};
static_assert(Board::Vmcb_control_area::total_size -
              sizeof(Board::Vmcb_control_area) - sizeof(Genode::Mmio<0>) -
              sizeof(Board::Vmcb_reserved_for_host) ==
              0);

/*
 * AMD Manual Vol. 2, Table B-2: VMCB Layout, State Save Area
 */
struct Board::Vmcb_state_save_area
{
	typedef Genode::Vcpu_state::Segment Segment;

	Segment          es, cs, ss, ds, fs, gs, gdtr, ldtr, idtr, tr;
	Genode::uint8_t  reserved1[43];
	Genode::uint8_t  cpl;
	Genode::uint8_t  reserved2[4];
	Genode::uint64_t efer;
	Genode::uint8_t  reserved3[112];
	Genode::uint64_t cr4, cr3, cr0, dr7, dr6, rflags, rip;
	Genode::uint8_t  reserved4[88];
	Genode::uint64_t rsp;
	Genode::uint64_t s_cet, ssp, isst_addr;
	Genode::uint64_t rax, star, lstar, cstar, sfmask, kernel_gs_base;
	Genode::uint64_t sysenter_cs, sysenter_esp, sysenter_eip, cr2;
	Genode::uint8_t  reserved5[32];
	Genode::uint64_t g_pat;
	Genode::uint64_t dbgctl;
	Genode::uint64_t br_from;
	Genode::uint64_t br_to;
	Genode::uint64_t lastexcpfrom;
	Genode::uint8_t  reserved6[72];
	Genode::uint64_t spec_ctrl;
} __attribute__((packed));


/*
 * VMCB data structure
 * See: AMD Manual Vol. 2, Appendix B Layout of VMCB
 *
 * We construct the VMCB by inheriting from its components. Inheritance is used
 * instead of making the components members of the overall structure in order to
 * present the interface on the top level. Order of inheritance is important!
 *
 * The Mmio interface is inherited from on this level to achieve the desired
 * placement of the Mmio data member address in the host data part and after the
 * VMCB control area data.
 * The remaining part of the VMCB control area that is reserved for host data
 * is then inherited from after the Mmio interface.
 * Lastly, the VMCB state save area is inherited from to make its members
 * directly available in the VCMB structure.
 * In total, this allows Register type access to the VMCB control area and easy
 * direct access to the VMCB state save area.
 */
struct alignas(Genode::get_page_size()) Board::Vmcb
:
	Board::Vmcb_control_area,
	public Genode::Mmio<Genode::get_page_size()>,
	Board::Vmcb_reserved_for_host,
	Board::Vmcb_state_save_area
{
	enum {
		Asid_host = 0,
	};

	Vmcb(Genode::uint32_t id);
	void init(Genode::size_t cpu_id, void * table_ptr);
	static Vmcb & host_vmcb(Genode::size_t cpu_id);
	static Genode::addr_t dummy_msrpm();
	void enforce_intercepts(Genode::uint32_t desired_primary = 0U, Genode::uint32_t desired_secondary = 0U);
	static Genode::addr_t dummy_iopm();

	Genode::uint8_t reserved[Genode::get_page_size()             -
	                         sizeof(Board::Vmcb_state_save_area) -
	                         Board::Vmcb_control_area::total_size];

	Genode::Vcpu_data * vcpu_data()
	{
		return reinterpret_cast<Genode::Vcpu_data *>(this);
	}

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
} __attribute__((packed));

#endif /* _INCLUDE__SPEC__PC__SVM_H_ */
