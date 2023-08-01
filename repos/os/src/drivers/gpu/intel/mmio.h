/*
 * \brief  GEN8/9/12 MMIO definitions
 * \author Josef Soentgen
 * \data   2017-03-15
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MMIO_H_
#define _MMIO_H_

/* Genode includes */
#include <base/log.h>
#include <platform_session/device.h>
#include <timer_session/connection.h>

/* local includes */
#include <types.h>


namespace Igd { class Mmio; }


class Igd::Mmio : public Platform::Device::Mmio
{
	public:

		unsigned _generation { 0 };

		void generation(unsigned gen) { _generation = gen; }

		unsigned generation() const
		{
			if (!_generation) {
				Genode::error("Unsupported generation: ", _generation);
			}

			return _generation;
		}

		enum {
			/*
			 * XXX IDs are taken from Linux, still looking
			 *     for a reference in the documentation
			 */
			RCS_ID  = 0,
			VCS_ID  = 1,
			BCS_ID  = 2,
			VECS_ID = 3,
			VCS2_ID = 4,

			NUM_ENGINES = 5,
			NUM_FENCES  = 32,

			SIZE = 2u<<20,
		};

		static char const *engine_name(int id)
		{
			switch (id) {
			case RCS_ID:  return "RCS";
			case VCS_ID:  return "VCS";
			case BCS_ID:  return "BCS";
			case VECS_ID: return "VECS";
			case VCS2_ID: return "VCS2";
			}
			return "<unknown>";
		}

		/*************************
		 ** Interrupt registers **
		 *************************/

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 953 ff.
		 */
		struct MASTER_INT_CTL : Register<0x44200, 32>
		{
			struct Master_interrupt_enable        : Bitfield<31, 1> { };
			struct Pcu_interrupts_pending         : Bitfield<30, 1> { };
			struct Audio_codec_interrupts_pending : Bitfield<24, 1> { };
			struct De_pch_interrupts_pending      : Bitfield<23, 1> { };
			struct De_misc_interrupts_pending     : Bitfield<22, 1> { };
			struct De_pch_misc                    : Bitfield<22, 2> { };
			struct De_port_interrupts_pending     : Bitfield<20, 1> { };
			struct De_pipe_c_interrupts_pending   : Bitfield<18, 1> { };
			struct De_pipe_b_interrupts_pending   : Bitfield<17, 1> { };
			struct De_pipe_a_interrupts_pending   : Bitfield<16, 1> { };
			struct De_pipe                        : Bitfield<16, 3> { };
			struct Vebox_interrupts_pending       : Bitfield< 6, 1> { };
			struct Gtpm_interrupts_pending        : Bitfield< 4, 1> { };
			struct Vcs2_interrupts_pending        : Bitfield< 3, 1> { };
			struct Vcs1_interrupts_pending        : Bitfield< 2, 1> { };
			struct Blitter_interrupts_pending     : Bitfield< 1, 1> { };
			struct Render_interrupts_pending      : Bitfield< 0, 1> { };
			struct De_interrupts_pending :
				Genode::Bitset_3<De_pipe, De_port_interrupts_pending, De_pch_misc> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 658 ff.
		 */
		template <long int REG>
		struct GT_0_INTERRUPT : Register<REG, 32>
		{
			using B = Register<REG, 32>;

			struct Bcs_wait_on_semaphore        : B::template Bitfield<27, 1> { };
			struct Bcs_ctx_switch_interrupt     : B::template Bitfield<24, 1> { };
			struct Bcs_mi_flush_dw_notify       : B::template Bitfield<20, 1> { };
			struct Bcs_error_interrupt          : B::template Bitfield<19, 1> { };
			struct Bcs_mi_user_interrupt        : B::template Bitfield<16, 1> { };

			struct Cs_wait_on_semaphore         : B::template Bitfield<11, 1> { };
			struct Cs_l3_counter_slave          : B::template Bitfield<10, 1> { };
			struct Cs_ctx_switch_interrupt      : B::template Bitfield< 8, 1> { };
			struct Page_fault_error             : B::template Bitfield< 7, 1> { };
			struct Cs_watchdog_counter_expired  : B::template Bitfield< 6, 1> { };
			struct L3_parity_error              : B::template Bitfield< 5, 1> { };
			struct Cs_pipe_control_notify       : B::template Bitfield< 4, 1> { };
			struct Cs_error_interrupt           : B::template Bitfield< 3, 1> { };
			struct Cs_mi_user_interrupt         : B::template Bitfield< 0, 1> { };
		};

		struct GT_0_INTERRUPT_ISR : GT_0_INTERRUPT<0x44300> { };
		struct GT_0_INTERRUPT_IMR : GT_0_INTERRUPT<0x44304> { };
		struct GT_0_INTERRUPT_IIR : GT_0_INTERRUPT<0x44308> { };
		struct GT_0_INTERRUPT_IER : GT_0_INTERRUPT<0x4430C> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 673 ff.
		 */
		template <long int REG>
		struct GT_1_INTERRUPT : Register<REG, 32>
		{
			using B = Register<REG, 32>;

			struct Vcs2_wait_on_semaphore        : B::template Bitfield<27, 1> { };
			struct Vcs2_ctx_switch_interrupt     : B::template Bitfield<24, 1> { };
			struct Vcs2_watchdog_counter_expired : B::template Bitfield<22, 1> { };
			struct Vcs2_mi_flush_dw_notify       : B::template Bitfield<20, 1> { };
			struct Vcs2_error_interrupt          : B::template Bitfield<19, 1> { };
			struct Vcs2_mi_user_interrupt        : B::template Bitfield<16, 1> { };

			struct Vcs1_wait_on_semaphore        : B::template Bitfield<11, 1> { };
			struct Vcs1_ctx_switch_interrupt     : B::template Bitfield< 8, 1> { };
			struct Vcs1_watchdog_counter_expired : B::template Bitfield< 6, 1> { };
			struct Vcs1_pipe_control_notify      : B::template Bitfield< 4, 1> { };
			struct Vcs1_error_interrupt          : B::template Bitfield< 3, 1> { };
			struct Vcs1_mi_user_interrupt        : B::template Bitfield< 0, 1> { };
		};

		struct GT_1_INTERRUPT_ISR : GT_1_INTERRUPT<0x44310> { };
		struct GT_1_INTERRUPT_IMR : GT_1_INTERRUPT<0x44314> { };
		struct GT_1_INTERRUPT_IIR : GT_1_INTERRUPT<0x44318> { };
		struct GT_1_INTERRUPT_IER : GT_1_INTERRUPT<0x4431C> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 687 ff.
		 */
		template <long int REG>
		struct GT_2_INTERRUPT : Register<REG, 32>
		{
			using B = Register<REG, 32>;

			struct Unslice_frequency_control_up_interrupt                       : B::template Bitfield<13, 1> { };
			struct Unslice_frequency_control_down_interrupt                     : B::template Bitfield<12, 1> { };
			struct Nfafdl_frequency_up_interrupt                                : B::template Bitfield<11, 1> { };
			struct Nfafdl_frequency_down_interrupt                              : B::template Bitfield<10, 1> { };
			struct Gtpm_engines_idle_interrupt                                  : B::template Bitfield< 8, 1> { };
			struct Gtpm_uncore_to_core_trap_interrupt                           : B::template Bitfield< 7, 1> { };
			struct Gtpm_render_frequency_downwards_timeout_during_rc6_interrupt : B::template Bitfield< 6, 1> { };
			struct Gtpm_render_p_state_up_threshold_interrupt                   : B::template Bitfield< 5, 1> { };
			struct Gtpm_render_p_state_down_threshold_interrupt                 : B::template Bitfield< 4, 1> { };
			struct Gtpm_render_geyserville_up_evaluation_interval_interrupt     : B::template Bitfield< 2, 1> { };
			struct Gtpm_render_geyserville_down_evaluation_interval_interrupt   : B::template Bitfield< 1, 1> { };
		};

		struct GT_2_INTERRUPT_ISR : GT_2_INTERRUPT<0x44320> { };
		struct GT_2_INTERRUPT_IMR : GT_2_INTERRUPT<0x44324> { };
		struct GT_2_INTERRUPT_IIR : GT_2_INTERRUPT<0x44328> { };
		struct GT_2_INTERRUPT_IER : GT_2_INTERRUPT<0x4432C> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 689 ff.
		 */
		template <long int REG>
		struct GT_3_INTERRUPT : Register<REG, 32>
		{
			using B = Register<REG, 32>;

			struct Performance_monitoring_buffer_half_full_interrupt : B::template Bitfield<28, 1> { };
			struct Vecs_wait_on_semaphore                            : B::template Bitfield<11, 1> { };
			struct Vecs_ctx_switch_interrupt                         : B::template Bitfield< 8, 1> { };
			struct Vecs_mi_flush_dw_notify                           : B::template Bitfield< 4, 1> { };
			struct Vecs_error_interrupt                              : B::template Bitfield< 3, 1> { };
			struct Vecs_mi_user_interrupt                            : B::template Bitfield< 0, 1> { };
		};

