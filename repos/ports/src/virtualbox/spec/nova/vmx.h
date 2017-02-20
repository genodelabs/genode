/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Norman Feske
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__NOVA__VMX_H_
#define _VIRTUALBOX__SPEC__NOVA__VMX_H_

#define GENODE_READ_SELREG_REQUIRED(REG) \
	(pCtx->REG.Sel != utcb->REG.sel) || \
	(pCtx->REG.ValidSel != utcb->REG.sel) || \
	(pCtx->REG.fFlags   != CPUMSELREG_FLAGS_VALID) || \
	(pCtx->REG.u32Limit != utcb->REG.limit) || \
	(pCtx->REG.u64Base  != utcb->REG.base) || \
	(pCtx->REG.Attr.u   != sel_ar_conv_from_nova(utcb->REG.ar))

#define GENODE_READ_SELREG(REG) \
	pCtx->REG.Sel      = utcb->REG.sel; \
	pCtx->REG.ValidSel = utcb->REG.sel; \
	pCtx->REG.fFlags   = CPUMSELREG_FLAGS_VALID; \
	pCtx->REG.u32Limit = utcb->REG.limit; \
	pCtx->REG.u64Base  = utcb->REG.base; \
	pCtx->REG.Attr.u   = sel_ar_conv_from_nova(utcb->REG.ar)

static inline bool vmx_save_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu)
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
	utcb->REG.sel   = pCtx->REG.Sel; \
	utcb->REG.limit = pCtx->REG.u32Limit; \
	utcb->REG.base  = pCtx->REG.u64Base; \
	utcb->REG.ar    = sel_ar_conv_to_nova(pCtx->REG.Attr.u ? : VMCS_SEG_UNUSABLE);

static inline bool vmx_load_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu)
{
	PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

	{
		utcb->mtd |= Nova::Mtd::ESDS;
		GENODE_WRITE_SELREG(es);
		GENODE_WRITE_SELREG(ds);
	}

	{
		utcb->mtd |= Nova::Mtd::FSGS;
		GENODE_WRITE_SELREG(fs);
		GENODE_WRITE_SELREG(gs);
	}

	{
		utcb->mtd |= Nova::Mtd::CSSS;
		GENODE_WRITE_SELREG(cs);
		GENODE_WRITE_SELREG(ss);
	}

	/* ldtr */
	if (pCtx->ldtr.Sel == 0) {
		{
			utcb->mtd |= Nova::Mtd::LDTR;

			utcb->ldtr.sel   = 0;
			utcb->ldtr.limit = 0;
			utcb->ldtr.base  = 0;
			utcb->ldtr.ar    = sel_ar_conv_to_nova(0x82);
		}
	} else {
		{
			utcb->mtd |= Nova::Mtd::LDTR;

			utcb->ldtr.sel   = pCtx->ldtr.Sel;
			utcb->ldtr.limit = pCtx->ldtr.u32Limit;
			utcb->ldtr.base  = pCtx->ldtr.u64Base;
			utcb->ldtr.ar    = sel_ar_conv_to_nova(pCtx->ldtr.Attr.u);
		}
	}

	/* tr */
	Assert(pCtx->tr.Attr.u & X86_SEL_TYPE_SYS_TSS_BUSY_MASK);
	{
		utcb->mtd |= Nova::Mtd::TR;

		utcb->tr.sel   = pCtx->tr.Sel;
		utcb->tr.limit = pCtx->tr.u32Limit;
		utcb->tr.base  = pCtx->tr.u64Base;
		utcb->tr.ar    = sel_ar_conv_to_nova(pCtx->tr.Attr.u);
	}

	return true;
}

#undef GENODE_WRITE_SELREG

#endif /* _VIRTUALBOX__SPEC__NOVA__VMX_H_ */
