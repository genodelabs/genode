/**
 * \brief  Shared-object support code
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2009-08-14
 *
 * Support code comprises hooks for execution of static constructors and
 * ARM-EABI dynamic linking.
 *
 * The ARM cross compiler uses the __gnu_Unwind_Find_exidx hook to locate a
 * 'ARM.exidx' section within a shared object. For this to work
 * 'dl_unwind_find_exidx' is excuted by 'ldso', which returns the section
 * address if it finds a shared object within the range of the provieded
 * program counter.
 */

/*
 * Copyright (C) 2009-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#define BEG { (ld_hook) ~1U }
#define END { (ld_hook) 0   }
#define SECTION(x) __attribute__((used,section( x )))

typedef void (*ld_hook)(void);
static ld_hook _lctors_start[1] SECTION("_mark_ctors_start") = BEG;
static ld_hook _lctors_end[1]   SECTION("_mark_ctors_end")   = END;

/*
 * '__dso_handle' needs to be defined in the main program and in each shared
 * object. Because ld.lib.so is both of them, '__dso_handle' is weak here.
 */
void *__dso_handle __attribute__((__visibility__("hidden")))
                   __attribute__((weak))                     = &__dso_handle;

/* called by dynamic linker on library startup (ld.lib.so) */
extern void _init(void) __attribute__((used,section(".init")));
extern void _init(void)
{
	/* call static constructors */
	for (ld_hook *func = _lctors_end; func > _lctors_start + 1; (*--func)());
}


/*
 * from gcc/config/arm/unwind-arm.h
 */
typedef unsigned _Unwind_Ptr __attribute__((__mode__(__pointer__)));


/*
 * Dummy for static libs, implemented in ldso for dynamic case
 */
extern _Unwind_Ptr dl_unwind_find_exidx(_Unwind_Ptr pc, int *pcount) __attribute__((weak));
extern _Unwind_Ptr dl_unwind_find_exidx(_Unwind_Ptr pc, int *pcount)
{
	return 0;
}


/*
 * Called from libgcc_eh.a file 'gcc/config/arm/unwind-arm.c' in function
 * 'get_eit_entry'
 */
extern _Unwind_Ptr __gnu_Unwind_Find_exidx (_Unwind_Ptr pc, int *pcount)
{
	return dl_unwind_find_exidx(pc, pcount);
}
