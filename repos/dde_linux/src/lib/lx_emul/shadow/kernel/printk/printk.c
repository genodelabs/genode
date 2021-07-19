/*
 * \brief  Replaces kernel/printk/printk.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/printk.h>
#include <lx_emul/log.h>
#include <lx_emul/debug.h>

asmlinkage __visible int printk(const char * fmt,...)
{
	va_list args;
	va_start(args, fmt);
	lx_emul_vprintf(fmt, args);
	va_end(args);
	return 0;
}


asmlinkage int vprintk(const char * fmt, va_list args)
{
	lx_emul_vprintf(fmt, args);
	return 0;
}


asmlinkage int vprintk_emit(int facility, int level,
                            const struct dev_printk_info *dev_info,
                            const char * fmt, va_list args)
{
	lx_emul_vprintf(fmt, args);
	return 0;
}

