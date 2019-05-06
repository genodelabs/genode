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

#ifndef _VIRTUALBOX__SVM_H_
#define _VIRTUALBOX__SVM_H_

/* based on HWSVMR0.h - adjusted to Genode */

#define GENODE_SVM_ASSERT_SELREG(REG) \
	AssertMsg(!pCtx->REG.Attr.n.u1Present || \
	          (pCtx->REG.Attr.n.u1Granularity \
	           ? (pCtx->REG.u32Limit & 0xfffU) == 0xfffU \
	           :  pCtx->REG.u32Limit <= 0xfffffU), \
	           ("%u %u %#x %#x %#llx\n", pCtx->REG.Attr.n.u1Present, \
	            pCtx->REG.Attr.n.u1Granularity, pCtx->REG.u32Limit, \
	            pCtx->REG.Attr.u, pCtx->REG.u64Base))

#define GENODE_READ_SELREG(REG) \
	pCtx->REG.Sel      = state->REG.value().sel; \
	pCtx->REG.ValidSel = state->REG.value().sel; \
	pCtx->REG.fFlags   = CPUMSELREG_FLAGS_VALID; \
	pCtx->REG.u32Limit = state->REG.value().limit; \
	pCtx->REG.u64Base  = state->REG.value().base; \
	pCtx->REG.Attr.u   = sel_ar_conv_from_genode(state->REG.value().ar)

static inline bool svm_save_state(Genode::Vm_state * state, VM * pVM, PVMCPU pVCpu)
{
	PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

	GENODE_READ_SELREG(cs);
	GENODE_READ_SELREG(ds);
	GENODE_READ_SELREG(es);
	GENODE_READ_SELREG(fs);
	GENODE_READ_SELREG(gs);
	GENODE_READ_SELREG(ss);

	GENODE_SVM_ASSERT_SELREG(cs);
	GENODE_SVM_ASSERT_SELREG(ds);
	GENODE_SVM_ASSERT_SELREG(es);
	GENODE_SVM_ASSERT_SELREG(fs);
	GENODE_SVM_ASSERT_SELREG(gs);
	GENODE_SVM_ASSERT_SELREG(ss);

	GENODE_READ_SELREG(ldtr);
	GENODE_READ_SELREG(tr);

	return true;
}

#undef GENODE_ASSERT_SELREG
#undef GENODE_READ_SELREG




#define GENODE_WRITE_SELREG(REG) \
	Assert(pCtx->REG.fFlags & CPUMSELREG_FLAGS_VALID); \
	Assert(pCtx->REG.ValidSel == pCtx->REG.Sel); \
	state->REG.value(Segment{pCtx->REG.Sel, sel_ar_conv_to_genode(pCtx->REG.Attr.u), \
	                         pCtx->REG.u32Limit, pCtx->REG.u64Base});

static inline bool svm_load_state(Genode::Vm_state * state, VM * pVM, PVMCPU pVCpu)
{
	typedef Genode::Vm_state::Segment Segment;

	PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

	state->efer.value(pCtx->msrEFER | MSR_K6_EFER_SVME);
	/* unimplemented */
	if (CPUMIsGuestInLongModeEx(pCtx))
		return false;
	state->efer.value(state->efer.value() & ~MSR_K6_EFER_LME);

	GENODE_WRITE_SELREG(es);
	GENODE_WRITE_SELREG(ds);

	GENODE_WRITE_SELREG(fs);
	GENODE_WRITE_SELREG(gs);

	GENODE_WRITE_SELREG(cs);
	GENODE_WRITE_SELREG(ss);

	GENODE_WRITE_SELREG(ldtr);
	GENODE_WRITE_SELREG(tr);

	return true;
}

#undef GENODE_WRITE_SELREG

#endif /* _VIRTUALBOX__SVM_H_ */
