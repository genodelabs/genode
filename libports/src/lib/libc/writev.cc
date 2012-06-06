/*
 * \brief  Plugin implementation
 * \author Josef Soentgen 
 * \date   2012-04-10
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#include <sys/uio.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>

enum { MAX_BUFFER_LEN = 2048 };

extern "C" int _writev(int d, const struct iovec *iov, int iovcnt)
{
	char buffer[MAX_BUFFER_LEN];
	char *v, *b;
	ssize_t written;
	size_t v_len, _len;
	int i;

	if (iovcnt < 1 || iovcnt > IOV_MAX) {
		return -EINVAL;
	}

	for (i = 0; i < iovcnt; i++)
		v_len += iov->iov_len;

	if (v_len > SSIZE_MAX) {
		return -EINVAL;
	}

	b = buffer;
	while (iovcnt) {
		v = static_cast<char *>(iov->iov_base);
		v_len = iov->iov_len;

		while (v_len > 0) {
			if (v_len < sizeof(buffer))
				_len = v_len;
			else
				_len = sizeof(buffer);

			// TODO error check
			memmove(b, v, _len);
			i = write(d, b, _len);
			v += _len;
			v_len -= _len;

			written += i;
		}

		iov++;
		iovcnt--;
	}

        return written;
}

extern "C" ssize_t writev(int d, const struct iovec *iov, int iovcnt)
{
	return _writev(d, iov, iovcnt);
}

