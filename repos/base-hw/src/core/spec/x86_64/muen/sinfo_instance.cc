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

#include <sinfo_instance.h>
#include <platform.h>

Genode::Sinfo * Genode::sinfo()
{
	static Sinfo singleton(Platform::mmio_to_virt(Sinfo::PHYSICAL_BASE_ADDR));
	return &singleton;
}
