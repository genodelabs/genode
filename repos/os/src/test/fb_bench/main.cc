/*
 * \brief  Framebuffer throughput test
 * \author Norman Feske
 * \date   2015-06-05
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <os/attached_dataspace.h>
#include <blit/blit.h>
#include <framebuffer_session/connection.h>
#include <timer_session/connection.h>


static unsigned long now_ms()
{
	static Timer::Connection timer;
	return timer.elapsed_ms();
}


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- test-fb_bench started ---\n");

	static Framebuffer::Connection fb;

	static Attached_dataspace fb_ds(fb.dataspace());
	static Framebuffer::Mode const fb_mode = fb.mode();

	/*
	 * Allocate two memory buffers as big as the framebuffer.
	 */
	char *src_buf[2];
	for (unsigned i = 0; i < 2; i++)
		src_buf[i] = (char *)env()->heap()->alloc(fb_ds.size());

	/* duration of individual test, in milliseconds */
	unsigned long duration_ms = 2000;

	printf("byte-wise memcpy from RAM to RAM...\n");
	{
		unsigned long transferred_kib = 0;
		unsigned long const start_ms = now_ms();

		for (; now_ms() - start_ms < duration_ms;) {
			memcpy(src_buf[0], src_buf[1], fb_ds.size());
			transferred_kib += fb_ds.size() / 1024;
		}

		unsigned long const end_ms = now_ms();

		printf("-> %ld MiB/sec\n",
		       (transferred_kib)/(end_ms - start_ms));
	}

	/*
	 * Fill one memory buffer with white pixels.
	 */
	memset(src_buf[1], ~0, fb_ds.size());

	printf("byte-wise memcpy from RAM to framebuffer...\n");
	{
		unsigned long transferred_kib = 0;
		unsigned long const start_ms = now_ms();

		for (unsigned i = 0; now_ms() - start_ms < duration_ms; i++) {
			memcpy(fb_ds.local_addr<char>(), src_buf[i % 2], fb_ds.size());
			transferred_kib += fb_ds.size() / 1024;
		}

		unsigned long const end_ms = now_ms();

		printf("-> %ld MiB/sec\n",
		       (transferred_kib)/(end_ms - start_ms));
	}

	/*
	 * Blitting via the blit library from RAM to framebuffer
	 */
	printf("copy via blit library from RAM to framebuffer...\n");
	{
		unsigned long transferred_kib = 0;
		unsigned long const start_ms = now_ms();

		/* line width in bytes */
		unsigned const w = fb_mode.width() * fb_mode.bytes_per_pixel();
		unsigned const h = fb_mode.height();

		for (unsigned i = 0; now_ms() - start_ms < duration_ms; i++) {
			blit(src_buf[i % 2], w, fb_ds.local_addr<char>(), w, w, h);

			transferred_kib += (w*h) / 1024;
		}

		unsigned long const end_ms = now_ms();

		printf("-> %ld MiB/sec\n",
		       (transferred_kib)/(end_ms - start_ms));
	}

	/*
	 * Unaligned blitting via the blit library from RAM to framebuffer
	 */
	printf("unaligned copy via blit library from RAM to framebuffer...\n");
	{
		unsigned long transferred_kib = 0;
		unsigned long const start_ms = now_ms();

		/* line width in bytes */
		unsigned const w = fb_mode.width() * fb_mode.bytes_per_pixel();
		unsigned const h = fb_mode.height();

		for (unsigned i = 0; now_ms() - start_ms < duration_ms; i++) {
			blit(src_buf[i % 2] + 2, w, fb_ds.local_addr<char>() + 2, w, w - 2, h);

			transferred_kib += (w*h) / 1024;
		}

		unsigned long const end_ms = now_ms();

		printf("-> %ld MiB/sec\n",
		       (transferred_kib)/(end_ms - start_ms));
	}

	printf("--- test-fb_bench finished ---\n");
	return 0;
}
