/*
 * \brief  Interface real libc
 * \author Sebastian Sumpf
 * \date   2023-06-12
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMPAT_LIBC_H_
#define _COMPAT_LIBC_H_

extern "C" int libc_stat(const char *, struct stat*);
extern "C" int libc_fstat(int, struct stat *);

#endif /* _COMPAT_LIBC_H_ */
