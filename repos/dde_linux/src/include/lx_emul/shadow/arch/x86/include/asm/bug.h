/**
 * \brief  Shadow asm/bug.h for x86
 * \author Sebastian Sumpf
 * \date   2024-06-27
 */

/*
 * This file shadows arch/x86/include/asm/bug.h to avoid HAVE_ARCH_BUG which
 * inserts undefined instructions (ud2) and use our own BUG() implementation.
 */

#ifndef _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__BUG_H_
#define _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__BUG_H_

#define HAVE_ARCH_BUG

#define BUG() do { \
	printk("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	while (1) { } \
} while (0);

#include <asm-generic/bug.h>

#endif /* _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__BUG_H_ */
