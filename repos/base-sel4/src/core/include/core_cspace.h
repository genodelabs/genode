/*
 * \brief   Core's CSpace layout definition
 * \author  Norman Feske
 * \date    2015-05-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_CSPACE_H_
#define _CORE__INCLUDE__CORE_CSPACE_H_

#include <sel4_boot_info.h>

namespace Genode { class Core_cspace; }


class Genode::Core_cspace
{
	public:

		/* CNode dimensions */
		enum {
			NUM_TOP_SEL_LOG2  = 12UL,
			/* CONFIG_ROOT_CNODE_SIZE_BITS from seL4 autoconf.h */
			NUM_CORE_SEL_LOG2 = CONFIG_ROOT_CNODE_SIZE_BITS,
			NUM_PHYS_SEL_LOG2 = 20UL,

			NUM_CORE_PAD_SEL_LOG2 = 32UL - NUM_TOP_SEL_LOG2 - NUM_CORE_SEL_LOG2,
		};

		/* selectors for initially created CNodes during core bootup */
		static inline unsigned long top_cnode_sel()      { return sel4_boot_info().empty.start; }
		static inline unsigned long core_pad_cnode_sel() { return top_cnode_sel() + 1; }
		static inline unsigned long core_cnode_sel()     { return core_pad_cnode_sel() + 1; }
		static inline unsigned long phys_cnode_sel()     { return core_cnode_sel() + 1; }
		static inline unsigned long untyped_cnode_4k()   { return phys_cnode_sel() + 1; }
		static inline unsigned long untyped_cnode_16k()  { return untyped_cnode_4k() + 1; }
		static unsigned long core_static_sel_end()       { return untyped_cnode_16k() + 1; }

		/* indices within top-level CNode */
		enum Top_cnode_idx {
			TOP_CNODE_CORE_IDX    = 0,

			/* XXX mark last index usable for PDs */

			TOP_CNODE_UNTYPED_16K = 0xffd, /* untyped objects 16K  */
			TOP_CNODE_UNTYPED_4K  = 0xffe, /* untyped objects  4K  */
			TOP_CNODE_PHYS_IDX    = 0xfff  /* physical page frames */
		};

		enum { CORE_VM_ID = 1 };
};

#endif /* _CORE__INCLUDE__CORE_CSPACE_H_ */