		struct GT_3_INTERRUPT_ISR : GT_3_INTERRUPT<0x44330> { };
		struct GT_3_INTERRUPT_IMR : GT_3_INTERRUPT<0x44334> { };
		struct GT_3_INTERRUPT_IIR : GT_3_INTERRUPT<0x44338> { };
		struct GT_3_INTERRUPT_IER : GT_3_INTERRUPT<0x4433C> { };

		/**
		 * GEN11+ interrupt registers
		 * IHD-OS-TGL-Vol 2c-12.21 part1 p. 1039 ff.
		 */
		struct GEN12_GFX_MSTR_INTR : Register<0x190010, 32>
		{
			struct Master_interrupt_enable : Bitfield<31, 1> { };
			struct Display                 : Bitfield<16, 1> { };
			struct Gt_dw_1                 : Bitfield< 1, 1> { };
			struct Gt_dw_0                 : Bitfield< 0, 1> { };
		};

		/* p. 1089 */
		struct GEN12_GT_INTR_DW0 : Register<0x190018, 32>
		{
			struct Rcs0 : Bitfield<0, 1> { };
		};

		/* p. 1091 */
		struct GEN12_INTR_IDENTITY_REG0 : Register<0x190060, 32>
		{
			struct Valid            : Bitfield<31, 1> { };
			struct Engine_interrupt : Bitfield<0, 16> { };
		};

		/*
		 * p. 1092
		 *
		 * Select engine ID to read INTR_IDENTITY from, use only one bit at a time,
		 * layout is as INTR_DW0 register
		 */
		struct GEN12_INTR_IIR_SELECTOR0 : Register<0x190070, 32, true>
		{
			struct Rcs0 : Bitfield<0, 1> { };
		};

		/* p. 1077 */
		struct GEN12_RENDER_COPY_INTR_ENABLE : Register<0x190030, 32>
		{
			struct Render_enable : Bitfield<16, 16> { };
			struct Copy_enable   : Bitfield< 0, 16> { };
		};

		/*
		 * IHD-OS-TGL-Vol 2d-12.21 p. 818 ff
		 */
		struct GEN12_RENDER_INTR_VEC : Genode::Register<16>
		{
			struct Catastrophic_error           : Bitfield<15, 1> { };
			struct Eu_restart                   : Bitfield<14, 1> { };
			struct Context_stall                : Bitfield<13, 1> { };
			struct Cs_wait_on_semaphore         : Bitfield<11, 1> { };
			struct Cs_ctx_switch_interrupt      : Bitfield< 8, 1> { };
			struct Legacy_page_fault_error      : Bitfield< 7, 1> { };
			struct Cs_watchdog_counter_expired  : Bitfield< 6, 1> { };
			struct Cs_pipe_control_notify       : Bitfield< 4, 1> { };
			struct Cs_error_interrupt           : Bitfield< 3, 1> { };
			struct Cs_mi_user_interrupt         : Bitfield< 0, 1> { };
		};

		/**************************
		 ** Page-table registers **
		 **************************/

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1169
		 */
		struct PAT_INDEX : Genode::Register<32>
		{
			struct Class_of_service : Bitfield<8, 2> { };
			struct Lru_age          : Bitfield<4, 2>
			{
				enum {
					UCR = 0b00,
					A0  = 0b01,
					DNC = 0b10,
					A3  = 0b11,
				};
			};
			struct Target_cache     : Bitfield<2, 2>
			{
				enum {
					ELLC    = 0b00,
					LLC     = 0b01,
					LLCELLC = 0b10,
					LLCeLLC = 0b11,
				};
			};
			struct Mem_type         : Bitfield<0, 2>
			{
				enum {
					UC = 0b00,
					WC = 0b01,
					WT = 0b10,
					WB = 0b11,
				};
			};
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1170
		 */
		struct PAT_INDEX_H : Register<0x040E0, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1071
		 */
		struct PAT_INDEX_L : Register<0x040E4, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1516 ff.
		 */
		struct TIMESTAMP_CTR : Register<0x44070, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 75
		 *
		 * TODO registers are not continuous
		 */
		struct FAULT_REG : Register_array<0x04000, 32, NUM_ENGINES, 32>
		{
			struct Engine_ID  : Bitfield<12,3>
			{
				enum EID {
					GFX  = 0,
					MFX0 = 1,
					MFX1 = 2,
					VEBX = 3,
					BLT  = 4
				};
			};
			struct SRCID      : Bitfield< 3,8> { };
			struct Fault_Type : Bitfield< 1,2>
			{
				enum GFX_FT {
					INVALID_PTE   = 0,
					INVALID_PDE   = 1,
					INVALID_PDPE  = 2,
					INVALID_PML4E = 3
				};
			};
			struct Valid_Bit  : Bitfield<0,1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 446
		 */
		struct FAULT_TLB_RB_DATA0 : Register<0x04B10, 32>
		{
			struct Fault_cycle_va : Bitfield<0, 32> { }; /* 43:12 */
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 447
		 */
		struct FAULT_TLB_RB_DATA1 : Register<0x04B14, 32>
		{
			struct Cycle_gtt_sel  : Bitfield<4, 1> { };
			struct Fault_cycle_va : Bitfield<0, 4> { }; /* 47:44 */
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1353 ff.
		 */
		struct RING_BUFFER_TAIL_RCSUNIT : Register<0x02030, 32>
		{
			struct Tail_Offset : Bitfield< 3, 18> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1350 ff.
		 */
		struct RING_BUFFER_HEAD_RCSUNIT : Register<0x02034, 32>
		{
			struct Wrap_Count  : Bitfield<21, 11> { };
			struct Head_Offset : Bitfield< 2, 19> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1352
		 */
		struct RING_BUFFER_START_RCSUNIT : Register<0x02038, 32>
		{
			struct Starting_Address : Bitfield<12, 20> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1345 ff.
		 */
		struct RING_BUFFER_CTL_RCSUNIT : Register<0x0203C, 32>
		{
			struct Buffer_length                 : Bitfield<12, 9> { };
			struct RBWait                        : Bitfield<11, 1> { };
			struct Semaphore_wait                : Bitfield<10, 1> { };
			struct Automatic_report_head_pointer : Bitfield< 1, 2>
			{
				enum {
					MI_AUTOREPORT_OFF    = 0,
					MI_AUTOREPORT_64KB   = 1,
					MI_AUTOREPORT_4KB    = 2,
					MI_AUTO_REPORT_128KB = 3
				};
			};
			struct Ring_buffer_enable : Bitfield< 0, 1> { };
		};


		/*********************
		 ** Debug registers **
		 *********************/

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 225
		 */
		struct HW_MEMRD : Register<0x2060, 32> { };

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 228
		 */
		struct IPEIR : Register<0x2064, 32>
		{
			struct Batch_buffer_error : Bitfield<3, 1> { };
		};

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 229
		 */
		struct IPEHR : Register<0x2068, 32> { };

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 229
		 */
		template <long int BASE>
		struct INSTDONE_BASE : Register<BASE + 0x6C, 32>
		{
			enum { DEFAULT_VALUE = 0xffe7fffe, };
		};

		struct RCS_INSTDONE : INSTDONE_BASE<0x2000>
		{
			struct Row_0_eu_0_done : Bitfield<31, 1> { };
			struct Row_0_eu_1_done : Bitfield<30, 1> { };
			struct Row_0_eu_2_done : Bitfield<29, 1> { };
			struct Row_0_eu_3_done : Bitfield<28, 1> { };
			struct Row_1_eu_0_done : Bitfield<27, 1> { };
			struct Row_1_eu_1_done : Bitfield<26, 1> { };
			struct Row_1_eu_2_done : Bitfield<25, 1> { };
			struct Row_1_eu_3_done : Bitfield<24, 1> { };
			struct Sf_done         : Bitfield<23, 1> { };
			struct Se_done         : Bitfield<22, 1> { };
			struct Windower_done   : Bitfield<21, 1> { };
			struct Reserved1       : Bitfield<20, 1> { };
			struct Reserved2       : Bitfield<19, 1> { };
			struct Dip_done        : Bitfield<18, 1> { };
			struct Pl_done         : Bitfield<17, 1> { };
			struct Dg_done         : Bitfield<16, 1> { };
			struct Qc_done         : Bitfield<15, 1> { };
			struct Ft_done         : Bitfield<14, 1> { };
			struct Dm_done         : Bitfield<13, 1> { };
			struct Sc_done         : Bitfield<12, 1> { };
			struct Fl_done         : Bitfield<11, 1> { };
			struct By_done         : Bitfield<10, 1> { };
			struct Ps_done         : Bitfield< 9, 1> { };
			struct Cc_done         : Bitfield< 8, 1> { };
			struct Map_fl_done     : Bitfield< 7, 1> { };
			struct Map_l2_idle     : Bitfield< 6, 1> { };
			struct Msg_arb_0_done  : Bitfield< 5, 1> { };
			struct Msg_arb_1_done  : Bitfield< 4, 1> { };
			struct Ic_row_0_done   : Bitfield< 3, 1> { };
			struct Ic_row_1_done   : Bitfield< 2, 1> { };
			struct Cp_done         : Bitfield< 1, 1> { };
			struct Ring_0_enable   : Bitfield< 0, 1> { };
		};

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 229
		 */
		template <long int BASE>
		struct INSTDONE_1_BASE : Register<BASE + 0x7C, 32>
		{
			enum { DEFAULT_VALUE = 0x0, };
		};

