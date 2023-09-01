/*
 * \brief  This file shadows linux/utsname.h
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SHADOW__LINUX__UTSNAME_H_
#define _SHADOW__LINUX__UTSNAME_H_

#include <uapi/linux/utsname.h>

extern struct new_utsname init_uts_ns;

static inline struct new_utsname *init_utsname(void)
{
	return &init_uts_ns;
}


static inline struct new_utsname *utsname(void)
{
	return init_utsname();
}

#endif /* _SHADOW__LINUX__UTSNAME_H_ */
