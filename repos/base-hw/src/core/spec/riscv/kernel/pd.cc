/*
 * \brief   Kernel backend for protection domains
 * \author  Seastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <assert.h>
#include <platform_pd.h>
#include <kernel/pd.h>

using Asid_allocator = Genode::Bit_allocator<256>;

static Asid_allocator &alloc() {
	return *unmanaged_singleton<Asid_allocator>(); }


Kernel::Pd::Pd(Kernel::Pd::Table   * const table,
               Genode::Platform_pd * const platform_pd)
: Kernel::Cpu::Pd((Genode::uint8_t)alloc().alloc()),
  _table(table), _platform_pd(platform_pd)
{
	capid_t invalid = _capid_alloc.alloc();
	assert(invalid == cap_id_invalid());
}


Kernel::Pd::~Pd()
{
	while (Object_identity_reference *oir = _cap_tree.first())
		oir->~Object_identity_reference();

	/* clean up buffers of memory management */
	Cpu::invalidate_tlb_by_pid(asid);
	alloc().free(asid);
}


void Kernel::Pd::admit(Kernel::Cpu::Context * const c)
{
	c->protection_domain(asid);
	c->translation_table((addr_t)translation_table());
}