		struct RCS_INSTDONE_1 : INSTDONE_1_BASE<0x2000> { };

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 231
		 */
		template <long int BASE>
		struct ACTHD_BASE : Register<BASE + 0x74, 32>
		{
			using B = Register<BASE + 0x74, 32>;

			struct Head_pointer : B::template Bitfield<2, 30> { };
			struct Reserved     : B::template Bitfield<0,  2> { }; /* MBZ */
		};

		struct RCS_ACTHD : ACTHD_BASE<0x2000> { };

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 232
		 */
		struct DMA_FADD_PREF : Register<0x2078, 32> { };

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 235
		 */
		struct NOP_ID : Register<0x2094, 32> { };

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 205
		 */
		struct PGTBL_ER : Register<0x2024, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 618 ff.
		 */
		struct GFX_MODE : Register<0x0229C, 32>
		{
			/* to change bits below, accordings mask bit must be set */
			struct Mask_bits : Bitfield<16, 16> { };

			struct Execlist_enable               : Bitfield<15, 1> { };
			struct Gen12_prefetch_disable        : Bitfield<10, 1> { };
			struct Gen11_gfx_disable_legacy_mode : Bitfield< 3, 1> { };
			struct Privilege_check_disable       : Bitfield< 0, 1> { };

			/*
			 * these bits are only valid in ringer buffer mode, we use execlist with
			 * contexts
			 */
			struct Ppgtt_enable                  : Bitfield< 9, 1> { };
			struct Virtual_addressing_enable     : Bitfield< 7, 1> { };

			template <typename T>
			static access_t set(access_t v, bool bit)
			{
				T::set(v, bit);
				Mask_bits::set(v, 1u << T::SHIFT);
				return v;
			}
		};

		/*
		 * IHD-OS-BDW-Vol 6-11.15 p. 19 ff.
		 * IHD-OS-BDW-Vol 7-11.15 p. 42
		 */
		struct RCS_RING_CONTEXT_STATUS_PTR : Register<0x023A0, 32>
		{
			struct Read_pointer_mask  : Bitfield<24, 8> { };
			struct Write_pointer_mask : Bitfield<16, 8> { };
			struct Read_pointer       : Bitfield< 8, 8> { };
			struct Write_pointer      : Bitfield< 0, 8> { };
		};

		/*********************
		 ** Error registers **
		 *********************/

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 413
		 */
		template <long int BASE>
		struct EIR_BASE : Register<BASE + 0xB0, 32>
		{
			using B = Register<BASE + 0xB0, 32>;

			struct Reserved            : B::template Bitfield<16, 16> { }; /* MBZ */
			struct Error_identity_bits : B::template Bitfield< 0, 16> { };

			struct Error_instruction : B::template Bitfield<0, 1>  { };
			struct Error_mem_refresh : B::template Bitfield<1, 1>  { };
			struct Error_page_table  : B::template Bitfield<4, 1>  { };
		};

		struct RCS_EIR : EIR_BASE<0x2000> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 414
		 */
		template <long int BASE>
		struct EMR_BASE : Register<BASE + 0xB4, 32>
		{
			using B = Register<BASE + 0xB4, 32>;

			struct Reserved         : B::template Bitfield<8, 24> { }; /* MBO */
			struct Error_mask_bits  : B::template Bitfield<0, 8>  { };

			struct Error_page_table  : B::template Bitfield<4, 1>  { };
			struct Error_mem_refresh : B::template Bitfield<1, 1>  { };
		};

		struct RCS_EMR : EMR_BASE<0x2000> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 415 ff.
		 */
		/* struct ERR */

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 417
		 */
		template <long int BASE>
		struct ESR_BASE : Register<BASE + 0xB8, 32>
		{
			using B = Register<BASE + 0xB8, 32>;

			struct Reserved          : B::template Bitfield<16, 16> { }; /* MBZ */
			struct Error_status_bits : B::template Bitfield< 0, 16> { };
		};

		struct RCS_ESR : ESR_BASE<0x2000> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 944 ff.
		 */
		struct ERROR : Register<0x040A0, 32>
		{
			struct Ctx_fault_ctxt_not_prsnt_err  : Bitfield<15, 1> { }; /* typo in PRM (prsmt) */
			struct Ctx_fault_root_not_prsnt_err  : Bitfield<14, 1> { }; /* "                   */
			struct Ctx_fault_pasid_not_prsnt_err : Bitfield<13, 1> { };
			struct Ctx_fault_pasid_ovflw_err     : Bitfield<12, 1> { };
			struct Ctx_fault_pasid_dis_err       : Bitfield<11, 1> { };

			struct Rstrm_fault_nowb_atomic_err        : Bitfield<10, 1> { };
			struct Unloaded_pd_error                  : Bitfield< 8, 1> { };
			struct Hws_page_fault_error               : Bitfield< 3, 1> { };
			struct Invalid_page_directory_entry_error : Bitfield< 2, 1> { };
			struct Ctx_page_fault_error               : Bitfield< 1, 1> { };
			struct Tlb_fault_error                    : Bitfield< 0, 1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 948
		 */
		struct ERROR_2 : Register<0x040A4, 32>
		{
			struct Tlbpend_reg_faultcnt : Bitfield< 0, 6> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1237
		 */
		struct PP_PFD : Register<0x04580, 32>
		{
			struct Fault_entry_page_address : Bitfield<12, 20> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 535
		 */
		struct GAMTARBMODE : Register<0x4a08, 32>
		{
			struct Arbiter_mode_control_1 : Bitfield<1, 1> { };
		};


		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1315 ff.
		 */
		struct RC_CTRL0 : Register<0x0A090, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1317 ff.
		 */
		struct RC_CTRL1 : Register<0x0A094, 32>
		{
			struct Rc_state : Bitfield<18, 1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1095
		 */
		struct NDE_RSTWRN_OPT : Register<0x46408, 32>
		{
			struct Rst_pch_handshake_en : Bitfield<4, 1> { };
		};

		/*
		 * XXX documentation reference missing
		 */
		struct PWR_WELL_CTL2 : Register<0x45404, 32>
		{
			struct Misc_io_power_state              : Bitfield< 0, 1> { };
			struct Misc_io_power_request            : Bitfield< 1, 1> { };
			struct Ddi_a_and_ddi_e_io_power_state   : Bitfield< 2, 1> { };
			struct Ddi_a_and_ddi_e_io_power_request : Bitfield< 3, 1> { };
			struct Ddi_b_io_power_state             : Bitfield< 4, 1> { };
			struct Ddi_b_io_power_request           : Bitfield< 5, 1> { };
			struct Ddi_c_io_power_state             : Bitfield< 6, 1> { };
			struct Ddi_c_io_power_request           : Bitfield< 7, 1> { };
			struct Ddi_d_io_power_state             : Bitfield< 8, 1> { };
			struct Ddi_d_io_power_request           : Bitfield< 9, 1> { };
			struct Power_well_1_state               : Bitfield<28, 1> { };
			struct Power_well_1_request             : Bitfield<29, 1> { };
			struct Power_well_2_state               : Bitfield<30, 1> { };
			struct Power_well_2_request             : Bitfield<31, 1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 853
		 */
		struct L3_LRA_1_GPGPU : Register<0x04DD4, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 711
		 *
		 * "Hardware Status Mask Register", this register controls which IRQs are
		 * even written to the PCI bus (should be same as unmasked in IMR)
		 */
		struct HWSTAM : Register<0x02098, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 712
		 */
		struct HWS_PGA_RCSUNIT  : Register<0x02080, 32> { };
		struct HWS_PGA_VCSUNIT0 : Register<0x12080, 32> { };
		struct HWS_PGA_VECSUNIT : Register<0x1A080, 32> { };
		struct HWS_PGA_VCSUNIT1 : Register<0x1C080, 32> { };
		struct HWS_PGA_BCSUNIT  : Register<0x22080, 32> { };

		struct PWRCTXA  : Register<0x02088, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1370
		 */
		struct RP_FREQ_NORMAL : Register<0x0A008, 32>
		{
			struct Turbo_disable : Bitfield<31, 1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1318
		 */
		struct RP_CTRL : Register<0xA024, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 493
		 */
		/*
			reg_post is in the same cacheline as FORCEWAKE and is
			read after FORCEWAKE was written to prevent reordering
			when reading from FORCEWAKE_ACK_HSW [1] (older driver
			version used a different register for posting reads but
			that did not work on certain older (~2012) machines.

			[1] https://lists.freedesktop.org/archives/intel-gfx/2015-January/058101.html
		*/
		enum { FORCEWAKE_ID_RENDER = 0, };
		struct FORCEWAKE_MT : Register<0x0A188, 32>
		{
			enum {
				RESET = 0xffff0000u,
			};

			struct Multiple_force_wake_mask : Bitfield<16, 16> { };

