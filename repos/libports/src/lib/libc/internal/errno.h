/*
 * \brief  C++ wrapper over errno
 * \author Christian Helmuth
 * \date   2016-04-26
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__ERRNO_H_
#define _LIBC__INTERNAL__ERRNO_H_

/* libc includes */
#include <errno.h>

namespace Libc { struct Errno; }

struct Libc::Errno
{
    explicit Errno(int error) { errno = error; }

    operator int() const { return -1; }
};

#endif /* _LIBC__INTERNAL__ERRNO_H_ */
