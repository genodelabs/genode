/*
 * \brief  Replaces kernel/printk/printk.c
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
#include <linux/stdarg.h>
#endif
#include <linux/kernel.h>
#include <lx_emul/log.h>
#include <lx_emul/debug.h>


static char print_string[2048];

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
asmlinkage __visible int printk(const char * fmt,...)
#else
asmlinkage __visible int _printk(const char * fmt,...)
#endif
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(print_string, sizeof(print_string), fmt, args);
	va_end(args);

	lx_emul_print_string(print_string);
	return 0;
}


asmlinkage int vprintk(const char * fmt, va_list args)
{
	vsnprintf(print_string, sizeof(print_string), fmt, args);
	lx_emul_print_string(print_string);
	return 0;
}


asmlinkage int vprintk_emit(int facility, int level,
                            const struct dev_printk_info *dev_info,
                            const char * fmt, va_list args)
{
	vsnprintf(print_string, sizeof(print_string), fmt, args);
	lx_emul_print_string(print_string);
	return 0;
}


void lx_emul_trace_msg(char const *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(print_string, sizeof(print_string), fmt, args);
	va_end(args);

	lx_emul_trace(print_string);
}
