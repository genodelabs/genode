/*
 * \brief   Kernel backend for protection domains
 * \author  Stefan Kalkowski
 * \date    2015-03-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_pd.h>
#include <kernel/pd.h>

Kernel::Pd::Pd(Kernel::Pd::Table   * const table,
               Genode::Platform_pd * const platform_pd)
: _table(table), _platform_pd(platform_pd)
{
	capid_t invalid = _capid_alloc.alloc();
	assert(invalid == cap_id_invalid());
}


Kernel::Pd::~Pd()
{
	while (Object_identity_reference *oir = _cap_tree.first())
		oir->~Object_identity_reference();
}


void Kernel::Pd::admit(Kernel::Cpu::Context * const c) {
	c->init((addr_t)translation_table(), this == Kernel::core_pd()); }
