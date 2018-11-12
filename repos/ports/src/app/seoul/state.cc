/*
 * \brief  Transform state between Genode VM session interface and Seoul
 * \author Alexander Boettcher
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

#include "state.h"

void Seoul::write_vm_state(CpuState &seoul, unsigned mtr, Genode::Vm_state &state)
{
	state = Genode::Vm_state {}; /* reset */

	if (mtr & MTD_GPR_ACDB) {
		state.ax.value(seoul.rax);
		state.cx.value(seoul.rcx);
		state.dx.value(seoul.rdx);
		state.bx.value(seoul.rbx);
		mtr &= ~MTD_GPR_ACDB;
	}

	if (mtr & MTD_GPR_BSD) {
		state.di.value(seoul.rdix);
		state.si.value(seoul.rsix);
		state.bp.value(seoul.rbpx);
		mtr &= ~MTD_GPR_BSD;
	}

	if (mtr & MTD_RIP_LEN) {
		state.ip.value(seoul.ripx);
		state.ip_len.value(seoul.inst_len);
		mtr &= ~MTD_RIP_LEN;
	}

	if (mtr & MTD_RSP) {
		state.sp.value(seoul.rspx);
		mtr &= ~MTD_RSP;
	}

	if (mtr & MTD_RFLAGS) {
		state.flags.value(seoul.rflx);
		mtr &= ~MTD_RFLAGS;
	}

	if (mtr & MTD_DR) {
		state.dr7.value(seoul.dr7);
		mtr &= ~MTD_DR;
	}

	if (mtr & MTD_CR) {
		state.cr0.value(seoul.cr0);
		state.cr2.value(seoul.cr2);
		state.cr3.value(seoul.cr3);
		state.cr4.value(seoul.cr4);
		mtr &= ~MTD_CR;
	}

	typedef Genode::Vm_state::Segment Segment;
	typedef Genode::Vm_state::Range   Range;

	if (mtr & MTD_CS_SS) {
		state.cs.value(Segment{seoul.cs.sel, seoul.cs.ar, seoul.cs.limit, seoul.cs.base});
		state.ss.value(Segment{seoul.ss.sel, seoul.ss.ar, seoul.ss.limit, seoul.ss.base});
		mtr &= ~MTD_CS_SS;
	}

	if (mtr & MTD_DS_ES) {
		state.es.value(Segment{seoul.es.sel, seoul.es.ar, seoul.es.limit, seoul.es.base});
		state.ds.value(Segment{seoul.ds.sel, seoul.ds.ar, seoul.ds.limit, seoul.ds.base});
		mtr &= ~MTD_DS_ES;
	}

	if (mtr & MTD_FS_GS) {
		state.fs.value(Segment{seoul.fs.sel, seoul.fs.ar, seoul.fs.limit, seoul.fs.base});
		state.gs.value(Segment{seoul.gs.sel, seoul.gs.ar, seoul.gs.limit, seoul.gs.base});
		mtr &= ~MTD_FS_GS;
	}

	if (mtr & MTD_TR) {
		state.tr.value(Segment{seoul.tr.sel, seoul.tr.ar, seoul.tr.limit, seoul.tr.base});
		mtr &= ~MTD_TR;
	}

	if (mtr & MTD_LDTR) {
		state.ldtr.value(Segment{seoul.ld.sel, seoul.ld.ar, seoul.ld.limit, seoul.ld.base});
		mtr &= ~MTD_LDTR;
	}

	if (mtr & MTD_GDTR) {
		state.gdtr.value(Range{seoul.gd.base, seoul.gd.limit});
		mtr &= ~MTD_GDTR;
	}

	if (mtr & MTD_IDTR) {
		state.idtr.value(Range{seoul.id.base, seoul.id.limit});
		mtr &= ~MTD_IDTR;
	}

	if (mtr & MTD_SYSENTER) {
		state.sysenter_cs.value(seoul.sysenter_cs);
		state.sysenter_sp.value(seoul.sysenter_esp);
		state.sysenter_ip.value(seoul.sysenter_eip);
		mtr &= ~MTD_SYSENTER;
	}

	if (mtr & MTD_QUAL) {
		state.qual_primary.value(seoul.qual[0]);
		state.qual_secondary.value(seoul.qual[1]);
		/* not read by any kernel */
		mtr &= ~MTD_QUAL;
	}

	if (mtr & MTD_CTRL) {
		state.ctrl_primary.value(seoul.ctrl[0]);
		state.ctrl_secondary.value(seoul.ctrl[1]);
		mtr &= ~MTD_CTRL;
	}

	if (mtr & MTD_INJ) {
		state.inj_info.value(seoul.inj_info);
		state.inj_error.value(seoul.inj_error);
		mtr &= ~MTD_INJ;
	}

	if (mtr & MTD_STATE) {
		state.intr_state.value(seoul.intr_state);
		state.actv_state.value(seoul.actv_state);
		mtr &= ~MTD_STATE;
	}

	if (mtr & MTD_TSC) {
		state.tsc.value(seoul.tsc_value);
		state.tsc_offset.value(seoul.tsc_off);
		mtr &= ~MTD_TSC;
	}

	if (mtr)
		Genode::error("state transfer incomplete ", Genode::Hex(mtr));
}

