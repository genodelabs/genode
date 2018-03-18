/*
 * \brief  Magic ring buffer test
 * \author Emery Hemingway
 * \date   2018-04-04
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>

/* gems includes */
#include <gems/magic_ring_buffer.h>

void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	Magic_ring_buffer<int> ring_buffer(env, 4097);

	log("--- magic ring buffer test, ", ring_buffer.capacity(), " int ring ---");


	int const count = ring_buffer.capacity()/3;

	int total = 0;

	for (int j = 0; j < 99; ++j) {
		for (int i = 0; i < count; ++i) {
			ring_buffer.write_addr()[i] = i;
		}

		ring_buffer.fill(count);

		for (int i = 0; i < count; ++i) {
			if (ring_buffer.read_addr()[i] != i) {
				error("ring buffer corruption, ",
				      ring_buffer.read_addr()[i], " != ", i);
				env.parent().exit(total+i);
				return;
			}
		}
		ring_buffer.drain(count);

		total += count;
	}

	log("--- test complete, ", total, " ints passed through ring ---");
	env.parent().exit(0);
}