			struct Force_wake_request_for_thread_15 : Bitfield<15, 1> { };
			struct Force_wake_request_for_thread_14 : Bitfield<14, 1> { };
			struct Force_wake_request_for_thread_13 : Bitfield<13, 1> { };
			struct Force_wake_request_for_thread_12 : Bitfield<12, 1> { };
			struct Force_wake_request_for_thread_11 : Bitfield<11, 1> { };
			struct Force_wake_request_for_thread_10 : Bitfield<10, 1> { };
			struct Force_wake_request_for_thread_9  : Bitfield< 9, 1> { };
			struct Force_wake_request_for_thread_8  : Bitfield< 8, 1> { };
			struct Force_wake_request_for_thread_7  : Bitfield< 7, 1> { };
			struct Force_wake_request_for_thread_6  : Bitfield< 6, 1> { };
			struct Force_wake_request_for_thread_5  : Bitfield< 5, 1> { };
			struct Force_wake_request_for_thread_4  : Bitfield< 4, 1> { };
			struct Force_wake_request_for_thread_3  : Bitfield< 3, 1> { };
			struct Force_wake_request_for_thread_2  : Bitfield< 2, 1> { };
			struct Force_wake_request_for_thread_1  : Bitfield< 1, 1> { };
			struct Force_wake_request_for_thread_0  : Bitfield< 0, 1> { };
		};

		struct DRIVER_RENDER_FWAKE_ACK : Register<0x0D84, 32> {
			struct Rcs_force_wake_enable_mask : Bitfield<16, 1> { };
			struct Rcs_force_wake_enable      : Bitfield< 0, 1> { };
		};

		struct ELEM_DESCRIPTOR1 : Register<0x4400, 32> { };
		struct ELEM_DESCRIPTOR2 : Register<0x4404, 32> { };

		/**
		 * GEN9+ GPM unit lx 5.15
		 */
		struct CTC_MODE : Register<0xa26c, 32>
		{
			struct Source_divide_logic : Bitfield<0, 1> { };
		};

		/**
		 * GEN8+ RPM unit
		 */
		struct RPM_CONFIG0 : Register<0x0d00, 32>
		{
			struct Ctc_shift               : Bitfield<1, 2> { };
			struct Crystal_clock_frequency : Bitfield<3, 3> { };

			static uint32_t clock_frequency(Crystal_clock_frequency::access_t freq)
			{
				switch (freq) {
				case 0: return 24'000'000;
				case 1: return 19'200'000;
				case 2: return 38'400'000;
				case 3: return 25'000'000;
				default:
					Genode::error("Unknown crystal clock frequency: ", freq);
					return 0;
				}
			}
		};

		/**
		 * Forcewake for GEN9 & GEN10, lx 5.13
		 */
		struct FORCEWAKE_GT_GEN9     : Register<0x0a188, 32> {
			struct Fallback_kernel_mask : Bitfield<31, 1> { };
			struct Kernel_mask          : Bitfield<16, 1> { };
			struct Fallback_kernel      : Bitfield<15, 1> { };
			struct Kernel               : Bitfield< 0, 1> { };
		};
		struct FORCEWAKE_MEDIA_GEN9  : Register<0x0a270, 32> {
			struct Fallback_kernel_mask : Bitfield<31, 1> { };
			struct Kernel_mask          : Bitfield<16, 1> { };
			struct Fallback_kernel      : Bitfield<15, 1> { };
			struct Kernel               : Bitfield< 0, 1> { };
		};
		struct FORCEWAKE_RENDER_GEN9 : Register<0x0a278, 32> {
			struct Fallback_kernel_mask : Bitfield<31, 1> { };
			struct Kernel_mask          : Bitfield<16, 1> { };
			struct Fallback_kernel      : Bitfield<15, 1> { };
			struct Kernel               : Bitfield< 0, 1> { };
		};

		struct FORCEWAKE_GEN9_RENDER_ACK : Register<0x000D84, 32> {
			struct Fallback_kernel_mask : Bitfield<31, 1> { };
			struct Kernel_mask          : Bitfield<16, 1> { };
			struct Fallback_kernel      : Bitfield<15, 1> { };
			struct Kernel               : Bitfield< 0, 1> { };
		};
		struct FORCEWAKE_GEN9_MEDIA_ACK  : Register<0x000D88, 32> {
			struct Fallback_kernel_mask : Bitfield<31, 1> { };
			struct Kernel_mask          : Bitfield<16, 1> { };
			struct Fallback_kernel      : Bitfield<15, 1> { };
			struct Kernel               : Bitfield< 0, 1> { };
		};
		struct FORCEWAKE_GEN9_GT_ACK     : Register<0x130044, 32> {
			struct Fallback_kernel_mask : Bitfield<31, 1> { };
			struct Kernel_mask          : Bitfield<16, 1> { };
			struct Fallback_kernel      : Bitfield<15, 1> { };
			struct Kernel               : Bitfield< 0, 1> { };
		};

		/*
		 * MSG idle register (one per engine) force wake status (undocumented)
		 */
		struct MSG_IDLE_CS : Register<0x8000, 32>
		{
			struct Pending_status : Bitfield<9, 5>  { };
			struct Pending_mask   : Bitfield<25, 5> { };
		};

		/* force wake status used with MSG_IDLE_CS (undocumented) */
		struct GEN9_PWRGT_DOMAIN_STATUS : Register<0xa2a0, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 703
		 *
		 * also known as FORCEWAKE_ACK_HSW in the Linux driver
		 */
		struct GTSP1 : Register<0x130044, 32> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 606
		 *
		 * also known as ECOBUS in the Linux driver
		 */
		struct MISC_CTRL0 : Register<0x0A180, 32> { };

		/*
		 * Reset control registers
		 *
		 * adds offset to given BASE address
		 */
		template <long int BASE>
		struct RESET_CTRL_BASE : Register<BASE + 0xD0, 32>
		{
			using B = Register<BASE + 0xD0, 32>;

			struct Mask_bits          : B::template Bitfield<16, 16> { };
			struct Catastrophic_error : B::template Bitfield< 2,  1> { };
			struct Ready_for_reset    : B::template Bitfield< 1,  1> { };
			struct Request_reset      : B::template Bitfield< 0,  1> { };
		};

		template <unsigned long BASE>
		struct MI_MODE_CTRL_BASE : Register<BASE + 0x9c, 32>
		{
			using F = Register<BASE + 0x9c, 32>;

			struct Rings_idle      : F::template Bitfield<     9, 1> { };
			struct Stop_rings_mask : F::template Bitfield<16 + 8, 1> { };
			struct Stop_rings      : F::template Bitfield<     8, 1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 288
		 */
		struct CS_RESET_CTRL   : RESET_CTRL_BASE  <0x02000> { };
		struct CS_MI_MODE_CTRL : MI_MODE_CTRL_BASE<0x02000> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 165
		 */
		struct BCS_RESET_CTRL   : RESET_CTRL_BASE  <0x22000> { };
		struct BCS_MI_MODE_CTRL : MI_MODE_CTRL_BASE<0x22000> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 790 ff.
		 */
		struct HSW_IDICR : Register<0x9008, 32>
		{
			struct Idi_hash_mask : Bitfield<16, 6> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 609 ff.
		 */
		struct GDRST : Register<0x0941C, 32>
		{
			struct Graphics_media1_soft_reset_ctl  : Bitfield<6, 1> { };
			struct Graphics_widi_soft_reset_ctl    : Bitfield<6, 1> { };
			struct Graphics_vebox_soft_reset_ctl   : Bitfield<4, 1> { };
			struct Graphics_blitter_soft_reset_ctl : Bitfield<3, 1> { };
			struct Graphics_media_soft_reset_ctl   : Bitfield<2, 1> { };
			struct Graphics_render_soft_reset_ctl  : Bitfield<1, 1> { };
			struct Graphics_full_soft_reset_ctl    : Bitfield<0, 1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1062 ff.
		 * IHD-OS-SKL-Vol 2c-05.16 part 2 p. 516 ff.
		 * IHD-OS-KBL-Vol 2c-1.17  part 2 p. 402 ff.
		 */
		struct FUSE2 : Register<0x09120, 32>
		{
			struct Gt_subslice_disable_fuse_gen8: Bitfield<21, 3> { };
			struct Gt_subslice_disable_fuse_gen9: Bitfield<20, 4> { };
			struct Gt_slice_enable_fuse:     Bitfield<25, 3> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1057 ff.
		 * IHD-OS-SKL-Vol 2c-05.16 part 2 p. 398 ff.
		 * IHD-OS-KBL-Vol 2c-1.17  part2 p. 397 ff.
		 *
		 * 0x9134, 0x9138, 0x913c
		 */
		struct EU_DISABLE : Register_array<0x9134, 32, 12, 8> { };

		/*
		 * IHD-OS-TGL-Vol 2c-12.21 part2 p. 81
		 */
		struct MIRROR_EU_DISABLE0 : Register<0x9134, 32>
		{
			struct Disabled : Bitfield<0, 8> { };
		};

		/*
		 * IHD-OS-TGL-Vol 2c-12.2 part 2 p.98
		 */
		struct MIRROR_GT_SLICE_EN : Register<0x9138, 32>
		{
			struct Enabled : Bitfield<0, 8> { };
		};

