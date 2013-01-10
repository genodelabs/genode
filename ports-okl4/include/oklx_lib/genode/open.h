/*
 * \brief  Genode C API file functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__GENODE__OPEN_H_
#define _INCLUDE__OKLINUX_SUPPORT__GENODE__OPEN_H_

/**
 * Opens a file and attach it (read-only) to the address-space
 *
 * \param name name of the file
 * \param sz   resulting pointer to the size of the opened file
 * \return     virtual address of the attached file
 */
void *genode_open(const char *name, unsigned long *sz);

#endif //_INCLUDE__OKLINUX_SUPPORT__GENODE__OPEN_H_
