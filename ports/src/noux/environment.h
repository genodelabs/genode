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
#include <util/arg_string.h>
#include <os/attached_ram_dataspace.h>
#include <base/printf.h>

/* Noux includes */
#include <path.h>
#include <pwd.h>
#include <range_checked_index.h>

namespace Noux {

	/**
	 * Front-end for PWD environment variable
	 */
	class Environment : private Attached_ram_dataspace, public Pwd
	{
		private:

			Sysio::Env *_env;

			Pwd::Path _pwd_path;

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


			/*******************
			 ** Pwd interface **
			 *******************/

			char const *pwd() { return _pwd_path.base(); }

			void pwd(char const *pwd)
			{
				_pwd_path.import(pwd);
				_pwd_path.remove_trailing('/');
				PINF("changed current work directory to %s", _pwd_path.base());
			}
	};
}
