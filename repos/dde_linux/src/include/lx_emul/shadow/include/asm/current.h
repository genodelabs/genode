/*
 * \brief  Shadows Linux kernel asm/current.h
 * \author Stefan Kalkowski
 * \date   2021-04-14
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__SHADOW__ASM__CURRENT_H_
#define _LX_EMUL__SHADOW__ASM__CURRENT_H_

#include <lx_emul/task.h>

#ifndef __ASSEMBLY__

#define current lx_emul_task_get_current()

#endif /* __ASSEMBLY__ */

#endif /* _LX_EMUL__SHADOW__ASM__CURRENT_H_ */
