/*
 * \brief  ldso dummy function implementations
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "rtld.h"
#include "rtld_machdep.h"

inline int 
lm_init (char *libmap_override)
{
	return 0;
}

inline void 
lm_fini (void)
{
}

inline char *
lm_find (const char *p, const char *f)
{
	return NULL;
}


inline void
_rtld_thread_init(struct RtldLockInfo *pli)
{
}

inline void
_rtld_atfork_pre(int *locks)
{
}

inline void
_rtld_atfork_post(int *locks)
{
}

inline void
lockdflt_init()
{
}

