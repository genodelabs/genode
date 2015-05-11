/*
 * \brief   Core's CSpace layout definition
 * \author  Norman Feske
 * \date    2015-05-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_CSPACE_H_
#define _CORE__INCLUDE__CORE_CSPACE_H_

namespace Genode { class Core_cspace; }


class Genode::Core_cspace
{
	public:

		/* CNode dimensions */
		enum {
			NUM_TOP_SEL_LOG2  = 12UL,
			NUM_CORE_SEL_LOG2 = 14UL,
			NUM_PHYS_SEL_LOG2 = 20UL,

			NUM_CORE_PAD_SEL_LOG2 = 32UL - NUM_TOP_SEL_LOG2 - NUM_CORE_SEL_LOG2,
		};

		/* selectors for statically created CNodes */
		enum Static_cnode_sel {
			TOP_CNODE_SEL         = 0xa00,
			CORE_PAD_CNODE_SEL    = 0xa01,
			CORE_CNODE_SEL        = 0xa02,
			PHYS_CNODE_SEL        = 0xa03,
			CORE_VM_PAD_CNODE_SEL = 0xa04,
			CORE_VM_CNODE_SEL     = 0xa05,
			CORE_STATIC_SEL_END,
		};

		/* indices within top-level CNode */
		enum Top_cnode_idx {
			TOP_CNODE_CORE_IDX = 0,
			TOP_CNODE_PHYS_IDX = 0xfff
		};

		enum { CORE_VM_ID = 1 };
};


#endif /* _CORE__INCLUDE__CORE_CSPACE_H_ */
