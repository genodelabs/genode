/*
 * \brief  C++ support for Microblaze cross compiler
 * \author Norman Feske
 * \date   2010-07-21
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * The mb-gcc generates calls to 'atexit' instead of '__cxa_atexit' as
 * usual.
 */
extern "C" __attribute__((weak))
void *atexit() { return 0; }