		/*
		 * IHD-OS-TGL-Vol 2c-12.21 part2 p.97
		 */
		struct MIRROR_GT_DSS_ENABLE : Register<0x913c, 32> { };


		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 611 ff.
		 */
		struct FENCE_REG : Register_array<0x100000, 64, NUM_FENCES, 64>
		{
			struct Fence_upper_bound : Bitfield<44, 20> { };
			struct Fence_pitch       : Bitfield<32, 11> { };
			struct Fence_lower_bound : Bitfield<12, 20> { };
			struct Tile_walk         : Bitfield< 1,  1> { };
			struct Fence_valid       : Bitfield< 0,  1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 12-11.15 p. 4
		 */
		struct TILECTL : Register<0x101000, 32>
		{
			struct SWZCTL : Bitfield<0, 2> { };
		};

		/*
		 * IHD-OS-BDW-Vol 12-11.15 p. 5
		 */
		struct GFX_FLSH_CNTL : Register<0x101008, 32> { };


		/************************
		 ** EXECLIST registers **
		 ************************/

		/* GEN8-11 */
		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 435
		 */
		template <long int BASE>
		struct EXECLIST_STATUS_BASE : Register<BASE + 0x234, 64>
		{
			using B = Register<BASE + 0x234, 64>;

			struct Current_context_id            : B::template Bitfield<32, 32> { };
			struct Arbitration_enable            : B::template Bitfield<16,  1> { };
			struct Current_active_element_status : B::template Bitfield<14,  2>
			{
				enum {
					NO_ACTIVE_ELEMENT = 0b00,
					ELEMENT_0_ACTIVE  = 0b01,
					ELEMENT_1_ACTIVE  = 0b10,
					RESERVERD         = 0b11,
				};
			};
			struct Last_context_switch_reason    : B::template Bitfield< 5, 9> { };
			struct Execlist_0_valid              : B::template Bitfield< 4, 1> { };
			struct Execlist_1_valid              : B::template Bitfield< 3, 1> { };
			struct Execlist_queue_full           : B::template Bitfield< 2, 1> { };
			struct Execlist_write_pointer        : B::template Bitfield< 1, 1> { };
			struct Current_execlist_pointer      : B::template Bitfield< 0, 1> { };
		};

		struct EXECLIST_STATUS_RSCUNIT  : EXECLIST_STATUS_BASE<0x02000> { };
		struct EXECLIST_STATUS_VCSUNIT0 : EXECLIST_STATUS_BASE<0x12000> { };
		struct EXECLIST_STATUS_VECSUNIT : EXECLIST_STATUS_BASE<0x1A000> { };
		struct EXECLIST_STATUS_VCSUNIT1 : EXECLIST_STATUS_BASE<0x1C000> { };
		struct EXECLIST_STATUS_BSCUNIT  : EXECLIST_STATUS_BASE<0x22000> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 438
		 */
		template <long int BASE>
		struct EXECLIST_SUBMITPORT_BASE : Register<BASE + 0x230, 32>
		{
			using B = Register<BASE + 0x230, 32>;

			struct Context_descriptor_dw : B::template Bitfield<0, 32> { };
		};

		struct EXECLIST_SUBMITPORT_RSCUNIT  : EXECLIST_SUBMITPORT_BASE<0x02000> { };
		struct EXECLIST_SUBMITPORT_VCSUNIT0 : EXECLIST_SUBMITPORT_BASE<0x12000> { };
		struct EXECLIST_SUBMITPORT_VECSUNIT : EXECLIST_SUBMITPORT_BASE<0x1A000> { };
		struct EXECLIST_SUBMITPORT_VCSUNIT1 : EXECLIST_SUBMITPORT_BASE<0x1C000> { };
		struct EXECLIST_SUBMITPORT_BSCUNIT  : EXECLIST_SUBMITPORT_BASE<0x22000> { };

		/* GEN12 */
		template <long int BASE>
		struct GEN12_EXECLIST_STATUS_BASE : Register<BASE + 0x234, 64>
		{
			using B = Register<BASE + 0x234, 64>;
			struct Execution_queue_invalid : B::template Bitfield<0, 1> { };
		};

		struct GEN12_EXECLIST_STATUS_RSCUNIT  : GEN12_EXECLIST_STATUS_BASE<0x02000> { };

		/*
		 * IHD-OS-TGL-Vol 2c-12.21 p. 896
		 */
		struct GEN12_EXECLIST_SQ_CONTENTS_RSCUNIT : Register_array<0x2510, 32, 16, 32> { };

		/*
		 * IHD-OS-TGL-Vol 2c-12.21 p.891
		 */
		template <long int BASE>
		struct GEN12_EXECLIST_CONTROL_BASE : Register<BASE + 0x550, 32>
		{
			using B = Register<BASE + 0x550, 32>;
			/* load submission queue into exec queue */
			struct Load : B::template Bitfield<0, 1> { };
		};

		struct GEN12_EXECLIST_CONTROL_RSCUNIT : GEN12_EXECLIST_CONTROL_BASE<0x02000> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 266 ff.
		 * IHD-OS-BDW-Vol 6-11.15 p. 19 ff.
		 */
		enum {
			CTXT_ST_BUF_NUM    = 6,
			CTXT_ST_BUF_DWORDS = 12,
		};
		template <long int BASE>
		struct CTXT_ST_BUF_BASE : Register_array<BASE + 0x370, 32, CTXT_ST_BUF_DWORDS, 32> { };

		struct CTXT_ST_BUF_RCSUNIT : CTXT_ST_BUF_BASE<0x2000> { };

		struct PGTBL_CTL2 : Register<0x20C4, 32> { };

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 252
		 */
		struct INSTPM : Register<0x20c0, 32> { };

		/*
		 * Ancient (2008) Volume 1: Graphics Core p. 252
		 */
		struct Cache_Mode_0 : Register<0x2120, 32> { };
		struct Cache_Mode_1 : Register<0x2124, 32> { };
		struct CTXT_SR_CTL  : Register<0x2714, 32> { };
		struct BB_STATE     : Register<0x2110, 32> { };
		struct BB_ADDR      : Register<0x2140, 32> { };
		struct CCID         : Register<0x2180, 32> { };
		struct CXT_SIZE       : Register<0x21A0, 32> { };
		struct CXT_SIZE_NOEXT : Register<0x21A4, 32> { };
		struct MI_DISP_PWR_DWN : Register<0x20E0, 32> { };
		struct MI_ARB_STATE    : Register<0x20E4, 32> { };
		struct MI_RDRET_STATE  : Register<0x20FC, 32> { };
		struct ECOSKPD         : Register<0x21D0, 32> { };

	private:

		struct Timer_delayer : Genode::Mmio::Delayer
		{
			Timer::Connection _timer;
			Timer_delayer(Genode::Env & env) : _timer(env) { }

			void usleep(uint64_t us) override { _timer.usleep(us); }
		} _delayer;

		void _fw_reset_gen8()
		{
			using namespace Genode;

			/* "needs to be initialized by newly loaded gfx driver" */
			write_post<FORCEWAKE_MT>(FORCEWAKE_MT::RESET);
			read<MISC_CTRL0>();
		}

		void _fw_reset_gen9()
		{
			write_post<FORCEWAKE_MEDIA_GEN9>(FORCEWAKE_MT::RESET);
			write_post<FORCEWAKE_RENDER_GEN9>(FORCEWAKE_MT::RESET);
			write_post<FORCEWAKE_GT_GEN9>(FORCEWAKE_MT::RESET);
		}

		void _forcewake_reset()
		{
			if (generation() == 8)
				_fw_reset_gen8();
			else if (generation() <= 12)
				_fw_reset_gen9();
			else Genode::error(__func__, " unsupported generation", generation());
		}


		/**
		 * Set forcewake state, i.e., prevent GT from powering down
		 */
		void _fw_enable(unsigned id)
		{
			using namespace Genode;

			unsigned const i = 1u << id;

			FORCEWAKE_MT::access_t v = 0;
			FORCEWAKE_MT::Multiple_force_wake_mask::set(v, i);
			FORCEWAKE_MT::Force_wake_request_for_thread_0::set(v, i);

			write<FORCEWAKE_MT>(v);
			read<MISC_CTRL0>();

			try {
				wait_for(Attempts(50), Microseconds(1000), _delayer,
				         GTSP1::Equal(1));
			} catch (Polling_timeout) {
				error("could not enable force-wake engine");
			}
		}

		/**
		 * Clear forcewake state
		 */
		void _fw_disable(unsigned id)
		{
			using namespace Genode;

			unsigned const i = 1u << id;

			FORCEWAKE_MT::access_t v = 0;
			FORCEWAKE_MT::Multiple_force_wake_mask::set(v, i);

			write<FORCEWAKE_MT>(v);
			read<MISC_CTRL0>();

			try {
				wait_for(Attempts(50), Microseconds(1000), _delayer,
				         GTSP1::Equal(0));
			} catch (Polling_timeout) {
				error("could not disable force-wake engine");
			}
		}

		/**
		 * Set forcewake state, i.e., prevent from powering down
		 */
		void _fw_enable_media() {
			_fw_enable<FORCEWAKE_MEDIA_GEN9, FORCEWAKE_GEN9_MEDIA_ACK>(); }

