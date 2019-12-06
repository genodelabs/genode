/*
 * \brief  lwIP platform support
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2016-12-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <util/reconstructible.h>
#include <base/sleep.h>

#include <lwip/genode_init.h>

extern "C" {
/* LwIP includes */
#include <lwip/timeouts.h>
#include <lwip/init.h>
#include <lwip/sys.h>
#include <arch/cc.h>

/* our abridged copy of string.h */
#include <string.h>
}

namespace Lwip {

	static Genode::Allocator *_heap;

	struct Sys_timer
	{
		void check_timeouts(Genode::Duration)
		{
			Genode::Lock::Guard g{ Lwip::lock() };
			sys_check_timeouts();
		}

		Genode::Timeout_scheduler &timer;

		Timer::Periodic_timeout<Sys_timer> timeout {
			timer, *this, &Sys_timer::check_timeouts,
			Genode::Microseconds{250*1000} };

		Sys_timer(Genode::Timeout_scheduler &timer) : timer(timer) { }
	};

	static Sys_timer *sys_timer_ptr;

	void genode_init(Genode::Allocator &heap, Genode::Timeout_scheduler &timer)
	{
		LWIP_ASSERT("LwIP initialized with an allocator that does not track sizes",
		            !heap.need_size_for_free());

		_heap = &heap;

		static Sys_timer sys_timer(timer);
		sys_timer_ptr = &sys_timer;

		lwip_init();
	}

	Genode::Lock &lock()
	{
		static Genode::Lock _lwip_lock;
		return _lwip_lock;
	}
}


extern "C" {

	void lwip_platform_assert(char const* msg, char const *file, int line)
	{
		Genode::error("Assertion \"", msg, "\" ", file, ":", line);
		Genode::sleep_forever();
	}

	void genode_free(void *ptr)
	{
		Lwip::_heap->free(ptr, 0);
	}

	void *genode_malloc(unsigned long size)
	{
		void *ptr = nullptr;
		return Lwip::_heap->alloc(size, &ptr) ? ptr : 0;
	}

	void *genode_calloc(unsigned long number, unsigned long size)
	{
		void *ptr = nullptr;
		size *= number;
		if (Lwip::_heap->alloc(size, &ptr)) {
			Genode::memset(ptr, 0x00, size);
			return ptr;
		}
		return nullptr;
	}

	u32_t sys_now(void) {
		return Lwip::sys_timer_ptr->timer.curr_time().trunc_to_plain_ms().value; }

	void genode_memcpy(void *dst, const void *src, size_t len) {
		Genode::memcpy(dst, src, len); }

	void *genode_memmove(void *dst, const void *src, size_t len) {
		return Genode::memmove(dst, src, len); }

	int memcmp(const void *b1, const void *b2, ::size_t len) {
		return Genode::memcmp(b1, b2, len); }

	int strcmp(const char *s1, const char *s2)
	{
		size_t len = Genode::min(Genode::strlen(s1), Genode::strlen(s2));
		return Genode::strcmp(s1, s2, len);
	}

	int strncmp(const char *s1, const char *s2, size_t len) {
		return Genode::strcmp(s1, s2, len); }

}
