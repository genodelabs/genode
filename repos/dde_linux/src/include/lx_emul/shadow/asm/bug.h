/*
 * \brief  Shadows Linux kernel asm/bug.h
 * \author Stefan Kalkowski
 * \date   2021-04-14
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__SHADOW__ASM__BUG_H_
#define _LX_EMUL__SHADOW__ASM__BUG_H_

#include_next <asm/bug.h>

#undef  __WARN_FLAGS
#define __WARN_FLAGS(flags)

#endif /* _LX_EMUL__SHADOW__ASM__BUG_H_ */
