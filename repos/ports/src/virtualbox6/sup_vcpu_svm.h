/*
 * \brief  SUPLib vCPU SVM utilities
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

#ifndef _SUP_VCPU_SVM_H_
#define _SUP_VCPU_SVM_H_

#include <VBox/vmm/hm_svm.h>


struct Sup::Svm
{
	private:

		enum Exit_condition
		{
			VCPU_SVM_NPT     = 0xfc,
			VCPU_SVM_INVALID = 0xfd,

			VCPU_STARTUP  = 0xfe,
			VCPU_PAUSED   = 0xff,
		};

		static ::uint64_t const _intercepts;

	public:

		/* prevent instantiation */
		Svm() = delete;

		static Genode::Vm_connection::Exit_config const exit_config;

		static inline unsigned ctrl_primary();
		static inline unsigned ctrl_secondary();

		static inline void transfer_state_to_vbox(Vcpu_state const &, VMCPU &, CPUMCTX &);
		static inline void transfer_state_to_vcpu(Vcpu_state &state, CPUMCTX const &);

		static inline Handle_exit_result handle_exit(Vcpu_state &);
};


Genode::Vm_connection::Exit_config const Sup::Svm::exit_config { /* ... */ };

::uint64_t const Sup::Svm::_intercepts = SVM_CTRL_INTERCEPT_INTR
                                       | SVM_CTRL_INTERCEPT_NMI
                                       | SVM_CTRL_INTERCEPT_INIT
                                       | SVM_CTRL_INTERCEPT_RDPMC
                                       | SVM_CTRL_INTERCEPT_CPUID
                                       | SVM_CTRL_INTERCEPT_RSM
                                       | SVM_CTRL_INTERCEPT_HLT
                                       | SVM_CTRL_INTERCEPT_IOIO_PROT
                                       | SVM_CTRL_INTERCEPT_MSR_PROT
                                       | SVM_CTRL_INTERCEPT_INVLPGA
                                       | SVM_CTRL_INTERCEPT_SHUTDOWN
                                       | SVM_CTRL_INTERCEPT_FERR_FREEZE
                                       | SVM_CTRL_INTERCEPT_VMRUN
                                       | SVM_CTRL_INTERCEPT_VMMCALL
                                       | SVM_CTRL_INTERCEPT_VMLOAD
                                       | SVM_CTRL_INTERCEPT_VMSAVE
                                       | SVM_CTRL_INTERCEPT_STGI
                                       | SVM_CTRL_INTERCEPT_CLGI
                                       | SVM_CTRL_INTERCEPT_SKINIT
                                       | SVM_CTRL_INTERCEPT_WBINVD
                                       | SVM_CTRL_INTERCEPT_MONITOR
                                       | SVM_CTRL_INTERCEPT_RDTSCP
                                       | SVM_CTRL_INTERCEPT_XSETBV
                                       | SVM_CTRL_INTERCEPT_MWAIT;

unsigned Sup::Svm::ctrl_primary()
{
	return (unsigned)(_intercepts & 0xffffffff);
}


unsigned Sup::Svm::ctrl_secondary()
{
	return (unsigned)((_intercepts >> 32) & 0xffffffff);
}


#define GENODE_SVM_ASSERT_SELREG(REG) \
	AssertMsg(!ctx.REG.Attr.n.u1Present || \
	          (ctx.REG.Attr.n.u1Granularity \
	           ? (ctx.REG.u32Limit & 0xfffU) == 0xfffU \
	           :  ctx.REG.u32Limit <= 0xfffffU), \
	           ("%u %u %#x %#x %#llx\n", ctx.REG.Attr.n.u1Present, \
	            ctx.REG.Attr.n.u1Granularity, ctx.REG.u32Limit, \
	            ctx.REG.Attr.u, ctx.REG.u64Base))

#define GENODE_READ_SELREG(REG) \
	ctx.REG.Sel      = state.REG.value().sel; \
	ctx.REG.ValidSel = state.REG.value().sel; \
	ctx.REG.fFlags   = CPUMSELREG_FLAGS_VALID; \
	ctx.REG.u32Limit = state.REG.value().limit; \
	ctx.REG.u64Base  = state.REG.value().base; \
	ctx.REG.Attr.u   = sel_ar_conv_from_genode(state.REG.value().ar)

