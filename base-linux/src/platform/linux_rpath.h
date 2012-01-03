/*
 * \brief  Linux resource path
 * \author Christian Helmuth
 * \date   2011-09-25
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM__LINUX_RPATH_H_
#define _PLATFORM__LINUX_RPATH_H_


/**
 * Return resource path for Genode
 *
 * Genode creates files for dataspaces and endpoints under in this directory.
 */
char const * lx_rpath();

#endif /* _PLATFORM__LINUX_RPATH_H_ */
