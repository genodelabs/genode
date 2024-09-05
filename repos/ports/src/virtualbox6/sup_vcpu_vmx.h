/*
 * \brief  SUPLib vCPU VMX utilities
 * \author Norman Feske
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2013-08-21
 *
 * This header is private to sup_vcpu.cc.
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_VCPU_VMX_H_
#define _SUP_VCPU_VMX_H_

#include <VBox/vmm/hm_vmx.h>


class Sup::Vmx
{
	private:

		enum { VMCS_SEG_UNUSABLE = 0x10000 };

		enum Exit_condition
		{
			VCPU_STARTUP  = 0xfe,
			VCPU_PAUSED   = 0xff,
		};

		static inline void _handle_default(Vcpu_state &);
		static inline void _handle_startup(Vcpu_state &);
		static inline void _handle_invalid(Vcpu_state const &);

	public:

		/* prevent instantiation */
		Vmx() = delete;

		static Vm_connection::Exit_config const exit_config;

		static inline unsigned ctrl_primary();
		static inline unsigned ctrl_secondary();

		static inline void transfer_state_to_vbox(Vcpu_state const &, VMCPU &, CPUMCTX &);
		static inline void transfer_state_to_vcpu(Vcpu_state &, CPUMCTX const &);

		static inline Handle_exit_result handle_exit(Vcpu_state &);
};


Vm_connection::Exit_config const Sup::Vmx::exit_config { /* ... */ };


unsigned Sup::Vmx::ctrl_primary()
{
	/* primary VM exit controls (from src/VBox/VMM/VMMR0/HWVMXR0.cpp) */
	return 0
	       | VMX_PROC_CTLS_HLT_EXIT
	       | VMX_PROC_CTLS_MOV_DR_EXIT
	       | VMX_PROC_CTLS_UNCOND_IO_EXIT
	       | VMX_PROC_CTLS_USE_TPR_SHADOW
	       | VMX_PROC_CTLS_RDPMC_EXIT
	       ;
}


unsigned Sup::Vmx::ctrl_secondary()
{
	/* secondary VM exit controls (from src/VBox/VMM/VMMR0/HWVMXR0.cpp) */
	return 0
	       | VMX_PROC_CTLS2_APIC_REG_VIRT
	       | VMX_PROC_CTLS2_WBINVD_EXIT
	       | VMX_PROC_CTLS2_UNRESTRICTED_GUEST
	       | VMX_PROC_CTLS2_VPID
	       | VMX_PROC_CTLS2_RDTSCP
	       | VMX_PROC_CTLS2_EPT
	       | VMX_PROC_CTLS2_INVPCID
	       | VMX_PROC_CTLS2_XSAVES_XRSTORS
	       ;
}


#define GENODE_READ_SELREG_REQUIRED(REG) \
	(ctx.REG.Sel      != state.REG.value().sel) || \
	(ctx.REG.ValidSel != state.REG.value().sel) || \
	(ctx.REG.fFlags   != CPUMSELREG_FLAGS_VALID) || \
	(ctx.REG.u32Limit != state.REG.value().limit) || \
	(ctx.REG.u64Base  != state.REG.value().base) || \
	(ctx.REG.Attr.u   != sel_ar_conv_from_genode(state.REG.value().ar))

#define GENODE_READ_SELREG(REG) \
	ctx.REG.Sel      = state.REG.value().sel; \
	ctx.REG.ValidSel = state.REG.value().sel; \
	ctx.REG.fFlags   = CPUMSELREG_FLAGS_VALID; \
	ctx.REG.u32Limit = state.REG.value().limit; \
	ctx.REG.u64Base  = state.REG.value().base; \
	ctx.REG.Attr.u   = sel_ar_conv_from_genode(state.REG.value().ar)

void Sup::Vmx::transfer_state_to_vbox(Vcpu_state const &state, VMCPU &vmcpu, CPUMCTX &ctx)
{
	GENODE_READ_SELREG(cs);
	GENODE_READ_SELREG(ds);
	GENODE_READ_SELREG(es);
	GENODE_READ_SELREG(fs);
	GENODE_READ_SELREG(gs);
	GENODE_READ_SELREG(ss);

	if (GENODE_READ_SELREG_REQUIRED(ldtr)) {
		GENODE_READ_SELREG(ldtr);
		CPUMSetChangedFlags(&vmcpu, CPUM_CHANGED_LDTR);
	}
	if (GENODE_READ_SELREG_REQUIRED(tr)) {
		GENODE_READ_SELREG(tr);
		CPUMSetChangedFlags(&vmcpu, CPUM_CHANGED_TR);
	}
}

#undef GENODE_READ_SELREG_REQUIRED
#undef GENODE_READ_SELREG


#define GENODE_WRITE_SELREG(REG) \
	Assert(ctx.REG.fFlags & CPUMSELREG_FLAGS_VALID); \
	Assert(ctx.REG.ValidSel == ctx.REG.Sel); \
	state.REG.charge( Segment { .sel   = ctx.REG.Sel, \
	                            .ar    = sel_ar_conv_to_genode(ctx.REG.Attr.u ? : VMCS_SEG_UNUSABLE), \
	                            .limit = ctx.REG.u32Limit, \
	                            .base  = ctx.REG.u64Base });

