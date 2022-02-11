/*
 * \brief  Advanced DMA 2
 * \author Martin Stein
 * \date   2015-02-05
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <cpu/memory_barrier.h>
#include <dataspace/client.h>

/* local includes */
#include <adma2.h>

using namespace Adma2;


Table::Table(Platform::Connection &platform)
:
	_ds(platform, _ds_size, UNCACHED),
	_base_virt(_ds.local_addr<Desc::access_t>())
{ }


int Table::setup_request(size_t const size, addr_t const buffer_phys)
{
	/* sanity check */
	static size_t constexpr max_size = _max_desc * Desc::Length::max;
	if (size > max_size) {
		Genode::error("block request too large");
		return -1;
	}
	/* install new descriptors till they cover all requested bytes */
	addr_t consumed = 0;
	for (int index = 0; consumed < size; index++) {

		/* clamp current request to maximum request size */
		size_t const remaining = size - consumed;
		size_t const curr = min(Desc::Length::max, remaining);

		/* assemble new descriptor */
		Desc::access_t desc = 0;
		Desc::Address::set(desc, buffer_phys + consumed);
		Desc::Length::set(desc, curr);
		Desc::Act1::set(desc, 0);
		Desc::Act2::set(desc, 1);
		Desc::Valid::set(desc, 1);

		/* let last descriptor generate transfer-complete signal */
		if (consumed + curr == size) { Desc::End::set(desc, 1); }

		/* install and account descriptor */
		_base_virt[index] = desc;
		consumed += curr;
	}
	/* ensure that all descriptor writes were actually executed */
	Genode::memory_barrier();
	return 0;
}