void Sup::Svm::transfer_state_to_vbox(Genode::Vcpu_state const &state, VMCPU &vmcpu, CPUMCTX &ctx)
{
	GENODE_READ_SELREG(cs);
	GENODE_READ_SELREG(ds);
	GENODE_READ_SELREG(es);
	GENODE_READ_SELREG(fs);
	GENODE_READ_SELREG(gs);
	GENODE_READ_SELREG(ss);

	if (!ctx.cs.Attr.n.u1Granularity
	 &&  ctx.cs.Attr.n.u1Present
	 &&  ctx.cs.u32Limit > UINT32_C(0xfffff))
	{
		Assert((ctx.cs.u32Limit & 0xfff) == 0xfff);
		ctx.cs.Attr.n.u1Granularity = 1;
	}


	GENODE_SVM_ASSERT_SELREG(cs);
	GENODE_SVM_ASSERT_SELREG(ds);
	GENODE_SVM_ASSERT_SELREG(es);
	GENODE_SVM_ASSERT_SELREG(fs);
	GENODE_SVM_ASSERT_SELREG(gs);
	GENODE_SVM_ASSERT_SELREG(ss);

	GENODE_READ_SELREG(ldtr);
	GENODE_READ_SELREG(tr);

	CPUMSetGuestEFER(&vmcpu, CPUMGetGuestEFER(&vmcpu) & ~(::uint64_t)MSR_K6_EFER_SVME);
}

#undef GENODE_ASSERT_SELREG
#undef GENODE_READ_SELREG


#define GENODE_WRITE_SELREG(REG) \
	Assert(ctx.REG.fFlags & CPUMSELREG_FLAGS_VALID); \
	Assert(ctx.REG.ValidSel == ctx.REG.Sel); \
	state.REG.charge(Segment { .sel   = ctx.REG.Sel, \
	                           .ar    = sel_ar_conv_to_genode(ctx.REG.Attr.u), \
	                           .limit = ctx.REG.u32Limit, \
	                           .base  = ctx.REG.u64Base});

void Sup::Svm::transfer_state_to_vcpu(Genode::Vcpu_state &state, CPUMCTX const &ctx)
{
	using Segment = Genode::Vcpu_state::Segment;

	state.efer.charge(state.efer.value() | MSR_K6_EFER_SVME);

	GENODE_WRITE_SELREG(cs);
	GENODE_WRITE_SELREG(ds);
	GENODE_WRITE_SELREG(es);
	GENODE_WRITE_SELREG(fs);
	GENODE_WRITE_SELREG(gs);
	GENODE_WRITE_SELREG(ss);

	GENODE_WRITE_SELREG(ldtr);
	GENODE_WRITE_SELREG(tr);
}

#undef GENODE_WRITE_SELREG


Sup::Handle_exit_result Sup::Svm::handle_exit(Vcpu_state &state)
{
	/*
	 * Table B-1. 070h 63:0 EXITCODE
	 *
	 * Appendix C SVM Intercept Exit Codes defines only
	 * 0x000..0x403 plus -1 and -2
	 */
	unsigned short const exit = state.exit_reason & 0xffff;

	switch (exit) {
	case SVM_EXIT_CPUID:
	case SVM_EXIT_HLT:
	case SVM_EXIT_INVLPGA:
	case SVM_EXIT_IOIO:
	case SVM_EXIT_MSR:
	case SVM_EXIT_READ_CR0 ... SVM_EXIT_WRITE_CR15:
	case SVM_EXIT_RDTSC:
	case SVM_EXIT_RDTSCP:
	case SVM_EXIT_WBINVD:
		Assert(state.actv_state.value() == VMX_VMCS_GUEST_ACTIVITY_ACTIVE);
		Assert(!VMX_EXIT_INT_INFO_IS_VALID(state.inj_info.value()));
		return { Exit_state::DEFAULT, VINF_EM_RAW_EMULATE_INSTR };

	case SVM_EXIT_VINTR:
		return { Exit_state::IRQ_WINDOW, VINF_SUCCESS };

	case VCPU_SVM_NPT:
		return { Exit_state::NPT_EPT, VINF_EM_RAW_EMULATE_INSTR };

	case VCPU_PAUSED:
		return { Exit_state::PAUSED, VINF_SUCCESS };

	case VCPU_STARTUP:
		return { Exit_state::STARTUP, VINF_SUCCESS };

	/* error conditions */

	case VCPU_SVM_INVALID:
		error("invalid SVM guest state - dead");
		return { Exit_state::ERROR, VERR_EM_GUEST_CPU_HANG };

	case SVM_EXIT_SHUTDOWN:
		error("unexpected SVM exit shutdown - dead");
		return { Exit_state::ERROR, VERR_EM_GUEST_CPU_HANG };

	default:
		return { Exit_state::ERROR, VERR_EM_GUEST_CPU_HANG };
	}
}

#endif /* _SUP_VCPU_SVM_H_ */
