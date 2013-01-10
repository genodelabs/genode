/*
 * \brief  Unoffical dlfnc.h extensions
 * \author Sebastian.Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-05
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DL_EXTENSIONS_
#define _DL_EXTENSIONS_

/* For a given PC, find the .so that it belongs to.
 * Returns the base address of the .ARM.exidx section
 * for that .so, and the number of 8-byte entries
 * in that section (via *pcount).
 * Intended to be called by libc's __gnu_Unwind_Find_exidx().
 * 
 * 'Quoted from Android'
 */

#ifdef __ARM_EABI__
unsigned long dl_unwind_find_exidx(unsigned long pc, int *pcount);
#endif

#endif /* _DL_EXTENSIONS_ */
