/*
 * \brief  Connection to PD service
 * \author Norman Feske
 * \date   2012-11-21
 *
 * In contrast to the generic version of 'pd_session/connection.h', the
 * Linux-specific version supplies additional argument to core's PD service:
 *
 * :'root': is the path of a chroot environment of the process
 * :'uid':  is the user ID of the process
 * :'gid':  is the designated group ID of the process
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PD_SESSION__CONNECTION_H_
#define _INCLUDE__PD_SESSION__CONNECTION_H_

#include <pd_session/client.h>
#include <base/connection.h>

namespace Genode {

	class Pd_connection : public Connection<Pd_session>, public Pd_session_client
	{
		private:

			template <Genode::size_t STRING_MAX_LEN>
			struct Arg
			{
				char string[STRING_MAX_LEN];

				Arg() { string[0] = 0; }
			};

			/**
			 * Convert root path argument to session-construction parameter
			 */
			struct Root_arg : Arg<Native_pd_args::ROOT_PATH_MAX_LEN>
			{
				Root_arg(Native_pd_args const *args)
				{
					if (args && args->root() && args->root()[0])
						Genode::snprintf(string, sizeof(string), ", root=\"%s\"",
						                 args->root());
				}
			};

			/**
			 * Convert UID argument to session-construction parameter
			 */
			struct Uid_arg : Arg<64>
			{
				Uid_arg(Native_pd_args const *args)
				{
					if (args && args->uid())
						Genode::snprintf(string, sizeof(string), ", uid=%u",
						                 args->uid());
				}
			};

			/**
			 * Convert GID argument to session-construction parameter
			 */
			struct Gid_arg : Arg<64>
			{
				Gid_arg(Native_pd_args const *args)
				{
					if (args && args->gid())
						Genode::snprintf(string, sizeof(string), ", gid=%u",
						                 args->gid());
				}
			};

		public:

			enum { RAM_QUOTA = 4*1024 };

			/**
			 * Constructor
			 *
			 * \param label    session label
			 * \param pd_args  Linux-specific PD-session arguments
			 */
			Pd_connection(char const *label = "", Native_pd_args const *pd_args = 0)
			:
				Connection<Pd_session>(
					session("ram_quota=4K, label=\"%s\"%s%s%s", label,
					        Root_arg(pd_args).string,
					        Uid_arg(pd_args).string,
					        Gid_arg(pd_args).string)),
				Pd_session_client(cap())
			{ }
	};
}

#endif /* _INCLUDE__PD_SESSION__CONNECTION_H_ */
