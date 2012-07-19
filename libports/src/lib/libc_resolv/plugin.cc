/*
 * \brief  Libc resolv
 * \author Josef Soentgen
 * \date   2012-07-19
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>

/* libc plugin includes */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


void *operator new (size_t, void *ptr) { return ptr; }

extern "C" void libc_freeaddrinfo(struct ::addrinfo *);
extern "C" int libc_getaddrinfo(const char *, const char *,
                                const struct ::addrinfo *,
                                struct ::addrinfo **);

/************
 ** Plugin **
 ************/

namespace {
	
	struct Plugin_context : Libc::Plugin_context { };

	static inline Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}

	class Plugin : public Libc::Plugin
	{
		private:

			Plugin_context _context;

		public:

			/**
			 * Constructor
			 */
			Plugin() { }

			bool supports_freeaddrinfo(struct ::addrinfo *res)
			{
				return true;
			}
			bool supports_getaddrinfo(const char *node, const char *service,
			                          const struct ::addrinfo *hints,
			                          struct ::addrinfo **res)
			{
				return true;
			}

			int getaddrinfo(const char *node, const char *service,
			                const struct ::addrinfo *hints,
			                struct ::addrinfo **res)
			{
				PDBG("libc_resolv getaddrinfo() called");
				return ::libc_getaddrinfo(node, service, hints, res);
			}

			void freeaddrinfo(struct ::addrinfo *res)
			{
				PDBG("libc_resolv freeaddrinfo() called");

				return ::libc_freeaddrinfo(res);
			}

	};

} /* unnamed namespace */

void __attribute__((constructor)) init_libc_resolv(void)
{
	static Plugin libc_resolv;
}
