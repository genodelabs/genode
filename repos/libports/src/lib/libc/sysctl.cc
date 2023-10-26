/*
 * \brief  Sysctl facade
 * \author Emery Hemingway
 * \author Benjamin Lamowski
 * \date   2016-04-27
 */

/*
 * Copyright (C) 2016-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>
#include <base/env.h>

/* libc includes */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

/* libc-internal includes */
#include <internal/plugin.h>
#include <internal/fd_alloc.h>
#include <internal/errno.h>
#include <internal/init.h>

using namespace Libc;

enum { PAGESIZE = 4096 };


static Genode::Env *_global_env;


void Libc::sysctl_init(Genode::Env &env)
{
	_global_env = &env;
}


extern "C" long sysconf(int name)
{
	switch (name) {
	case _SC_CHILD_MAX:        return CHILD_MAX;
	case _SC_OPEN_MAX:         return getdtablesize();
	case _SC_PAGESIZE:         return PAGESIZE;
	case _SC_PHYS_PAGES:
		return _global_env->pd().ram_quota().value / PAGESIZE;
	case _SC_NPROCESSORS_CONF:
		[[fallthrough]];
	case _SC_NPROCESSORS_ONLN: {
		Affinity::Space space = _global_env->cpu().affinity_space();
		return space.total() ? : 1;
	}
	case _SC_GETPW_R_SIZE_MAX:
		return -1;
	default:
		warning(__func__, "(", name, ") not implemented");
		return Errno(EINVAL);
	}
}


extern "C" int __sysctl(const int *name, u_int namelen,
                        void *oldp, size_t *oldlenp,
                        const void *newp, size_t newlen)
{
	/* read only */
	if (!oldp) /* check for write attempt */
		return Errno(newp ? EPERM : EINVAL);

	if (namelen != 2)
		return Errno(ENOENT);

	/* reject special interface for sysctlnametomib() */
	if (name[0] == 0 && name[1] == 3)
		return Errno(ENOENT);

	char *buf = (char*)oldp;
	int index_a = name[0];
	int index_b = name[1];


	Genode::memset(buf, 0x00, *oldlenp);

	/* builtins */
	{
		switch(index_a) {
		case CTL_KERN: switch(index_b) {
			case KERN_ARND:
				return getentropy(oldp, *oldlenp);
		}
		case CTL_HW: switch(index_b) {

			case HW_REALMEM:
			case HW_PHYSMEM:
			case HW_USERMEM:
				switch (*oldlenp) {
				case 4:
					*(Genode::int32_t*)oldp = _global_env->pd().ram_quota().value;
					break;
				case 8:
					*(Genode::int64_t*)oldp = _global_env->pd().ram_quota().value;
					break;
				default:
					return Errno(EINVAL);
				}
				return 0;

			case HW_PAGESIZE:
				*(int*)oldp = (int)PAGESIZE;
				*oldlenp = sizeof(int);
				return 0;
			/*
			 * Used on ARM platforms to check HW fp support. Since the
			 * FP is enabled on all our ARM platforms we return true.
			 */
			case HW_FLOATINGPT:
				*(int*)oldp = 1;
				*oldlenp = sizeof(int);
				return 0;

			} break;

		case CTL_P1003_1B: switch(index_b) {

			case CTL_P1003_1B_PAGESIZE:
				*(int*)oldp = PAGESIZE;
				*oldlenp = sizeof(int);
				return 0;

			} break;
		}
	}

	/* fallback values */
	{
		switch(index_a) {

		case CTL_KERN:
			switch(index_b) {
			case KERN_OSTYPE:
				copy_cstring(buf, "Genode", *oldlenp);
				*oldlenp = Genode::strlen(buf);
				return 0;

			case KERN_OSRELEASE:
			case KERN_OSREV:
			case KERN_VERSION:
				*oldlenp = 0;
				return 0;

			case KERN_HOSTNAME:
				copy_cstring(buf, "localhost", *oldlenp);
				*oldlenp = Genode::strlen(buf);
				return 0;

			} break;

		case CTL_HW: switch(index_b) {

			case HW_MACHINE:
				*oldlenp = 0;
				return 0;

			case HW_NCPU:
				Affinity::Space space = _global_env->cpu().affinity_space();

				*(int*)oldp = space.total() ? : 1;
				*oldlenp = sizeof(int);
				return 0;

			} break;

		}
	}

	warning("missing sysctl for [", index_a, "][", index_b, "]");
	return Errno(ENOENT);
}
