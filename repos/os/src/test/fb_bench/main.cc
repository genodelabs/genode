/*
 * \brief  Framebuffer throughput test
 * \author Norman Feske
 * \author Martin Stein
 * \date   2015-06-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_dataspace.h>
#include <blit/blit.h>
#include <framebuffer_session/connection.h>
#include <timer_session/connection.h>

using namespace Genode;

struct Test
{
	enum { DURATION_MS = 2000 };

	Env                     &env;
	int                      id;
	Timer::Connection        timer   { env };
	Heap                     heap    { env.ram(), env.rm() };
	Framebuffer::Connection  fb      { env, Framebuffer::Mode() };
	Attached_dataspace       fb_ds   { env.rm(), fb.dataspace() };
	Framebuffer::Mode const  fb_mode { fb.mode() };
	char                    *buf[2];

	Test(Env &env, int id, char const *brief) : env(env), id(id)
	{
		log("\nTEST ", id, ": ", brief, "\n");
		for (unsigned i = 0; i < 2; i++) {
			if (!heap.alloc(fb_ds.size(), (void **)&buf[i])) {
				env.parent().exit(-1); }
		}
		/* fill one memory buffer with white pixels */
		memset(buf[1], ~0, fb_ds.size());
	}

	void conclusion(unsigned kib, unsigned start_ms, unsigned end_ms) {
		log("throughput: ", kib / (end_ms - start_ms), " MiB/sec"); }

	~Test() { log("\nTEST ", id, " finished\n"); }

	private:

		/*
		 * Noncopyable
		 */
		Test(Test const &);
		Test &operator = (Test const &);
};

struct Bytewise_ram_test : Test
{
	static constexpr char const *brief = "byte-wise memcpy from RAM to RAM";

	Bytewise_ram_test(Env &env, int id) : Test(env, id, brief)
	{
		unsigned       kib      = 0;
		unsigned const start_ms = timer.elapsed_ms();
		for (; timer.elapsed_ms() - start_ms < DURATION_MS;) {
			memcpy(buf[0], buf[1], fb_ds.size());
			kib += fb_ds.size() / 1024;
		}
		conclusion(kib, start_ms, timer.elapsed_ms());
	}
};

struct Bytewise_fb_test : Test
{
	static constexpr char const *brief = "byte-wise memcpy from RAM to FB";

	Bytewise_fb_test(Env &env, int id) : Test(env, id, brief)
	{
		unsigned       kib      = 0;
		unsigned const start_ms = timer.elapsed_ms();
		for (unsigned i = 0; timer.elapsed_ms() - start_ms < DURATION_MS; i++) {
			memcpy(fb_ds.local_addr<char>(), buf[i % 2], fb_ds.size());
			kib += fb_ds.size() / 1024;
		}
		conclusion(kib, start_ms, timer.elapsed_ms());
	}
};

struct Blit_test : Test
{
	static constexpr char const *brief = "copy via blit library from RAM to FB";

	Blit_test(Env &env, int id) : Test(env, id, brief)
	{
		unsigned       kib      = 0;
		unsigned const start_ms = timer.elapsed_ms();
		unsigned const w        = fb_mode.width() * fb_mode.bytes_per_pixel();
		unsigned const h        = fb_mode.height();
		for (unsigned i = 0; timer.elapsed_ms() - start_ms < DURATION_MS; i++) {
			blit(buf[i % 2], w, fb_ds.local_addr<char>(), w, w, h);
			kib += (w * h) / 1024;
		}
		conclusion(kib, start_ms, timer.elapsed_ms());
	}
};

struct Unaligned_blit_test : Test
{
	static constexpr char const *brief = "unaligned copy via blit library from RAM to FB";

	Unaligned_blit_test(Env &env, int id) : Test(env, id, brief)
	{
		unsigned       kib      = 0;
		unsigned const start_ms = timer.elapsed_ms();
		unsigned const w        = fb_mode.width() * fb_mode.bytes_per_pixel();
		unsigned const h        = fb_mode.height();
		for (unsigned i = 0; timer.elapsed_ms() - start_ms < DURATION_MS; i++) {
			blit(buf[i % 2] + 2, w, fb_ds.local_addr<char>() + 2, w, w - 2, h);
			kib += (w * h) / 1024;
		}
		conclusion(kib, start_ms, timer.elapsed_ms());
	}
};

struct Main
{
	Constructible<Bytewise_ram_test>   test_1 { };
	Constructible<Bytewise_fb_test>    test_2 { };
	Constructible<Blit_test>           test_3 { };
	Constructible<Unaligned_blit_test> test_4 { };

	Main(Env &env)
	{
		log("--- Framebuffer benchmark ---");
		test_1.construct(env, 1); test_1.destruct();
		test_2.construct(env, 2); test_2.destruct();
		test_3.construct(env, 3); test_3.destruct();
		test_4.construct(env, 4); test_4.destruct();
		log("--- Framebuffer benchmark finished ---");
	}
};

void Component::construct(Env &env) { static Main main(env); }
