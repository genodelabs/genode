/*
 * \brief  Genode linkage defines needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-11-24
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GENODE__LINKAGE_H_
#define _INCLUDE__GENODE__LINKAGE_H_

#ifdef i386
#define FASTCALL __attribute__((regparm(3)))
#else
#define FASTCALL
#endif

#endif /* _INCLUDE__GENODE__LINKAGE_H_ */
