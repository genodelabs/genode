/*
 * \brief  Access to kernel info page (KIP)
 * \author Julian Stecklina
 * \date   2008-02-20
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__KIP_H_
#define _CORE__INCLUDE__KIP_H_

namespace Pistachio {
#include <l4/types.h>
#include <l4/kip.h>

	/**
	 * Return a pointer to the kernel info page
	 */
	L4_KernelInterfacePage_t *get_kip();

	unsigned int get_page_size_log2();

	L4_Word_t get_page_mask();

	inline L4_Word_t get_page_size()
	{
		return 1<<get_page_size_log2();
	}

	inline L4_ThreadId_t get_sigma0()
	{
		/* from l4/sigma0.h */
		return L4_GlobalId (get_kip()->ThreadInfo.X.UserBase, 1);
	}

	inline unsigned int get_user_base()
	{
		return get_kip()->ThreadInfo.X.UserBase;
	}

	inline unsigned int get_threadno_bits()
	{
#ifdef L4_32BIT
		return 18;
#else
#error "Unsupported architecture."
#endif
	}
}

#endif /* _CORE__INCLUDE__KIP_H_ */
