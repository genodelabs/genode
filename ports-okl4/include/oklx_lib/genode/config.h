/*
 * \brief  Genode C API configuration related functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-07-08
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__GENODE__CONFIG_H_
#define _INCLUDE__OKLINUX_SUPPORT__GENODE__CONFIG_H_

/**
 * Get the kernel command line
 */
char* genode_config_cmdline(void);

/**
 * Get name of initrd file
 */
char* genode_config_initrd(void);

/**
 * Returns whether audio driver should be used, or not.
 */
int genode_config_audio(void);

/**
 * Returns whether nic driver should be used, or not.
 */
int genode_config_nic(void);

/**
 * Returns whether block driver should be used, or not.
 */
int genode_config_block(void);

#endif //_INCLUDE__OKLINUX_SUPPORT__GENODE__CONFIG_H_
