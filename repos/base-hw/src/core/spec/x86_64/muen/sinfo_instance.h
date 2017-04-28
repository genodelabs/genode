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

#ifndef _CORE__SPEC__X86_64__MUEN__SINFO_INSTANCE_H_
#define _CORE__SPEC__X86_64__MUEN__SINFO_INSTANCE_H_

/* base includes */
#include <muen/sinfo.h>

namespace Genode
{
	/**
	 * Return sinfo singleton
	 */
	Sinfo * sinfo();
}

#endif /* _CORE__SPEC__X86_64__MUEN__SINFO_INSTANCE_H_ */
