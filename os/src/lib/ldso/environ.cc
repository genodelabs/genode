/*
 * \brief  Dummy 'lx_environ' used on on-Linux platforms
 * \author Norman Feske
 * \date   2011-08-23
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * The 'lx_environ' pointer is supposed to be initialized by the startup code
 * on Linux. It is specially treated in 'main.c'. On platforms w/o 'lx_environ',
 * we resolve the symbol by the weak symbol defined here. On Linux, the
 * weak symbol is overridden by the version in the startup library.
 *
 * XXX: We should better remove the Linux-specific code from ldso's 'main.c'.
 */
__attribute__((weak)) char **lx_environ = (char **)0;