unsigned Seoul::read_vm_state(Genode::Vm_state &state, CpuState &seoul)
{
	unsigned mtr = 0;

	if (state.ax.valid() || state.cx.valid() ||
	    state.dx.valid() || state.bx.valid()) {

		if (!state.ax.valid() || !state.cx.valid() ||
		    !state.dx.valid() || !state.bx.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_GPR_ACDB;

		seoul.rax   = state.ax.value();
		seoul.rcx   = state.cx.value();
		seoul.rdx   = state.dx.value();
		seoul.rbx   = state.bx.value();
	}

	if (state.bp.valid() || state.di.valid() || state.si.valid()) {

		if (!state.bp.valid() || !state.di.valid() || !state.si.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_GPR_BSD;
		seoul.rdix = state.di.value();
		seoul.rsix = state.si.value();
		seoul.rbpx = state.bp.value();
	}

	if (state.flags.valid()) {
		mtr |= MTD_RFLAGS;
		seoul.rflx = state.flags.value();
	}

	if (state.sp.valid()) {
		mtr |= MTD_RSP;
		seoul.rspx = state.sp.value();
	}

	if (state.ip.valid() || state.ip_len.valid()) {
		if (!state.ip.valid() || !state.ip_len.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_RIP_LEN;
		seoul.ripx = state.ip.value();
		seoul.inst_len = state.ip_len.value();
	}

	if (state.dr7.valid()) {
		mtr |= MTD_DR;
		seoul.dr7 = state.dr7.value();
	}
#if 0
	if (state.r8.valid()  || state.r9.valid() ||
	    state.r10.valid() || state.r11.valid() ||
	    state.r12.valid() || state.r13.valid() ||
	    state.r14.valid() || state.r15.valid()) {

		Genode::warning("r8-r15 not supported");
	}
#endif

	if (state.cr0.valid() || state.cr2.valid() ||
	    state.cr3.valid() || state.cr4.valid()) {

		mtr |= MTD_CR;

		seoul.cr0 = state.cr0.value();
		seoul.cr2 = state.cr2.value();
		seoul.cr3 = state.cr3.value();
		seoul.cr4 = state.cr4.value();
	}

	if (state.cs.valid() || state.ss.valid()) {
		if (!state.cs.valid() || !state.ss.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_CS_SS;

		seoul.cs.sel   = state.cs.value().sel;
		seoul.cs.ar    = state.cs.value().ar;
		seoul.cs.limit = state.cs.value().limit;
		seoul.cs.base  = state.cs.value().base;

		seoul.ss.sel   = state.ss.value().sel;
		seoul.ss.ar    = state.ss.value().ar;
		seoul.ss.limit = state.ss.value().limit;
		seoul.ss.base  = state.ss.value().base;
	}

	if (state.es.valid() || state.ds.valid()) {
		if (!state.es.valid() || !state.ds.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_DS_ES;

		seoul.es.sel   = state.es.value().sel;
		seoul.es.ar    = state.es.value().ar;
		seoul.es.limit = state.es.value().limit;
		seoul.es.base  = state.es.value().base;

		seoul.ds.sel   = state.ds.value().sel;
		seoul.ds.ar    = state.ds.value().ar;
		seoul.ds.limit = state.ds.value().limit;
		seoul.ds.base  = state.ds.value().base;
	}

	if (state.fs.valid() || state.gs.valid()) {
		if (!state.fs.valid() || !state.gs.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_FS_GS;

		seoul.fs.sel   = state.fs.value().sel;
		seoul.fs.ar    = state.fs.value().ar;
		seoul.fs.limit = state.fs.value().limit;
		seoul.fs.base  = state.fs.value().base;

		seoul.gs.sel   = state.gs.value().sel;
		seoul.gs.ar    = state.gs.value().ar;
		seoul.gs.limit = state.gs.value().limit;
		seoul.gs.base  = state.gs.value().base;
	}

	if (state.tr.valid()) {
		mtr |= MTD_TR;
		seoul.tr.sel   = state.tr.value().sel;
		seoul.tr.ar    = state.tr.value().ar;
		seoul.tr.limit = state.tr.value().limit;
		seoul.tr.base  = state.tr.value().base;
	}

	if (state.ldtr.valid()) {
		mtr |= MTD_LDTR;
		seoul.ld.sel   = state.ldtr.value().sel;
		seoul.ld.ar    = state.ldtr.value().ar;
		seoul.ld.limit = state.ldtr.value().limit;
		seoul.ld.base  = state.ldtr.value().base;
	}

	if (state.gdtr.valid()) {
		mtr |= MTD_GDTR;
		seoul.gd.limit = state.gdtr.value().limit;
		seoul.gd.base  = state.gdtr.value().base;
	}

	if (state.idtr.valid()) {
		mtr |= MTD_IDTR;
		seoul.id.limit = state.idtr.value().limit;
		seoul.id.base  = state.idtr.value().base;
	}

	if (state.sysenter_cs.valid() || state.sysenter_sp.valid() ||
	    state.sysenter_ip.valid()) {

		if (!state.sysenter_cs.valid() || !state.sysenter_sp.valid() ||
		    !state.sysenter_ip.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_SYSENTER;

		seoul.sysenter_cs = state.sysenter_cs.value();
		seoul.sysenter_esp = state.sysenter_sp.value();
		seoul.sysenter_eip = state.sysenter_ip.value();
	}

	if (state.ctrl_primary.valid() || state.ctrl_secondary.valid()) {
		if (!state.ctrl_primary.valid() || !state.ctrl_secondary.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_CTRL;

		seoul.ctrl[0] = state.ctrl_primary.value();
		seoul.ctrl[1] = state.ctrl_secondary.value();
	}

	if (state.inj_info.valid() || state.inj_error.valid()) {
		if (!state.inj_info.valid() || !state.inj_error.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_INJ;

		seoul.inj_info  = state.inj_info.value();
		seoul.inj_error = state.inj_error.value();
	}

	if (state.intr_state.valid() || state.actv_state.valid()) {
		if (!state.intr_state.valid() || !state.actv_state.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_STATE;

		seoul.intr_state = state.intr_state.value();
		seoul.actv_state = state.actv_state.value();
	}

	if (state.tsc.valid() || state.tsc_offset.valid()) {
		if (!state.tsc.valid() || !state.tsc_offset.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_TSC;

		seoul.tsc_value = state.tsc.value();
		seoul.tsc_off   = state.tsc_offset.value();
	}

	if (state.qual_primary.valid() || state.qual_secondary.valid()) {
		if (!state.qual_primary.valid() || !state.qual_secondary.valid())
			Genode::warning("missing state ", __LINE__);

		mtr |= MTD_QUAL;

		seoul.qual[0] = state.qual_primary.value();
		seoul.qual[1] = state.qual_secondary.value();
	}
#if 0
	if (state.efer.valid()) {
		Genode::warning("efer not supported by Seoul");
	}

	if (state.pdpte_0.valid() || state.pdpte_1.valid() ||
	    state.pdpte_2.valid() || state.pdpte_3.valid()) {

		Genode::warning("pdpte not supported by Seoul");
	}

	if (state.star.valid() || state.lstar.valid() ||
	    state.fmask.valid() || state.kernel_gs_base.valid()) {

		Genode::warning("star, lstar, fmask, kernel_gs not supported by Seoul");
	}

	if (state.tpr.valid() || state.tpr_threshold.valid()) {
		Genode::warning("tpr not supported by Seoul");
	}
#endif
	return mtr;
}
