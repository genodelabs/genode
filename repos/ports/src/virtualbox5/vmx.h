/*
 * \brief  Genode specific VirtualBox SUPLib supplements
 * \author Norman Feske
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2013-2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__VMX_H_
#define _VIRTUALBOX__VMX_H_

#define GENODE_READ_SELREG_REQUIRED(REG) \
	(pCtx->REG.Sel != state->REG.value().sel) || \
	(pCtx->REG.ValidSel != state->REG.value().sel) || \
	(pCtx->REG.fFlags   != CPUMSELREG_FLAGS_VALID) || \
	(pCtx->REG.u32Limit != state->REG.value().limit) || \
	(pCtx->REG.u64Base  != state->REG.value().base) || \
	(pCtx->REG.Attr.u   != sel_ar_conv_from_genode(state->REG.value().ar))

#define GENODE_READ_SELREG(REG) \
	pCtx->REG.Sel      = state->REG.value().sel; \
	pCtx->REG.ValidSel = state->REG.value().sel; \
	pCtx->REG.fFlags   = CPUMSELREG_FLAGS_VALID; \
	pCtx->REG.u32Limit = state->REG.value().limit; \
	pCtx->REG.u64Base  = state->REG.value().base; \
	pCtx->REG.Attr.u   = sel_ar_conv_from_genode(state->REG.value().ar)

static inline bool vmx_save_state(Genode::Vm_state * state, VM * pVM, PVMCPU pVCpu)
{
	PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

	GENODE_READ_SELREG(cs);
	GENODE_READ_SELREG(ds);
	GENODE_READ_SELREG(es);
	GENODE_READ_SELREG(fs);
	GENODE_READ_SELREG(gs);
	GENODE_READ_SELREG(ss);

	if (GENODE_READ_SELREG_REQUIRED(ldtr)) {
		GENODE_READ_SELREG(ldtr);
		CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_LDTR);
	}
	if (GENODE_READ_SELREG_REQUIRED(tr)) {
		GENODE_READ_SELREG(tr);
		CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_TR);
	}

	return true;
}

#undef GENODE_READ_SELREG_REQUIRED
#undef GENODE_READ_SELREG


enum { VMCS_SEG_UNUSABLE = 0x10000 };

#define GENODE_WRITE_SELREG(REG) \
	Assert(pCtx->REG.fFlags & CPUMSELREG_FLAGS_VALID); \
	Assert(pCtx->REG.ValidSel == pCtx->REG.Sel); \
	state->REG.value(Segment{pCtx->REG.Sel, \
	                         sel_ar_conv_to_genode(pCtx->REG.Attr.u ? : VMCS_SEG_UNUSABLE), \
	                         pCtx->REG.u32Limit, \
	                         pCtx->REG.u64Base});

static inline bool vmx_load_state(Genode::Vm_state * state, VM * pVM, PVMCPU pVCpu)
{
	PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

	typedef Genode::Vm_state::Segment Segment;

	GENODE_WRITE_SELREG(es);
	GENODE_WRITE_SELREG(ds);

	GENODE_WRITE_SELREG(fs);
	GENODE_WRITE_SELREG(gs);

	GENODE_WRITE_SELREG(cs);
	GENODE_WRITE_SELREG(ss);

	/* ldtr */
	if (pCtx->ldtr.Sel == 0) {
		state->ldtr.value(Segment{0, sel_ar_conv_to_genode(0x82), 0, 0});
	} else {
		state->ldtr.value(Segment{pCtx->ldtr.Sel,
		                          sel_ar_conv_to_genode(pCtx->ldtr.Attr.u),
		                          pCtx->ldtr.u32Limit, pCtx->ldtr.u64Base});
	}

	/* tr */
	state->tr.value(Segment{pCtx->tr.Sel, sel_ar_conv_to_genode(pCtx->tr.Attr.u),
	                        pCtx->tr.u32Limit, pCtx->tr.u64Base});

	return true;
}

#undef GENODE_WRITE_SELREG

#endif /* _VIRTUALBOX__VMX_H_ */
