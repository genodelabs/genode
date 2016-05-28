/*
 * \brief  C++ wrapper over errno
 * \author Christian Helmuth
 * \date   2016-04-26
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC_ERRNO_H_
#define _LIBC_ERRNO_H_

/* libc includes */
#include <errno.h>

namespace Libc { struct Errno; }

struct Libc::Errno
{
    int const error;

    explicit Errno(int error) : error(error) { errno = error; }

    operator int() const { return -1; }
};

#endif /* _LIBC_ERRNO_H_ */
