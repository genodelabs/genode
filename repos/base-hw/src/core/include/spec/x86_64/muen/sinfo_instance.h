/*
 * \brief  Sinfo kernel singleton
 * \author Reto Buerki
 * \date   2016-05-09
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__X86_64__MUEN__SINFO_INSTANCE_H_
#define _CORE__INCLUDE__SPEC__X86_64__MUEN__SINFO_INSTANCE_H_

/* base includes */
#include <muen/sinfo.h>
#include <platform.h>

namespace Genode
{
	/**
	 * Return sinfo singleton
	 */
	static Sinfo * sinfo() {
		static Sinfo singleton(Platform::mmio_to_virt(Sinfo::PHYSICAL_BASE_ADDR));
		return &singleton;
	}
}

#endif /* _CORE__INCLUDE__SPEC__X86_64__MUEN__SINFO_INSTANCE_H_ */