void Sup::Vmx::transfer_state_to_vcpu(Vcpu_state &state, CPUMCTX const &ctx)
{
	using Segment = Vcpu_state::Segment;

	GENODE_WRITE_SELREG(cs);
	GENODE_WRITE_SELREG(ds);
	GENODE_WRITE_SELREG(es);
	GENODE_WRITE_SELREG(fs);
	GENODE_WRITE_SELREG(gs);
	GENODE_WRITE_SELREG(ss);

	if (ctx.ldtr.Sel == 0) {
		state.ldtr.charge(Segment { .sel   = 0,
		                            .ar    = sel_ar_conv_to_genode(0x82),
		                            .limit = 0,
		                            .base  = 0 });
	} else {
		state.ldtr.charge(Segment { .sel   = ctx.ldtr.Sel,
		                            .ar    = sel_ar_conv_to_genode(ctx.ldtr.Attr.u),
		                            .limit = ctx.ldtr.u32Limit,
		                            .base  = ctx.ldtr.u64Base });
	}

	state.tr.charge(Segment { .sel   = ctx.tr.Sel,
	                          .ar    = sel_ar_conv_to_genode(ctx.tr.Attr.u),
	                          .limit = ctx.tr.u32Limit,
	                          .base  = ctx.tr.u64Base });
}

#undef GENODE_WRITE_SELREG


void Sup::Vmx::_handle_invalid(Vcpu_state const &state)
{
	unsigned const dubious = state.inj_info.value() |
	                         state.intr_state.value() |
	                         state.actv_state.value();
	if (dubious)
		warning(__func__, " - dubious -"
		                  " inj_info=",   Hex(state.inj_info.value()),
		                  " inj_error=",  Hex(state.inj_error.value()),
		                  " intr_state=", Hex(state.intr_state.value()),
		                  " actv_state=", Hex(state.actv_state.value()));

	error("invalid guest state - dead");
}


void Sup::Vmx::_handle_default(Vcpu_state &state)
{
	Assert(state.actv_state.value() == VMX_VMCS_GUEST_ACTIVITY_ACTIVE);
	Assert(!VMX_EXIT_INT_INFO_IS_VALID(state.inj_info.value()));
}


Sup::Handle_exit_result Sup::Vmx::handle_exit(Vcpu_state &state)
{
	/* table 24-14. Format of Exit Reason - 15:0 Basic exit reason */
	unsigned short const exit = state.exit_reason & 0xffff;

	switch (exit) {
	case VMX_EXIT_INIT_SIGNAL:
	case VMX_EXIT_TASK_SWITCH:
	case VMX_EXIT_CPUID:
	case VMX_EXIT_RDTSC:
	case VMX_EXIT_RDTSCP:
	case VMX_EXIT_VMCALL:
	case VMX_EXIT_WBINVD:
	case VMX_EXIT_MOV_DRX:
	case VMX_EXIT_XSETBV:
	case VMX_EXIT_MOV_CRX:
	case VMX_EXIT_HLT:
		_handle_default(state);
		return { Exit_state::DEFAULT, VINF_EM_RAW_EMULATE_INSTR };

	case VMX_EXIT_INT_WINDOW:
		return { Exit_state::IRQ_WINDOW, VINF_SUCCESS };

	case VMX_EXIT_EPT_VIOLATION:
		return { Exit_state::NPT_EPT, VINF_EM_RAW_EMULATE_INSTR };

	case VMX_EXIT_IO_INSTR:
		_handle_default(state);
		/* EMHandleRCTmpl.h does not distinguish READ/WRITE rc */
		return { Exit_state::DEFAULT, VINF_IOM_R3_IOPORT_WRITE };

	case VMX_EXIT_TPR_BELOW_THRESHOLD:
		_handle_default(state);
		/* the instruction causing the exit has already been executed */
		return { Exit_state::DEFAULT, VINF_SUCCESS };

	case VMX_EXIT_RDMSR:
		_handle_default(state);
		return { Exit_state::DEFAULT, VINF_CPUM_R3_MSR_READ };

	case VMX_EXIT_WRMSR:
		_handle_default(state);
		return { Exit_state::DEFAULT, VINF_CPUM_R3_MSR_WRITE };

	case VCPU_PAUSED:
		return { Exit_state::PAUSED, VINF_SUCCESS };

	case VCPU_STARTUP:
		return { Exit_state::STARTUP, VINF_SUCCESS };

	/* error conditions */

	case VMX_EXIT_ERR_INVALID_GUEST_STATE:
		_handle_invalid(state);
		return { Exit_state::ERROR, VERR_EM_GUEST_CPU_HANG };

	case VMX_EXIT_TRIPLE_FAULT:
		return { Exit_state::ERROR, VINF_EM_TRIPLE_FAULT };

	default:
		return { Exit_state::ERROR, VERR_EM_GUEST_CPU_HANG };
	}
}

#endif /* _SUP_VCPU_VMX_H_ */