		void _fw_enable_gt() {
			_fw_enable<FORCEWAKE_GT_GEN9, FORCEWAKE_GEN9_GT_ACK>(); }

		void _fw_enable_render() {
			_fw_enable<FORCEWAKE_RENDER_GEN9, FORCEWAKE_GEN9_RENDER_ACK>(); }

		template <typename REG, typename REG_ACK>
		void _fw_enable()
		{
			using namespace Genode;

			while (read<typename REG_ACK::Kernel>()) {
				_delayer.usleep(500 * 1000);

				_fw_enable_wa<REG, REG_ACK>();
			}

			typename REG::access_t v = 0;
			REG::Kernel_mask::set(v, 1);
			REG::Kernel     ::set(v, 1);
			write<REG>(v);

			try {
				wait_for(Attempts(50), Microseconds(1000), _delayer,
				         typename REG_ACK::Equal(1));
			} catch (Polling_timeout) {
				error(__func__, " could not enable force-wake");
			}
		}

		template <typename REG, typename REG_ACK>
		void _fw_enable_wa()
		{
			using namespace Genode;

			while (read<typename REG_ACK::Fallback_kernel>()) {
				_delayer.usleep(500 * 1000);
			}

			typename REG::access_t v_set = 0;
			REG::Fallback_kernel_mask::set(v_set, 1);
			REG::Fallback_kernel     ::set(v_set, 1);
			write<REG>(v_set);

			_delayer.usleep(100 * 1000);

			while (!(read<typename REG_ACK::Fallback_kernel>())) {
				_delayer.usleep(500 * 1000);
			}

			typename REG::access_t v_clear = 0;
			REG::Fallback_kernel_mask::set(v_clear, 1);
			REG::Fallback_kernel     ::set(v_clear, 0);
			write<REG>(v_clear);
		}

		void _fw_disable_media() {
			_fw_disable<FORCEWAKE_MEDIA_GEN9, FORCEWAKE_GEN9_MEDIA_ACK>(); }

		void _fw_disable_gt() {
			_fw_disable<FORCEWAKE_GT_GEN9, FORCEWAKE_GEN9_GT_ACK>(); }

		void _fw_disable_render() {
			_fw_disable<FORCEWAKE_RENDER_GEN9, FORCEWAKE_GEN9_RENDER_ACK>(); }

		template <typename REG, typename REG_ACK>
		void _fw_disable()
		{
			typename REG::access_t v = 0;
			REG::Kernel_mask::set(v, 1);
			REG::Kernel     ::set(v, 0);
			write<REG>(v);

			while (read<typename REG_ACK::Kernel>()) {
				_delayer.usleep(500 * 1000);
			}
		}


		/**
		 * Reset interrupts
		 */
		void _intr_reset()
		{
			write_post<MASTER_INT_CTL>(0);

			write_post<GT_0_INTERRUPT_IMR>(0xffffffff);
			write<GT_0_INTERRUPT_IER>(0);

			/* do it like they do in Rome^WLinux */
			write_post<GT_0_INTERRUPT_IIR>(0xffffffff);
			write_post<GT_0_INTERRUPT_IIR>(0xffffffff);

			/* mask all in the hardware status mask register */
			write_post<HWSTAM>(0xffffffff);

			/* TODO DE intr handling (p. 363) */
		}

		void _intr_reset_gen12()
		{
			write<GEN12_GFX_MSTR_INTR::Master_interrupt_enable>(0);
			write_post<HWSTAM>(0xffffffff);
		}

		void _interrupt_reset()
		{
			if (generation() < 11)
				_intr_reset();
			else if (generation() <= 12)
				_intr_reset_gen12();
			else
				Genode::error(__func__, " unsupported generation ", generation());
		}

		/**
		 * Enable interrupts
		 */
		void _intr_enable()
		{
			using namespace Genode;

			/* GT0 RCS/BCS */
			{
				uint32_t tmp = read<GT_0_INTERRUPT_IIR>();
				if (tmp != 0) { error("GT_0_INTERRUPT_IIR not zero: ", Hex(tmp)); }

				GT_0_INTERRUPT_IER::access_t ier = 0;
				GT_0_INTERRUPT_IER::Cs_mi_user_interrupt::set(ier, 1);
				GT_0_INTERRUPT_IER::Cs_ctx_switch_interrupt::set(ier, 1);
				GT_0_INTERRUPT_IER::Bcs_mi_user_interrupt::set(ier, 1);
				GT_0_INTERRUPT_IER::Bcs_ctx_switch_interrupt::set(ier, 1);
				write<GT_0_INTERRUPT_IER>(ier);

				GT_0_INTERRUPT_IMR::access_t imr = ~0u;
				GT_0_INTERRUPT_IMR::Cs_mi_user_interrupt::set(imr, 0);
				GT_0_INTERRUPT_IMR::Cs_ctx_switch_interrupt::set(imr, 0);
				GT_0_INTERRUPT_IMR::Bcs_mi_user_interrupt::set(imr, 0);
				GT_0_INTERRUPT_IMR::Bcs_ctx_switch_interrupt::set(imr, 0);
				write_post<GT_0_INTERRUPT_IMR>(imr);

				/* demask also in the hardware status mask register */
				write_post<HWSTAM>(imr);
			}

			/* GT1 VCS1/VCS2 */
			{
				uint32_t tmp = read<GT_1_INTERRUPT_IIR>();
				if (tmp != 0) { error("GT_1_INTERRUPT_IIR not zero"); }

				GT_1_INTERRUPT_IER::access_t ier = 0;
				GT_1_INTERRUPT_IER::Vcs1_mi_user_interrupt::set(ier, 1);
				GT_1_INTERRUPT_IER::Vcs1_ctx_switch_interrupt::set(ier, 1);
				GT_1_INTERRUPT_IER::Vcs2_mi_user_interrupt::set(ier, 1);
				GT_1_INTERRUPT_IER::Vcs2_ctx_switch_interrupt::set(ier, 1);
				write<GT_1_INTERRUPT_IER>(ier);

				GT_1_INTERRUPT_IMR::access_t imr = ~0u;
				GT_1_INTERRUPT_IMR::Vcs1_mi_user_interrupt::set(imr, 0);
				GT_1_INTERRUPT_IMR::Vcs1_ctx_switch_interrupt::set(imr, 0);
				GT_1_INTERRUPT_IMR::Vcs2_mi_user_interrupt::set(imr, 0);
				GT_1_INTERRUPT_IMR::Vcs2_ctx_switch_interrupt::set(imr, 0);
				write_post<GT_1_INTERRUPT_IMR>(imr);
			}

			/* GT2 GTPM */
			{
				uint32_t tmp = read<GT_2_INTERRUPT_IIR>();
				if (tmp != 0) { error("GT_2_INTERRUPT_IIR not zero"); }

				GT_2_INTERRUPT_IER::access_t ier = 0;
				write<GT_2_INTERRUPT_IER>(ier);

				GT_2_INTERRUPT_IMR::access_t imr = ~0u;
				write_post<GT_2_INTERRUPT_IMR>(imr);
			}

			/* GT3 VECS*/
			{
				uint32_t tmp = read<GT_3_INTERRUPT_IIR>();
				if (tmp != 0) { error("GT_3_INTERRUPT_IIR not zero"); }

				GT_3_INTERRUPT_IER::access_t ier = 0;
				GT_3_INTERRUPT_IER::Vecs_mi_user_interrupt::set(ier, 1);
				GT_3_INTERRUPT_IER::Vecs_ctx_switch_interrupt::set(ier, 1);
				write<GT_3_INTERRUPT_IER>(ier);

				GT_3_INTERRUPT_IMR::access_t imr = ~0u;
				GT_3_INTERRUPT_IMR::Vecs_mi_user_interrupt::set(imr, 0);
				GT_3_INTERRUPT_IMR::Vecs_ctx_switch_interrupt::set(imr, 0);
				write_post<GT_3_INTERRUPT_IMR>(imr);
			}

			/* finally enable interrupts */
			write_post<MASTER_INT_CTL::Master_interrupt_enable>(1);
		}

		void _intr_enable_gen12()
		{
			GEN12_RENDER_INTR_VEC::access_t vec { 0 };
			GEN12_RENDER_INTR_VEC::Cs_mi_user_interrupt::set(vec, 1);
			GEN12_RENDER_INTR_VEC::Cs_ctx_switch_interrupt::set(vec, 1);
			write<GEN12_RENDER_COPY_INTR_ENABLE::Render_enable>(vec);
			write_post<HWSTAM>(~vec);
			write<GEN12_GFX_MSTR_INTR::Master_interrupt_enable>(1);
		}

		/**
		 * Disable Render P-states
		 */
		void _disable_rps()
		{
			/*
			 * Set RC0 state -- does not matter at this point b/c
			 * we disable RC states entirely.
			 */
			write_post<RC_CTRL1::Rc_state>(0);

			write<RC_CTRL0>(0);
			write<RP_FREQ_NORMAL::Turbo_disable>(1);
			write<RP_CTRL>(0);
		}

		/**
		 * Reset fence registers
		 */
		void _reset_fences()
		{
			for (int i = 0; i < NUM_FENCES; i++) {
				write<FENCE_REG>(i, 0);
				read<FENCE_REG>(i);
			}
		}

