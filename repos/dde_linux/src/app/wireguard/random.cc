/*
 * \brief  Randomness backend for lx_emul
 * \author Stefan Kalkowski
 * \date   2022-01-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <lx_kit/env.h>
#include <lx_emul/random.h>
#include <jitterentropy.h>


extern "C" void lx_emul_random_bytes(void * buf, int bytes)
{
	static rand_data * jent = nullptr;

	if (!jent) {
		jitterentropy_init(Lx_kit::env().heap);

		if (jent_entropy_init() != 0)
			Genode::error("jitterentropy library could not be initialized!");

		jent = jent_entropy_collector_alloc(0, 0);
		if (!jent)
			Genode::error("jitterentropy could not allocate entropy collector!");
	}

	jent_read_entropy(jent, (char*)buf, bytes);
}
