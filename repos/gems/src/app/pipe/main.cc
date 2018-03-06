/*
 * \brief  Standalone POSIX pipe
 * \author Emery Hemingway
 * \date   2018-03-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* Libc includes */
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main()
{
	enum { SIXTEEN_K = 1 << 14 };
	static char buf[SIXTEEN_K];

	Genode::uint64_t total = 0;

	while (true) {

		auto const nr = fread(buf, 1, sizeof(buf), stdin);
		if (nr == 0 && feof(stdin))
			break;

		if (nr < 1 || nr > sizeof(buf)) {
			int res = errno;
			Genode::error((char const *)strerror(res));
			return res;
		}

		auto remain = nr;
		auto off = 0;
		while (remain > 0) {
			auto const nw = fwrite(buf+off, 1, remain, stdout);
			if (nw < 1 || nw > remain) {
				int res = errno;
				Genode::error((char const *)strerror(res));
				return res;
			}

			remain -= nw;
			off += nw;
			total += nw;
		}
	}

	Genode::log("piped ", total, " bytes");
	return 0;
};
