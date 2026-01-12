/*
 * \brief  Shadows Linux kernel asm/page_64.h
 * \author Alexander Boettcher
 * \date   2022-02-08
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#ifndef __ASM_PAGE_64_H
#define __ASM_PAGE_64_H

#define clear_page(page)	__builtin_memset((page), 0, PAGE_SIZE)
void copy_page(void *to, void *from);

#include <linux/version.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(6,14,0)
#define task_size_max() ((1UL << __VIRTUAL_MASK_SHIFT) - PAGE_SIZE)
#endif

#endif /* __ASM_PAGE_64_H */

