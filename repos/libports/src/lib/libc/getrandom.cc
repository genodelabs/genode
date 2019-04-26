/*
 * \brief  C-library back end
 * \author Emery Hemingway
 * \date   2019-05-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
extern "C" {
#include <sys/random.h>
}

/* Genode includes */
#include <trace/timestamp.h>
#include <base/log.h>
#include <util/string.h>

extern "C" ssize_t __attribute__((weak))
getrandom(void *buf, size_t buflen, unsigned int flags)
{
	static Genode::Trace::Timestamp ts { };
	ts ^= Genode::Trace::Timestamp();
	Genode::memcpy(buf, &ts, Genode::min(sizeof(ts), buflen));

	Genode::warning(__func__, " NOT IMPLEMENTED!");
	return buflen;
}
