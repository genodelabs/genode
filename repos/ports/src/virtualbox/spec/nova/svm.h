/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Norman Feske
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__NOVA__SVM_H_
#define _VIRTUALBOX__SPEC__NOVA__SVM_H_

/* based on HWSVMR0.h - adjusted to Genode/Nova */

#define GENODE_SVM_ASSERT_SELREG(REG) \
	AssertMsg(!pCtx->REG.Attr.n.u1Present || \
	          (pCtx->REG.Attr.n.u1Granularity \
	           ? (pCtx->REG.u32Limit & 0xfffU) == 0xfffU \
	           :  pCtx->REG.u32Limit <= 0xfffffU), \
	           ("%u %u %#x %#x %#llx\n", pCtx->REG.Attr.n.u1Present, \
	            pCtx->REG.Attr.n.u1Granularity, pCtx->REG.u32Limit, \
	            pCtx->REG.Attr.u, pCtx->REG.u64Base))

#define GENODE_READ_SELREG(REG) \
	pCtx->REG.Sel      = utcb->REG.sel; \
	pCtx->REG.ValidSel = utcb->REG.sel; \
	pCtx->REG.fFlags   = CPUMSELREG_FLAGS_VALID; \
	pCtx->REG.u32Limit = utcb->REG.limit; \
	pCtx->REG.u64Base  = utcb->REG.base; \
	pCtx->REG.Attr.u   = sel_ar_conv_from_nova(utcb->REG.ar)

static inline bool svm_save_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu)
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
	utcb->REG.sel   = pCtx->REG.Sel; \
	utcb->REG.limit = pCtx->REG.u32Limit; \
	utcb->REG.base  = pCtx->REG.u64Base; \
	utcb->REG.ar    = sel_ar_conv_to_nova(pCtx->REG.Attr.u)

static inline bool svm_load_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu)
{
	PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

#ifdef __x86_64__
	utcb->mtd  |= Nova::Mtd::EFER;
	utcb->efer  = pCtx->msrEFER | MSR_K6_EFER_SVME;
	/* unimplemented */
	if (CPUMIsGuestInLongModeEx(pCtx))
		return false;
	utcb->efer &= ~MSR_K6_EFER_LME;
#endif

	utcb->mtd |= Nova::Mtd::ESDS;
	GENODE_WRITE_SELREG(es);
	GENODE_WRITE_SELREG(ds);

	utcb->mtd |= Nova::Mtd::FSGS;
	GENODE_WRITE_SELREG(fs);
	GENODE_WRITE_SELREG(gs);

	utcb->mtd |= Nova::Mtd::CSSS;
	GENODE_WRITE_SELREG(cs);
	GENODE_WRITE_SELREG(ss);

	/* ldtr */
	utcb->mtd |= Nova::Mtd::LDTR;
	GENODE_WRITE_SELREG(ldtr);

	/* tr */
	utcb->mtd |= Nova::Mtd::TR;
	GENODE_WRITE_SELREG(tr);

	return true;
}

#undef GENODE_WRITE_SELREG

#endif /* _VIRTUALBOX__SPEC__NOVA__SVM_H_ */
