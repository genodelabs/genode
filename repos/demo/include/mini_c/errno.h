/*
 * \brief  Mini C errno
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__MINI_C__ERRNO_H_
#define _INCLUDE__MINI_C__ERRNO_H_

static int errno __attribute__ ((used)) = 0;

enum { EINTR = 4 };

#endif /* _INCLUDE__MINI_C__ERRNO_H_ */
