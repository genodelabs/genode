/*
 * \brief  Our implementation of __gnu_Unwind_Find_exidx
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-05
 *
 * This file is used for ARM-EABI dynamic linking, only. The ARM cross-compiler
 * uses this hook to locate a 'ARM.exidx' section within a shared object. For
 * this to work 'dl_unwind_find_exidx' is excuted by 'ldso', which returns the
 * section address if it finds a shared object within the range of the provieded
 * progam counter
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

/*
 * from gcc/config/arm/unwind-arm.h
 */
typedef unsigned _Unwind_Ptr __attribute__((__mode__(__pointer__)));


/*
 * Implemented in ldso
 * */
extern "C" _Unwind_Ptr __attribute__((weak)) dl_unwind_find_exidx(_Unwind_Ptr pc, int *pcount);
extern "C" _Unwind_Ptr dl_unwind_find_exidx(_Unwind_Ptr pc, int *pcount)
{
	Genode::error("dl_unwind_find_exidx called");
	return 0;
}


/*
 * Called from libgcc_eh.a file 'gcc/config/arm/unwind-arm.c' in function
 * 'get_eit_entry'
 */
extern "C" _Unwind_Ptr __gnu_Unwind_Find_exidx (_Unwind_Ptr pc, int *pcount)
{
	return dl_unwind_find_exidx(pc, pcount);
}