		/**
		 * Clear fault registers of all engines
		 */
		void _clear_fault_regs()
		{
			/* TODO */
		}

		/**
		 * Flush GFX TLB in the system agent
		 */
		void _gfx_flush_cntl()
		{
			write_post<GFX_FLSH_CNTL>(1);
		}

		/**
		 * Setup page attributes
		 */
		void _set_page_attributes()
		{
			enum { NUM_PAT = 8, };
			PAT_INDEX::access_t p[NUM_PAT] = {};

			unsigned const mt[NUM_PAT] {
				PAT_INDEX::Mem_type::WB,
				PAT_INDEX::Mem_type::WC,
				PAT_INDEX::Mem_type::WT,
				PAT_INDEX::Mem_type::UC,
				PAT_INDEX::Mem_type::WB,
				PAT_INDEX::Mem_type::WB,
				PAT_INDEX::Mem_type::WB,
				PAT_INDEX::Mem_type::WB,
			};

			unsigned const tc[NUM_PAT] {
				PAT_INDEX::Target_cache::LLC,
				PAT_INDEX::Target_cache::LLCELLC,
				PAT_INDEX::Target_cache::LLCELLC,
				PAT_INDEX::Target_cache::ELLC,
				PAT_INDEX::Target_cache::LLCELLC,
				PAT_INDEX::Target_cache::LLCELLC,
				PAT_INDEX::Target_cache::LLCELLC,
				PAT_INDEX::Target_cache::LLCELLC,
			};

			unsigned const la[NUM_PAT] {
				PAT_INDEX::Lru_age::UCR,
				PAT_INDEX::Lru_age::UCR,
				PAT_INDEX::Lru_age::UCR,
				PAT_INDEX::Lru_age::UCR,
				PAT_INDEX::Lru_age::UCR,
				PAT_INDEX::Lru_age::A0,
				PAT_INDEX::Lru_age::DNC,
				PAT_INDEX::Lru_age::A3,
			};

			uint64_t v = 0u;
			for (int i = 0; i < NUM_PAT; i++) {
				PAT_INDEX::Lru_age::set(     p[i], la[i]);
				PAT_INDEX::Mem_type::set(    p[i], mt[i]);
				PAT_INDEX::Target_cache::set(p[i], tc[i]);
				v |= (p[i] & 0xff) << (8 * i);
			}

			write<PAT_INDEX_L>((uint32_t)(v));
			write<PAT_INDEX_H>((uint32_t)(v >> 32));
		}

		/**
		 * Stop engine
		 */
		template <typename REG, typename REG_MI_MODE>
		bool _stop_engine()
		{
			auto mi_mode = read<REG_MI_MODE>();

			unsigned loop = 0;

			while (loop < 10 && !(REG_MI_MODE::Rings_idle::get(mi_mode))) {
				REG_MI_MODE::Stop_rings_mask::set(mi_mode, 1);
				REG_MI_MODE::Stop_rings::set(mi_mode, 1);
				write_post<REG_MI_MODE>(mi_mode);

				_delayer.usleep(10 * loop);

				mi_mode = read<REG_MI_MODE>();

				loop ++;
			}

			if (!(REG_MI_MODE::Rings_idle::get(mi_mode))) {
				Genode::error("could not stop engine");
				return false;
			}

			return true;
		}

		/**
		 * Reset engine
		 */
		template <typename REG, typename REG_MI_MODE>
		bool _reset_engine()
		{
			if (!_stop_engine<REG, REG_MI_MODE>())
				return false;

			typename REG::access_t v = 0;
			REG::Mask_bits::set(v, 1);
			REG::Request_reset::set(v, 1);
			write_post<REG>(v);
			try {
				wait_for(Attempts(35), Microseconds(20), _delayer,
				         typename REG::Ready_for_reset::Equal(1));
			} catch (Mmio::Polling_timeout) {
				Genode::error("could not reset engine");
				return false;
			}

			return true;
		}

		/**
		 * Reset engine by using hardware id
		 *
		 * \param id   hardware id of the engine to be resetted
		 */
		bool _reset_engine(unsigned id)
		{
			switch (id) {
			case RCS_ID: return _reset_engine<CS_RESET_CTRL, CS_MI_MODE_CTRL>();
			case BCS_ID: return _reset_engine<BCS_RESET_CTRL, BCS_MI_MODE_CTRL>();
			default: return true;
			}
		}

		bool _reset_engines()
		{
			bool reset_failed = false;

			for (int i = 0; i < NUM_ENGINES; i++) {
				if (!_reset_engine(i)) {
					reset_failed = true;
					Genode::warning("engine ", i, " reset failed");
				}
			}
			return !reset_failed;
		}

		/**
		 * Reset device
		 */
		void _reset_device()
		{
			bool res = _reset_engines();
			if (!res) {
				Genode::warning("cannot reset device, engines not ready");
				return;
			}

			write<GDRST::Graphics_full_soft_reset_ctl>(1);
			try {
				wait_for(Attempts(50), Microseconds(10), _delayer,
				         GDRST::Graphics_full_soft_reset_ctl::Equal(0));
			} catch (Mmio::Polling_timeout) {
				Genode::error("resetting device failed");
			}
		}

		/**
		 * Disable north DE handshake
		 */
		void _disable_nde_handshake()
		{
			NDE_RSTWRN_OPT::access_t v = read<NDE_RSTWRN_OPT>();
			NDE_RSTWRN_OPT::Rst_pch_handshake_en::set(v, 0);
			write<NDE_RSTWRN_OPT>(v);
		}

		uint32_t _clock_frequency_gen12()
		{
			uint32_t freq { 0 };

			/* TIMESTAMP_OVERRIDE when source divide logic */
			if (read<CTC_MODE::Source_divide_logic>()) {
				Genode::error("clock frequency: source divide not implemented");
			}
			/* RMP_CONFIG when crystal logic */
			else {
				freq = RPM_CONFIG0::clock_frequency(
					read<RPM_CONFIG0::Crystal_clock_frequency>());

				Genode::log("clock frequency: ", freq, " Hz from crystal logic");

				/*
				 * Shift by Ctc_shift for lowest timestamp counter because counter may
				 * not increment at every cycle
				 */
				freq >>= 3 - read<RPM_CONFIG0::Ctc_shift>();
			}

			return freq;
		}

	public:

		Mmio(Platform::Device & device, Genode::Env & env)
		: Platform::Device::Mmio(device, {0}), _delayer(env) { }

		Delayer &delayer() { return _delayer; }

		template <typename T>
		void write_post(typename T::access_t const value)
		{
			write<T>(value);
			(void)read<T>();
		}

		/**
		 * Enable execlist
		 */
		void enable_execlist()
		{
			/*
			 * This disables all interrupts delivered to the bus, make sure to
			 * enable IRQs later
			 */
			write_post<HWSTAM>(~0u);
			write<CS_MI_MODE_CTRL::Stop_rings>(0);

			using G = GFX_MODE;
			G::access_t v = 0;

			if (generation() >= 11)
				v = G::set<G::Gen11_gfx_disable_legacy_mode>(v, 1);
			else
				v = G::set<G::Execlist_enable>(v, 1);

			write<G>(v);
		}

		uint32_t clock_frequency()
		{
			if (generation() >= 11)
				return _clock_frequency_gen12();

			return 0;
		}

		void forcewake_gen8_enable() { _fw_enable(FORCEWAKE_ID_RENDER); }
		void forcewake_gen8_disable() { _fw_disable(FORCEWAKE_ID_RENDER); }

		void forcewake_gen9_enable()
		{
			_fw_enable_gt();
			_fw_enable_render();
			//_fw_enable_media();
		}

		void forcewake_gen9_disable()
		{
			//_fw_disable_media();
			_fw_disable_render();
			_fw_disable_gt();
		}

		void forcewake_enable()
		{
			switch (generation()) {
			case 8:
				forcewake_gen8_enable();
				return;
			case 9:
			case 12:
				forcewake_gen9_enable();
				return;
			default:
				Genode::error(__func__, " unsupported generation ", generation());
			}
		}

		void forcewake_disable()
		{
			switch (generation()) {
			case 8:
				forcewake_gen8_disable();
				return;
			case 9: [[fallthrough]];
			case 12:
				forcewake_gen9_disable();
				return;
			default:
				Genode::error(__func__, " unsupported generation ", generation());
			}
		}

		/* reset used during intialization only */
		void reset()
		{
			_interrupt_reset();
			_forcewake_reset();
			forcewake_enable();
			_reset_device();
			_reset_fences();

			_disable_nde_handshake();
			_set_page_attributes();
		}

		void init()
		{
			_disable_rps();
			enable_execlist();
		}

		void enable_intr()
		{
			write<Igd::Mmio::RCS_EMR>(0xffffff00);

			if (generation() < 11)
				_intr_enable();
			else
				_intr_enable_gen12();
		}

		void restore_hwstam()
		{
			if (generation() < 11)
				write_post<HWSTAM>(read<GT_0_INTERRUPT_IMR>());
			else
				write_post<HWSTAM>(~read<GEN12_RENDER_COPY_INTR_ENABLE::Render_enable>());
		}

