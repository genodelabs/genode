/*
 * \brief  Marshalling of mbox messages for framebuffer channel
 * \author Norman Feske
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FRAMEBUFFER_MESSAGE_H_
#define _FRAMEBUFFER_MESSAGE_H_

/* Genode includes */
#include <util/misc_math.h>
#include <base/printf.h>

/* board-specific includes */
#include <drivers/board_base.h>
#include <platform/framebuffer_info.h>

namespace Platform { struct Framebuffer_message; }


/**
 * Mailbox message buffer for the framebuffer channel
 */
struct Platform::Framebuffer_message : Framebuffer_info
{
	Framebuffer_message(Framebuffer_info const &info) : Framebuffer_info(info) { }

	void finalize() { }

	static unsigned channel() { return 1; }

	static Genode::Board_base::Videocore_cache_policy cache_policy()
	{
		return Genode::Board_base::COHERENT;
	}

	void dump(char const *label)
	{
		using Genode::printf;

		printf("%s message:\n", label);
		printf(" phys_width:  %u\n",     phys_width);
		printf(" phys_height: %u\n",     phys_height);
		printf(" virt_width:  %u\n",     virt_width);
		printf(" virt_height: %u\n",     virt_height);
		printf(" pitch:       %u\n",     pitch);
		printf(" depth:       %d\n",     depth);
		printf(" x_offset:    %d\n",     x_offset);
		printf(" y_offset:    %d\n",     y_offset);
		printf(" addr:        0x%08x\n", addr);
		printf(" size:        0x%08x\n", size);
	}

	inline void *operator new (__SIZE_TYPE__, void *ptr) { return ptr; }
};

#endif /* _FRAMEBUFFER_MESSAGE_H_ */
