/*
 * \brief  Sysctl facade
 * \author Emery Hemingway
 * \date   2016-04-27
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>
#include <base/env.h>

/* Genode-specific libc interfaces */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* Libc includes */
#include <sys/sysctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "libc_errno.h"
#include "libc_init.h"


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
	case _SC_NPROCESSORS_CONF: return 1;
	case _SC_NPROCESSORS_ONLN: return 1;
	case _SC_PAGESIZE:         return PAGESIZE;

	case _SC_PHYS_PAGES:
		return _global_env->ram().ram_quota().value / PAGESIZE;
	default:
		Genode::warning(__func__, "(", name, ") not implemented");
		return Libc::Errno(EINVAL);
	}
}


/* non-standard FreeBSD function not supported */
extern "C" int sysctlbyname(char const *name, void *oldp, size_t *oldlenp,
                            void *newp, size_t newlen)
{
	Genode::warning(__func__, "(", name, ",...) not implemented");
	return Libc::Errno(ENOENT);
}


extern "C" int __sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
                        void *newp, size_t newlen)
{
	static const ctlname ctl_names[] = CTL_NAMES;
	static const ctlname ctl_kern_names[] = CTL_KERN_NAMES;
	static const ctlname ctl_hw_names[] = CTL_HW_NAMES;
	static const ctlname ctl_user_names[] = CTL_USER_NAMES;
	static const ctlname ctl_p1003_1b_names[] = CTL_P1003_1B_NAMES;

	/* read only */
	if (!oldp) /* check for write attempt */
		return Libc::Errno(newp ? EPERM : EINVAL);

	ctlname const *ctl = nullptr;
	char *buf = (char*)oldp;
	int index_a = name[0];
	int index_b = name[1];

	if (namelen != 2) return Libc::Errno(ENOENT);
	if (index_a >= CTL_MAXID) return Libc::Errno(EINVAL);

	switch(index_a) {
	case CTL_KERN:
		if (index_b >= KERN_MAXID) return Libc::Errno(EINVAL);
		ctl = &ctl_kern_names[index_b]; break;

	case CTL_HW:
		if (index_b >= HW_MAXID) return Libc::Errno(EINVAL);
		ctl = &ctl_hw_names[index_b]; break;

	case CTL_USER:
		if (index_b >= USER_MAXID) return Libc::Errno(EINVAL);
		ctl = &ctl_user_names[index_b]; break;

	case CTL_P1003_1B:
		if (index_b >= CTL_P1003_1B_MAXID) return Libc::Errno(EINVAL);
		ctl = &ctl_p1003_1b_names[index_b]; break;
	}

	if (!ctl) return Libc::Errno(EINVAL);

	if (((ctl->ctl_type == CTLTYPE_INT)    && (*oldlenp < sizeof(int))) ||
	    ((ctl->ctl_type == CTLTYPE_STRING) && (*oldlenp < 1)) ||
	    ((ctl->ctl_type == CTLTYPE_QUAD)   && (*oldlenp < sizeof(Genode::uint64_t))) ||
	    ((ctl->ctl_type == CTLTYPE_UINT)   && (*oldlenp < sizeof(unsigned int))) ||
	    ((ctl->ctl_type == CTLTYPE_LONG)   && (*oldlenp < sizeof(long))) ||
	    ((ctl->ctl_type == CTLTYPE_ULONG)  && (*oldlenp < sizeof(unsigned long))))
	{
		return Libc::Errno(EINVAL);
	}

	/* builtins */
	{
		switch(index_a) {
		case CTL_HW: switch(index_b) {

			case HW_PHYSMEM:
			case HW_USERMEM:
				*(unsigned long*)oldp = _global_env->ram().ram_quota().value;
				*oldlenp = sizeof(unsigned long);
				return 0;

			case HW_PAGESIZE:
				*(int*)oldp = (int)PAGESIZE;
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


	/* runtime overrides */
	{
		Libc::Absolute_path sysctl_path(ctl_names[index_a].ctl_name, "/.sysctl/");
		sysctl_path.append("/");
		sysctl_path.append(ctl->ctl_name);

		/*
		 * read from /.sysctl/...
		 *
		 * The abstracted libc interface is used to read files here
		 * rather than to explicity resolve a file system plugin.
		 */
		int fd = open(sysctl_path.base(), 0);
		if (fd != -1) {
			auto n = read(fd, buf, *oldlenp);
			close(fd);

			if (n > 0) switch (ctl->ctl_type) {
			case CTLTYPE_INT: {
				long value = 0;
				Genode::ascii_to((char*)oldp, value);
				*(int*)oldp = int(value);
				*oldlenp = sizeof(int);
				return 0;
			}

			case CTLTYPE_STRING:
				*oldlenp = n;
				return 0;

			case CTLTYPE_QUAD: {
				Genode::uint64_t value = 0;
				Genode::ascii_to((char*)oldp, value);
				*(Genode::uint64_t*)oldp = value;
				*oldlenp = sizeof(Genode::uint64_t);
				return 0;
			}

			case CTLTYPE_UINT: {
				unsigned value = 0;
				Genode::ascii_to((char*)oldp, value);
				*(unsigned*)oldp = value;
				*oldlenp = sizeof(unsigned);
				return 0;
			}

			case CTLTYPE_LONG: {
				long value = 0;
				Genode::ascii_to((char*)oldp, value);
				*(long*)oldp = value;
				*oldlenp = sizeof(long);
				return 0;
			}

			case CTLTYPE_ULONG: {
				unsigned long value = 0;
				Genode::ascii_to((char*)oldp, value);
				*(unsigned long*)oldp = value;
				*oldlenp = sizeof(unsigned long);
				return 0;
			}

			default:
				Genode::warning("unhandled sysctl data type for ", sysctl_path);
				return Libc::Errno(EINVAL);
			}
		}
	}


	/* fallback values */
	{
		switch(index_a) {

		case CTL_KERN:
			switch(index_b) {
			case KERN_OSTYPE:
				Genode::strncpy(buf, "Genode", *oldlenp);
				*oldlenp = Genode::strlen(buf);
				return 0;

			case KERN_OSRELEASE:
			case KERN_OSREV:
			case KERN_VERSION:
				*oldlenp = 0;
				return 0;

			case KERN_HOSTNAME:
				Genode::strncpy(buf, "localhost", *oldlenp);
				*oldlenp = Genode::strlen(buf);
				return 0;

			} break;

		case CTL_HW: switch(index_b) {

			case HW_MACHINE:
				*oldlenp = 0;
				return 0;

			case HW_NCPU:
				*(int*)oldp = 1;
				*oldlenp = sizeof(int);
				return 0;

			} break;

		}
	}

	Genode::warning("sysctl: no builtin or override value found for ",
	                Genode::Cstring(ctl_names[index_a].ctl_name), ".",
	                Genode::Cstring(ctl->ctl_name));
	return Libc::Errno(ENOENT);
}
