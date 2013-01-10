/*
 * \brief  Genode C API print functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__GENODE__PRINT_H_
#define _INCLUDE__OKLINUX_SUPPORT__GENODE__PRINT_H_

/**
 * Prints a message via Genode's log session mechanism
 */
void genode_printf(const char *format, ...);

#endif //_INCLUDE__OKLINUX_SUPPORT__GENODE__PRINT_H_
