/*
 * \brief  Test for cache performance
 * \author Johannes Schlatow
 * \date   2019-05-02
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/heap.h>

#include "genode_time.h"
#include "common.h"

void triplet_test(void * src, void * dst, size_t size, unsigned iterations)
{
	size_t size_kb = size / 1024;

	unsigned long res1 = timed_test(src, nullptr, size, iterations, touch_words);
	unsigned long res2 = timed_test(src, src,     size, iterations, memcpy_cpu);
	unsigned long res3 = timed_test(src, dst,     size, iterations, memcpy_cpu);

	log(size_kb, "KB (Cycles/KB): ",
	             res1 / size_kb / iterations, " | ",
	             res2 / size_kb / iterations, " | ",
	             res3 / size_kb / iterations);
}


struct Main
{
	Genode::Env &env;

	Genode::Heap heap { env.ram(), env.rm() };

	Main(Genode::Env &env);
};


Main::Main(Genode::Env &env) : env(env)
{
	using namespace Genode;

	enum { MAX_KB = 4*1024 };

	struct Buffer { char buf[MAX_KB*1024]; };

	Buffer * buf1 = new (heap) Buffer;
	Buffer * buf2 = new (heap) Buffer;

	memset(buf1, 0, MAX_KB*1024);
	memset(buf2, 0, MAX_KB*1024);

	log("--- test-cache started (touch words | touch lines | memcpy) ---");

	sweep_test<8, MAX_KB>(buf1, buf2, 30, triplet_test);

	log("--- test-cache done ---");

	destroy(heap, buf1);
	destroy(heap, buf2);
}

void Component::construct(Genode::Env &env) { static Main inst(env); }

