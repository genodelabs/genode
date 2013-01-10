/*
 * \brief  kctrace.h dummies required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SYS_KTRACE_H_
#define _SYS_KTRACE_H_

static inline
int utrace(const void *addr, size_t len) { return 0;}

#endif //_SYS_KTRACE_H_