		void disable_master_irq()
		{
			if (generation() < 11)
				write_post<Igd::Mmio::MASTER_INT_CTL::Master_interrupt_enable>(0);
			else
				write<GEN12_GFX_MSTR_INTR::Master_interrupt_enable>(0);
		}

		void enable_master_irq()
		{
			if (generation() < 11)
				write_post<Igd::Mmio::MASTER_INT_CTL::Master_interrupt_enable>(1);
			else
				write<GEN12_GFX_MSTR_INTR::Master_interrupt_enable>(1);
		}

		unsigned read_irq_vector()
		{
			unsigned vec = 0;
			if (generation() < 11) {
				vec = read<GT_0_INTERRUPT_IIR>();
			} else {
				write<GEN12_INTR_IIR_SELECTOR0::Rcs0>(1);

				try {
					wait_for(Attempts(50), Microseconds(500), _delayer,
					         GEN12_INTR_IDENTITY_REG0::Valid::Equal(1));
				} catch (Polling_timeout) {
					return vec;
				}
				vec = read<GEN12_INTR_IDENTITY_REG0::Engine_interrupt>();
				write<GEN12_INTR_IDENTITY_REG0::Valid>(1);
			}

			return vec;
		};

		void clear_render_irq(unsigned v)
		{
			if (generation() < 11)
				write_post<GT_0_INTERRUPT_IIR>(v);
			else
				write<GEN12_GT_INTR_DW0::Rcs0>(1);
		}

		void clear_render_irq()
		{
			unsigned v = 0;
			if (generation() < 11)
				v = read_irq_vector();
			clear_render_irq(v);
		}

		/**
		 * IRQ causes
		 */
		bool render_irq()
		{
			if (generation() < 11)
				return read<MASTER_INT_CTL::Render_interrupts_pending>() == 1;
			else {
				if (read<GEN12_GFX_MSTR_INTR::Gt_dw_0>() == 1 &&
				    read<GEN12_GT_INTR_DW0::Rcs0>() == 1)
					return true;
			}
			return false;
		}

		bool display_irq()
		{
			if (generation() < 11)
				return read<MASTER_INT_CTL::De_interrupts_pending>() != 0;
			else
				return read<GEN12_GFX_MSTR_INTR::Display>() == 1;
		}

		bool context_switch(unsigned const vector)
		{
			if (generation() < 11)
				return GT_0_INTERRUPT_IIR::Cs_ctx_switch_interrupt::get(vector);
			else
				return GEN12_RENDER_INTR_VEC::Cs_ctx_switch_interrupt::get(vector);
		}

		bool user_complete(unsigned const vector)
		{
			if (generation() < 11)
				return GT_0_INTERRUPT_IIR::Cs_mi_user_interrupt::get(vector);
			else
				return GEN12_RENDER_INTR_VEC::Cs_mi_user_interrupt::get(vector);
		}


		void flush_gfx_tlb() { _gfx_flush_cntl(); }

		void clear_errors()
		{
			write_post<ERROR>(0);
		}

		void update_context_status_pointer()
		{
			size_t const context_status_size = generation() < 11 ? 6 : 12;

			RCS_RING_CONTEXT_STATUS_PTR::access_t const wp = read<RCS_RING_CONTEXT_STATUS_PTR::Write_pointer>();
			if (wp > (context_status_size - 1)) {
				Genode::warning("ring context status write-pointer invalid", Genode::Hex(wp));
				return;
			}

			RCS_RING_CONTEXT_STATUS_PTR::access_t v = 0;
			RCS_RING_CONTEXT_STATUS_PTR::Read_pointer_mask::set(v, 0xff);
			RCS_RING_CONTEXT_STATUS_PTR::Read_pointer::set(v, wp);
			write<RCS_RING_CONTEXT_STATUS_PTR>(v);
		}

		uint32_t find_free_fence()
		{
			uint32_t id = 0xff;

			for (int i = 0; i < NUM_FENCES; i++) {
				if (!read<FENCE_REG::Fence_valid>(i)) {
					id = i;
					break;
				}
			}

			return id;
		}

		uint32_t update_fence(uint32_t id, addr_t lower, addr_t upper,
		                      uint32_t pitch, bool x)
		{
			FENCE_REG::access_t v = 0;

			typename FENCE_REG::access_t const l = lower >> 12;
			typename FENCE_REG::access_t const u = upper >> 12;
			FENCE_REG::Fence_upper_bound::set(v, u);
			FENCE_REG::Fence_lower_bound::set(v, l);
			FENCE_REG::Fence_pitch::set(v, pitch);
			enum { MI_TILE_XMAJOR = 0b00, MI_TILE_YMAJOR = 0b01, };
			FENCE_REG::Tile_walk::set(v, x ? MI_TILE_XMAJOR : MI_TILE_YMAJOR);
			FENCE_REG::Fence_valid::set(v, 1);

			write<FENCE_REG>(v, id);
			(void)read<FENCE_REG>(id);
			return id;
		}

		void clear_fence(uint32_t const id)
		{
			write<FENCE_REG>(0, id);
			(void)read<FENCE_REG>(id);
		}

		/**
		 * Check if fault
		 */
		bool fault_regs_valid()
		{
			bool result = false;

			/* TODO */

			return result;
		}

		/******************
		 ** clock gating **
		 ******************/

		/*
		 * gen9_init_clock_gating, lx 5.13
		 */
		struct CHICKEN_PAR1_1      : Register<0x42080, 32> {
			struct SKL_DE_COMPRESSED_HASH_MODE : Bitfield<15, 1> {};
			struct SKL_EDP_PSR_FIX_RDWRAP      : Bitfield< 3, 1> {};
		};
		struct GEN8_CHICKEN_DCPR_1 : Register<0x46430, 32> {
			struct MASK_WAKEMEM : Bitfield<13, 1> {};
		};
		struct DISP_ARB_CTL        : Register<0x45000, 32> {
			struct DISP_FBC_MEMORY_WAKE        : Bitfield<31, 1> {};
			struct DISP_TILE_SURFACE_SWIZZLING : Bitfield<13, 1> { };
			struct DISP_FBC_WM_DIS             : Bitfield<15, 1> {};
		};

		void gen9_clock_gating()
		{
			{
				auto v = read<CHICKEN_PAR1_1>();
				CHICKEN_PAR1_1::SKL_DE_COMPRESSED_HASH_MODE::set(v, 1);
				write<CHICKEN_PAR1_1>(v);
			}

			{
				auto v = read<CHICKEN_PAR1_1>();
				CHICKEN_PAR1_1::SKL_EDP_PSR_FIX_RDWRAP::set(v, 1);
				write<CHICKEN_PAR1_1>(v);
			}

			{
				auto v = read<GEN8_CHICKEN_DCPR_1>();
				GEN8_CHICKEN_DCPR_1::MASK_WAKEMEM::set(v, 1);
				write<GEN8_CHICKEN_DCPR_1>(v);
			}

			{
				auto v = read<DISP_ARB_CTL>();
				DISP_ARB_CTL::DISP_FBC_MEMORY_WAKE::set(v, 1);
				write<DISP_ARB_CTL>(v);
			}
		}

		/*
		 * kbl_init_clock_gating, lx 5.13
		 */
		struct FBC_LLC_READ_CTRL : Register<0x09044, 32> {
			struct FBC_LLC_FULLY_OPEN : Bitfield<30, 1> {};
		};
		struct ILK_DPFC_CHICKEN  : Register<0x43224, 32> {
			struct ILK_DPFC_NUKE_ON_ANY_MODIFICATION : Bitfield<23, 1> {};
		};

		void kbl_clock_gating()
		{
			gen9_clock_gating();

			{
				/* WAC6entrylatency:kbl */
				auto v = read<FBC_LLC_READ_CTRL>();
				FBC_LLC_READ_CTRL::FBC_LLC_FULLY_OPEN::set(v, 1);
				write<FBC_LLC_READ_CTRL>(v);
			}

			/* WaDisableSDEUnitClockGating:kbl 0-STEP_B0 */
			/* WaDisableGamClockGating:kbl     0-STEP_B0 */

			{
				/*
				 * WaFbcTurnOffFbcWatermark:kbl
				 * Display WA #0562: kbl
				 */
				auto v = read<DISP_ARB_CTL>();
				DISP_ARB_CTL::DISP_FBC_WM_DIS::set(v, 1);
				write<DISP_ARB_CTL>(v);
			}

			{
				/*
				 * WaFbcNukeOnHostModify:kbl
				 * Display WA #0873: kbl
				 */
				auto v = read<ILK_DPFC_CHICKEN>();
				ILK_DPFC_CHICKEN::ILK_DPFC_NUKE_ON_ANY_MODIFICATION::set(v, 1);
				write<ILK_DPFC_CHICKEN>(v);
			}
		}

		struct Arbiter_control : Register<0xb004, 32> {
			struct Gaps_tsv_enable : Bitfield<7, 1> {};
		};

		/*********************
		 ** DEBUG interface **
		 *********************/

		void dump();
		void power_dump();
		void error_dump();
		void intr_dump();
		void fault_dump();
		void execlist_status_dump();
		void context_status_pointer_dump();
};

#endif /* _MMIO_H_ */
