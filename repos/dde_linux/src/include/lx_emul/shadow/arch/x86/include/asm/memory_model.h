/*
 * \brief  Shadows Linux kernel asm-generic/memory_model.h
 * \author Norman Feske
 * \date   2021-06-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_MEMORY_MODEL_H
#define __ASM_MEMORY_MODEL_H

#include <linux/pfn.h>

#ifndef __ASSEMBLY__

#define page_to_pfn(page) virt_to_pfn(page_to_virt(page))
#define pfn_to_page(pfn)  virt_to_page(pfn_to_virt(pfn))

#endif /* __ASSEMBLY__ */

#endif /* __ASM_MEMORY_MODEL_H */

