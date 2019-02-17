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
#include <base/log.h>

/* board-specific includes */
#include <drivers/defs/rpi.h>
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

	static Rpi::Videocore_cache_policy cache_policy() {
		return Rpi::COHERENT;
	}

	void dump(char const *label)
	{
		using Genode::log;

		log(label, " message:");
		log(" phys_width:  ", phys_width);
		log(" phys_height: ", phys_height);
		log(" virt_width:  ", virt_width);
		log(" virt_height: ", virt_height);
		log(" pitch:       ", pitch);
		log(" depth:       ", depth);
		log(" x_offset:    ", x_offset);
		log(" y_offset:    ", y_offset);
		log(" addr:        ", Genode::Hex(addr));
		log(" size:        ", Genode::Hex(size));
	}

	inline void *operator new (__SIZE_TYPE__, void *ptr) { return ptr; }
};

#endif /* _FRAMEBUFFER_MESSAGE_H_ */
