/*
 * \brief  Process environment utility
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/string.h>
#include <os/attached_ram_dataspace.h>
#include <base/printf.h>

/* Noux includes */
#include <path.h>

namespace Noux {

	class Environment : private Attached_ram_dataspace
	{
		private:

			Sysio::Env *_env;

		public:

			/**
			 * \param env  comma-separated list of environment variables
			 */
			Environment(Sysio::Env const &env) :
				Attached_ram_dataspace(Genode::env()->ram_session(), sizeof(Sysio::Env)),
				_env(local_addr<Sysio::Env>())
			{
				memcpy(_env, env, sizeof(Sysio::Env));
			}

			using Attached_ram_dataspace::cap;

			/**
			 * Return list of environment variables as zero-separated list
			 */
			Sysio::Env const &env() { return *_env; }
	};
}
