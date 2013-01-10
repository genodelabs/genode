/*
 * \brief  sysarch.h dummies required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MACHINE_SYSARCH_H_
#define _MACHINE_SYSARCH_H_

static inline
int i386_set_gsbase(void *addr) { return 0; }

inline void amd64_set_fsbase(void *p)
{}
#endif //_MACHINE_SYSARCH_H_

