/*
 * \brief  Dump Broadwell MMIO registers
 * \author Josef Soentgen
 * \data   2017-03-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <mmio.h>
#include <context.h>


void Igd::Mmio::dump()
{
	using namespace Genode;

	log("MMIO vaddr:", Hex(base()), " size:", Hex(Igd::Mmio::SIZE));
	log("GFX_MODE: ", Hex(read<GFX_MODE>()));
	log("  Privilege_check_disable:   ", Hex(read<GFX_MODE::Privilege_check_disable>()));
	log("  Execlist_enable:           ", Hex(read<GFX_MODE::Execlist_enable>()));
	log("  Virtual_addressing_enable: ", Hex(read<GFX_MODE::Virtual_addressing_enable>()));
	log("  Ppgtt_enable:              ", Hex(read<GFX_MODE::Ppgtt_enable>()));
	log("0x2080 - HWS_PGA: ", Hex(read<HWS_PGA_RCSUNIT>()));
	log("0x2088 - PWRCTXA: ", Hex(read<PWRCTXA>()));
	log("0x2098 - HWSTAM: ", Hex(read<HWSTAM>()));
	log("0x0D84 - DRIVER_RENDER_FWAKE_ACK: ", Hex(read<DRIVER_RENDER_FWAKE_ACK>()));
	log("0x4400 - ELEM_DESCRIPTOR1 :        ", Hex(read<ELEM_DESCRIPTOR1>()));
	log("0x4404 - ELEM_DESCRIPTOR2 :        ", Hex(read<ELEM_DESCRIPTOR2>()));
	log("0x2060 - HW_MEMRD :        ", Hex(read<HW_MEMRD>()));
	log("0x2064 - IPEIR:          ", Hex(read<IPEIR>()));
	log("0x2068 - IPEHR:          ", Hex(read<IPEHR>()));
	log("0x206C - RCS_INSTDONE:   ", Hex(read<RCS_INSTDONE>()));
	log("0x2074 - RCS_ACTHD:      ", Hex(read<RCS_ACTHD>()));
	log("0x2078 - DMA_FADD_PREF:  ", Hex(read<DMA_FADD_PREF>()));
	log("0x207C - RCS_INSTDONE_1: ", Hex(read<RCS_INSTDONE_1>()));
	log("0x2094 - NOP_ID:         ", Hex(read<NOP_ID>()));
	log("0x20C0 - INSTPM:         ", Hex(read<INSTPM>()));
	log("0x2120 - Cache_mode_0:   ", Hex(read<Cache_Mode_0>()));
	log("0x2124 - Cache_mode_1:   ", Hex(read<Cache_Mode_1>()));
	log("0x2714 - Ctx S/R Ctrl:   ", Hex(read<CTXT_SR_CTL>()));
	log("0x2140 - BB_ADDR:        ", Hex(read<BB_ADDR>()));
	log("0x2110 - BB_STATE:       ", Hex(read<BB_STATE>()));
	log("0x2180 - CCID:           ", Hex(read<CCID>()));
	log("0x21A0 - CXT_SIZE:       ", Hex(read<CXT_SIZE>()));
	log("0x21A4 - CXT_SIZE_EXT:   ", Hex(read<CXT_SIZE_NOEXT>()));
	log("0x20E0 - MI_DISP_PWR_DWN ", Hex(read<MI_DISP_PWR_DWN>()));
	log("0x20E4 - MI_ARB_STATE    ", Hex(read<MI_ARB_STATE>()));
	log("0x20FC - MI_RDRET_STATE  ", Hex(read<MI_RDRET_STATE>()));
	log("0x209C - MI_MODE         ", Hex(read<CS_MI_MODE_CTRL>()));
	log("0x21D0 - ECOSKPD         ", Hex(read<ECOSKPD>()));
}


void Igd::Mmio::power_dump()
{
	using namespace Genode;

	log("PWR_WELL_CTL2");
	log("  Misc_io_power_state:              ", Hex(read<PWR_WELL_CTL2::Misc_io_power_state>()));
	log("  Misc_io_power_request:            ", Hex(read<PWR_WELL_CTL2::Misc_io_power_request>()));
	log("  Ddi_a_and_ddi_e_io_power_state:   ", Hex(read<PWR_WELL_CTL2::Ddi_a_and_ddi_e_io_power_state>()));
	log("  Ddi_a_and_ddi_e_io_power_request: ", Hex(read<PWR_WELL_CTL2::Ddi_a_and_ddi_e_io_power_request>()));
	log("  Ddi_b_io_power_state:             ", Hex(read<PWR_WELL_CTL2::Ddi_b_io_power_state>()));
	log("  Ddi_b_io_power_request:           ", Hex(read<PWR_WELL_CTL2::Ddi_b_io_power_request>()));
	log("  Ddi_c_io_power_state:             ", Hex(read<PWR_WELL_CTL2::Ddi_c_io_power_state>()));
	log("  Ddi_c_io_power_request:           ", Hex(read<PWR_WELL_CTL2::Ddi_c_io_power_request>()));
	log("  Ddi_d_io_power_state:             ", Hex(read<PWR_WELL_CTL2::Ddi_d_io_power_state>()));
	log("  Ddi_d_io_power_request:           ", Hex(read<PWR_WELL_CTL2::Ddi_d_io_power_request>()));
	log("  Power_well_1_state:               ", Hex(read<PWR_WELL_CTL2::Power_well_1_state>()));
	log("  Power_well_1_request:             ", Hex(read<PWR_WELL_CTL2::Power_well_1_request>()));
	log("  Power_well_2_state:               ", Hex(read<PWR_WELL_CTL2::Power_well_2_state>()));
	log("  Power_well_2_request:             ", Hex(read<PWR_WELL_CTL2::Power_well_2_request>()));
}


void Igd::Mmio::error_dump()
{
	using namespace Genode;

	log("ERROR: ", Hex(read<ERROR>()));
	if (read<ERROR>()) {
		log("  Ctx_fault_ctxt_not_prsnt_err:       ", Hex(read<ERROR::Ctx_fault_ctxt_not_prsnt_err>()));
		log("  Ctx_fault_root_not_prsnt_err:       ", Hex(read<ERROR::Ctx_fault_root_not_prsnt_err>()));
		log("  Ctx_fault_pasid_not_prsnt_err:      ", Hex(read<ERROR::Ctx_fault_pasid_not_prsnt_err>()));
		log("  Ctx_fault_pasid_ovflw_err:          ", Hex(read<ERROR::Ctx_fault_pasid_ovflw_err>()));
		log("  Ctx_fault_pasid_dis_err:            ", Hex(read<ERROR::Ctx_fault_pasid_dis_err>()));
		log("  Rstrm_fault_nowb_atomic_err:        ", Hex(read<ERROR::Rstrm_fault_nowb_atomic_err>()));
		log("  Unloaded_pd_error:                  ", Hex(read<ERROR::Unloaded_pd_error>()));
		log("  Hws_page_fault_error:               ", Hex(read<ERROR::Hws_page_fault_error>()));
		log("  Invalid_page_directory_entry_error: ", Hex(read<ERROR::Invalid_page_directory_entry_error>()));
		log("  Ctx_page_fault_error:               ", Hex(read<ERROR::Ctx_page_fault_error>()));
		log("  Tlb_fault_error:                    ", Hex(read<ERROR::Tlb_fault_error>()));
	}

	log("ERROR_2: ", Hex(read<ERROR_2>()));
	if (read<ERROR_2>()) {
		log("  Tlbpend_reg_faultcnt:               ", Hex(read<ERROR_2::Tlbpend_reg_faultcnt>()));
	}

	log("RCS_EIR:        ", Hex(read<RCS_EIR>()));
	if (read<RCS_EIR::Error_identity_bits>()) {
		if (read<RCS_EIR::Error_instruction>())
			log("  Error_instruction");
		if (read<RCS_EIR::Error_mem_refresh>())
			log("  Error_mem_refresh");
		if (read<RCS_EIR::Error_page_table>())
			log("  Error_page_table");

		auto type = read<RCS_EIR::Error_identity_bits>();
		if (type != (RCS_EIR::Error_page_table::masked(type) |
		             RCS_EIR::Error_mem_refresh::masked(type) |
		             RCS_EIR::Error_instruction::masked(type)))
			log("  some unknown error bits are set");
	}

	log("RCS_ESR:        ", Hex(read<RCS_ESR>()));
	log("RCS_EMR:        ", Hex(read<RCS_EMR>()));
	log("RCS_INSTDONE:   ", Hex(read<RCS_INSTDONE>()));
	if (read<RCS_INSTDONE>() != RCS_INSTDONE::DEFAULT_VALUE ||
	    read<RCS_INSTDONE>() != 0xffffffff) {
			log("  Row_0_eu_0_done : ", Hex(read<RCS_INSTDONE::Row_0_eu_0_done >()));
			log("  Row_0_eu_1_done : ", Hex(read<RCS_INSTDONE::Row_0_eu_1_done >()));
			log("  Row_0_eu_2_done : ", Hex(read<RCS_INSTDONE::Row_0_eu_2_done >()));
			log("  Row_0_eu_3_done : ", Hex(read<RCS_INSTDONE::Row_0_eu_3_done >()));
			log("  Row_1_eu_0_done : ", Hex(read<RCS_INSTDONE::Row_1_eu_0_done >()));
			log("  Row_1_eu_1_done : ", Hex(read<RCS_INSTDONE::Row_1_eu_1_done >()));
			log("  Row_1_eu_2_done : ", Hex(read<RCS_INSTDONE::Row_1_eu_2_done >()));
			log("  Row_1_eu_3_done : ", Hex(read<RCS_INSTDONE::Row_1_eu_3_done >()));
			log("  Sf_done         : ", Hex(read<RCS_INSTDONE::Sf_done         >()));
			log("  Se_done         : ", Hex(read<RCS_INSTDONE::Se_done         >()));
			log("  Windower_done   : ", Hex(read<RCS_INSTDONE::Windower_done   >()));
			log("  Reserved1       : ", Hex(read<RCS_INSTDONE::Reserved1       >()));
			log("  Reserved2       : ", Hex(read<RCS_INSTDONE::Reserved2       >()));
			log("  Dip_done        : ", Hex(read<RCS_INSTDONE::Dip_done        >()));
			log("  Pl_done         : ", Hex(read<RCS_INSTDONE::Pl_done         >()));
			log("  Dg_done         : ", Hex(read<RCS_INSTDONE::Dg_done         >()));
			log("  Qc_done         : ", Hex(read<RCS_INSTDONE::Qc_done         >()));
			log("  Ft_done         : ", Hex(read<RCS_INSTDONE::Ft_done         >()));
			log("  Dm_done         : ", Hex(read<RCS_INSTDONE::Dm_done         >()));
			log("  Sc_done         : ", Hex(read<RCS_INSTDONE::Sc_done         >()));
			log("  Fl_done         : ", Hex(read<RCS_INSTDONE::Fl_done         >()));
			log("  By_done         : ", Hex(read<RCS_INSTDONE::By_done         >()));
			log("  Ps_done         : ", Hex(read<RCS_INSTDONE::Ps_done         >()));
			log("  Cc_done         : ", Hex(read<RCS_INSTDONE::Cc_done         >()));
			log("  Map_fl_done     : ", Hex(read<RCS_INSTDONE::Map_fl_done     >()));
			log("  Map_l2_idle     : ", Hex(read<RCS_INSTDONE::Map_l2_idle     >()));
			log("  Msg_arb_0_done  : ", Hex(read<RCS_INSTDONE::Msg_arb_0_done  >()));
			log("  Msg_arb_1_done  : ", Hex(read<RCS_INSTDONE::Msg_arb_1_done  >()));
			log("  Ic_row_0_done   : ", Hex(read<RCS_INSTDONE::Ic_row_0_done   >()));
			log("  Ic_row_1_done   : ", Hex(read<RCS_INSTDONE::Ic_row_1_done   >()));
			log("  Cp_done         : ", Hex(read<RCS_INSTDONE::Cp_done         >()));
			log("  Ring_0_enable   : ", Hex(read<RCS_INSTDONE::Ring_0_enable   >()));
	}
	log("RCS_INSTDONE_1: ", Hex(read<RCS_INSTDONE_1>()));
	log("RCS_ACTHD:      ", Hex(read<RCS_ACTHD>()));
	log("IPEHR:          ", Hex(read<IPEHR>()));
	log("IPEIR:          ", Hex(read<IPEIR>()));
	log("PGTBL_ER:       ", Hex(read<PGTBL_ER>()));
}


void Igd::Mmio::intr_dump()
{
#define DUMP_GT_1
#define DUMP_GT_2
#define DUMP_GT_3

	using namespace Genode;

	log("MASTER_INT_CTL");
	log("  Master_interrupt_enable:        ", Hex(read<MASTER_INT_CTL::Master_interrupt_enable>()));
	log("  Pcu_interrupts_pending:         ", Hex(read<MASTER_INT_CTL::Pcu_interrupts_pending>()));
	log("  Audio_codec_interrupts_pending: ", Hex(read<MASTER_INT_CTL::Audio_codec_interrupts_pending>()));
	log("  De_pch_interrupts_pending:      ", Hex(read<MASTER_INT_CTL::De_pch_interrupts_pending>()));
	log("  De_misc_interrupts_pending:     ", Hex(read<MASTER_INT_CTL::De_misc_interrupts_pending>()));
	log("  De_port_interrupts_pending:     ", Hex(read<MASTER_INT_CTL::De_port_interrupts_pending>()));
	log("  De_pipe_c_interrupts_pending:   ", Hex(read<MASTER_INT_CTL::De_pipe_c_interrupts_pending>()));
	log("  De_pipe_b_interrupts_pending:   ", Hex(read<MASTER_INT_CTL::De_pipe_b_interrupts_pending>()));
	log("  De_pipe_a_interrupts_pending:   ", Hex(read<MASTER_INT_CTL::De_pipe_a_interrupts_pending>()));
	log("  Vebox_interrupts_pending:       ", Hex(read<MASTER_INT_CTL::Vebox_interrupts_pending>()));
	log("  Gtpm_interrupts_pending:        ", Hex(read<MASTER_INT_CTL::Gtpm_interrupts_pending>()));
	log("  Vcs2_interrupts_pending:        ", Hex(read<MASTER_INT_CTL::Vcs2_interrupts_pending>()));
	log("  Vcs1_interrupts_pending:        ", Hex(read<MASTER_INT_CTL::Vcs1_interrupts_pending>()));
	log("  Blitter_interrupts_pending:     ", Hex(read<MASTER_INT_CTL::Blitter_interrupts_pending>()));
	log("  Render_interrupts_pending:      ", Hex(read<MASTER_INT_CTL::Render_interrupts_pending>()));

#ifndef DUMP_GT_0
#define DUMP_GT_0(reg) \
	log(#reg); \
	log("  Bcs_wait_on_semaphore:       ", Hex(read<reg::Bcs_wait_on_semaphore>()));       \
	log("  Bcs_ctx_switch_interrupt:    ", Hex(read<reg::Bcs_ctx_switch_interrupt>()));    \
	log("  Bcs_mi_flush_dw_notify:      ", Hex(read<reg::Bcs_mi_flush_dw_notify>()));      \
	log("  Bcs_error_interrupt:         ", Hex(read<reg::Bcs_error_interrupt>()));         \
	log("  Bcs_mi_user_interrupt:       ", Hex(read<reg::Bcs_mi_user_interrupt>()));       \
	log("  Cs_wait_on_semaphore:        ", Hex(read<reg::Cs_wait_on_semaphore>()));        \
	log("  Cs_l3_counter_slave:         ", Hex(read<reg::Cs_l3_counter_slave>()));         \
	log("  Cs_ctx_switch_interrupt:     ", Hex(read<reg::Cs_ctx_switch_interrupt>()));     \
	log("  Page_fault_error:            ", Hex(read<reg::Page_fault_error>()));            \
	log("  Cs_watchdog_counter_expired: ", Hex(read<reg::Cs_watchdog_counter_expired>())); \
	log("  L3_parity_error:             ", Hex(read<reg::L3_parity_error>()));             \
	log("  Cs_pipe_control_notify:      ", Hex(read<reg::Cs_pipe_control_notify>()));      \
	log("  Cs_error_interrupt:          ", Hex(read<reg::Cs_error_interrupt>()));          \
	log("  Cs_mi_user_interrupt:        ", Hex(read<reg::Cs_mi_user_interrupt>()));

	DUMP_GT_0(GT_0_INTERRUPT_ISR);
	DUMP_GT_0(GT_0_INTERRUPT_IIR);
	DUMP_GT_0(GT_0_INTERRUPT_IER);
	DUMP_GT_0(GT_0_INTERRUPT_IMR);
#undef DUMP_GT_0
#endif

#ifndef DUMP_GT_1
#define DUMP_GT_1(reg) \
	log(#reg); \
	log("  Vcs2_wait_on_semaphore:        ", Hex(read<reg::Vcs2_wait_on_semaphore>()));        \
	log("  Vcs2_ctx_switch_interrupt:     ", Hex(read<reg::Vcs2_ctx_switch_interrupt>()));     \
	log("  Vcs2_watchdog_counter_expired: ", Hex(read<reg::Vcs2_watchdog_counter_expired>())); \
	log("  Vcs2_mi_flush_dw_notify:       ", Hex(read<reg::Vcs2_mi_flush_dw_notify>()));       \
	log("  Vcs2_error_interrupt:          ", Hex(read<reg::Vcs2_error_interrupt>()));          \
	log("  Vcs2_mi_user_interrupt:        ", Hex(read<reg::Vcs2_mi_user_interrupt>()));        \
	log("  Vcs1_wait_on_semaphore:        ", Hex(read<reg::Vcs1_wait_on_semaphore>()));        \
	log("  Vcs1_ctx_switch_interrupt:     ", Hex(read<reg::Vcs1_ctx_switch_interrupt>()));     \
	log("  Vcs1_watchdog_counter_expired: ", Hex(read<reg::Vcs1_watchdog_counter_expired>())); \
	log("  Vcs1_pipe_control_notify:      ", Hex(read<reg::Vcs1_pipe_control_notify>()));      \
	log("  Vcs1_error_interrupt:          ", Hex(read<reg::Vcs1_error_interrupt>()));          \
	log("  Vcs1_mi_user_interrupt:        ", Hex(read<reg::Vcs1_mi_user_interrupt>()));        \

	DUMP_GT_1(GT_1_INTERRUPT_ISR);
	DUMP_GT_1(GT_1_INTERRUPT_IIR);
	DUMP_GT_1(GT_1_INTERRUPT_IER);
	DUMP_GT_1(GT_1_INTERRUPT_IMR);
#undef DUMP_GT_1
#endif

#ifndef DUMP_GT_2
#define DUMP_GT_2(reg) \
	log(#reg); \
	log("  Unslice_frequency_control_up_interrupt:                       ", Hex(read<reg::Unslice_frequency_control_up_interrupt>()));   \
	log("  Unslice_frequency_control_down_interrupt:                     ", Hex(read<reg::Unslice_frequency_control_down_interrupt>())); \
	log("  Nfafdl_frequency_up_interrupt:                                ", Hex(read<reg::Nfafdl_frequency_up_interrupt>()));      \
	log("  Nfafdl_frequency_down_interrupt:                              ", Hex(read<reg::Nfafdl_frequency_down_interrupt>()));    \
	log("  Gtpm_engines_idle_interrupt:                                  ", Hex(read<reg::Gtpm_engines_idle_interrupt>()));        \
	log("  Gtpm_uncore_to_core_trap_interrupt:                           ", Hex(read<reg::Gtpm_uncore_to_core_trap_interrupt>())); \
	log("  Gtpm_render_frequency_downwards_timeout_during_rc6_interrupt: ", Hex(read<reg::Gtpm_render_frequency_downwards_timeout_during_rc6_interrupt>())); \
	log("  Gtpm_render_p_state_up_threshold_interrupt:                   ", Hex(read<reg::Gtpm_render_p_state_up_threshold_interrupt>()));                   \
	log("  Gtpm_render_p_state_down_threshold_interrupt:                 ", Hex(read<reg::Gtpm_render_p_state_down_threshold_interrupt>()));                 \
	log("  Gtpm_render_geyserville_up_evaluation_interval_interrupt:     ", Hex(read<reg::Gtpm_render_geyserville_up_evaluation_interval_interrupt>()));     \
	log("  Gtpm_render_geyserville_down_evaluation_interval_interrupt:   ", Hex(read<reg::Gtpm_render_geyserville_down_evaluation_interval_interrupt>()));

	DUMP_GT_2(GT_2_INTERRUPT_ISR);
	DUMP_GT_2(GT_2_INTERRUPT_IIR);
	DUMP_GT_2(GT_2_INTERRUPT_IER);
	DUMP_GT_2(GT_2_INTERRUPT_IMR);
#undef DUMP_GT_2
#endif

#ifndef DUMP_GT_3
#define DUMP_GT_3(reg) \
	log(#reg); \
	log("  Performance_monitoring_buffer_half_full_interrupt: ", Hex(read<reg::Performance_monitoring_buffer_half_full_interrupt>())); \
	log("  Vecs_wait_on_semaphore:                            ", Hex(read<reg::Vecs_wait_on_semaphore>()));    \
	log("  Vecs_ctx_switch_interrupt:                         ", Hex(read<reg::Vecs_ctx_switch_interrupt>())); \
	log("  Vecs_mi_flush_dw_notify:                           ", Hex(read<reg::Vecs_mi_flush_dw_notify>()));   \
	log("  Vecs_error_interrupt:                              ", Hex(read<reg::Vecs_error_interrupt>()));      \
	log("  Vecs_mi_user_interrupt:                            ", Hex(read<reg::Vecs_mi_user_interrupt>()));    \

	DUMP_GT_3(GT_3_INTERRUPT_ISR);
	DUMP_GT_3(GT_3_INTERRUPT_IIR);
	DUMP_GT_3(GT_3_INTERRUPT_IER);
	DUMP_GT_3(GT_3_INTERRUPT_IMR);
#undef DUMP_GT_3
#endif
}


void Igd::Mmio::fault_dump()
{
	Genode::log("FAULT_TLB_RB_DATA0: ", Genode::Hex(read<FAULT_TLB_RB_DATA0>()));
	Genode::log("FAULT_TLB_RB_DATA1: ", Genode::Hex(read<FAULT_TLB_RB_DATA1>()));

	using namespace Genode;

	uint64_t const addr = ((uint64_t)(read<FAULT_TLB_RB_DATA1::Fault_cycle_va>() & 0xf) << 44) |
                          ((uint64_t) read<FAULT_TLB_RB_DATA0>() << 12);
	Genode::log("  ggtt: ", read<FAULT_TLB_RB_DATA1::Cycle_gtt_sel>(), " "
	            "addr: ", Genode::Hex(addr));
}


void Igd::Mmio::execlist_status_dump()
{
	using namespace Genode;

#define DUMP_EXECLIST_STATUS(reg) \
	log(#reg); \
	log("  Current_context_id: ", Hex(read<reg::Current_context_id>())); \
	log("  Arbitration_enable: ", Hex(read<reg::Arbitration_enable>())); \
	log("  Current_active_element_status: ", Hex(read<reg::Current_active_element_status>())); \
	log("  Last_context_switch_reason: ", Hex(read<reg::Last_context_switch_reason>())); \
	if (read<reg::Last_context_switch_reason>()) { \
		Igd::Context_status_qword::access_t v = read<reg::Last_context_switch_reason>(); \
		log("   Wait_on_scanline:  ", Igd::Context_status_qword::Wait_on_scanline::get(v)); \
		log("   Wait_on_semaphore: ", Igd::Context_status_qword::Wait_on_semaphore::get(v)); \
		log("   Wait_on_v_blank:   ", Igd::Context_status_qword::Wait_on_v_blank::get(v)); \
		log("   Wait_on_sync_flip: ", Igd::Context_status_qword::Wait_on_sync_flip::get(v)); \
		log("   Context_complete:  ", Igd::Context_status_qword::Context_complete::get(v)); \
		log("   Active_to_idle:    ", Igd::Context_status_qword::Active_to_idle::get(v)); \
		log("   Element_switch:    ", Igd::Context_status_qword::Element_switch::get(v)); \
		log("   Preempted:         ", Igd::Context_status_qword::Preempted::get(v)); \
		log("   Idle_to_active:    ", Igd::Context_status_qword::Idle_to_active::get(v)); \
	} \
	log("  Execlist_0_valid: ", Hex(read<reg::Execlist_0_valid>())); \
	log("  Execlist_1_valid: ", Hex(read<reg::Execlist_1_valid>())); \
	log("  Execlist_queue_full: ", Hex(read<reg::Execlist_queue_full>())); \
	log("  Execlist_write_pointer: ", Hex(read<reg::Execlist_write_pointer>())); \
	log("  Current_execlist_pointer: ", Hex(read<reg::Current_execlist_pointer>()));

	DUMP_EXECLIST_STATUS(EXECLIST_STATUS_RSCUNIT);

#undef DUMP_EXECLIST_STATUS
}


void Igd::Mmio::context_status_pointer_dump()
{
	using namespace Genode;

	uint32_t const v  = read<RCS_RING_CONTEXT_STATUS_PTR>();
	uint32_t const wp = read<RCS_RING_CONTEXT_STATUS_PTR::Write_pointer>();
	uint32_t const rp = read<RCS_RING_CONTEXT_STATUS_PTR::Read_pointer>();

	log("RCS_RING_CONTEXT_STATUS_PTR: ", Hex(v, Hex::PREFIX, Hex::PAD));
	log("  Read pointer:   ", Hex(rp));
	log("  Write pointer:  ", Hex(wp));

	if (wp == 0x7) {
		warning("RCS seems to be idle");
		return;
	}

	uint32_t r = rp;
	uint32_t w = wp;

	while (r != w) {

		if (++r == CTXT_ST_BUF_NUM) { r = 0; }

		uint32_t const i = r;

		uint32_t const csu = read<CTXT_ST_BUF_RCSUNIT>(i*2+1);
		uint32_t const csl = read<CTXT_ST_BUF_RCSUNIT>(i*2);
		uint64_t const cs  = ((uint64_t)csu << 32) | csl;

		log(i, "  Context_status:     ", Hex(cs));

		Igd::Context_status_qword::access_t const v = cs;
		log(i, "    Context_complete:  ", Igd::Context_status_qword::Context_complete::get(v));
		log(i, "    Active_to_idle:    ", Igd::Context_status_qword::Active_to_idle::get(v));
		log(i, "    Element_switch:    ", Igd::Context_status_qword::Element_switch::get(v));
		log(i, "    Preempted:         ", Igd::Context_status_qword::Preempted::get(v));
		log(i, "    Idle_to_active:    ", Igd::Context_status_qword::Idle_to_active::get(v));

		log(i, "  Context_status_udw: ", Hex(csu));
		log(i, "  Context_status_ldw: ", Hex(csl));
	}
}
