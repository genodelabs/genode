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

			enum { ENV_DS_SIZE = 4096 };

			char *_env;

			Pwd::Path _pwd_path;

		public:

			/**
			 * \param env  comma-separated list of environment variables
			 */
			Environment(char const *env) :
				Attached_ram_dataspace(Genode::env()->ram_session(), ENV_DS_SIZE),
				_env(local_addr<char>())
			{
				strncpy(_env, env, ENV_DS_SIZE);
			}

			using Attached_ram_dataspace::cap;

			/**
			 * Return list of environment variables as comma-separated list
			 */
			char const *env() { return _env; }


			/*******************
			 ** Pwd interface **
			 *******************/

			char const *pwd() { return _pwd_path.base(); }

			void pwd(char const *pwd)
			{
				_pwd_path.import(pwd);
				_pwd_path.remove_trailing('/');

				char quoted[Sysio::MAX_PATH_LEN];
				Range_checked_index<unsigned> i(0, sizeof(quoted));

				try {
					char const *s = _pwd_path.base();
					quoted[i++] = '"';
					while (*s) {
						if (*s == '"')
							quoted[i++] = '/';
						quoted[i++] = *s++;
					}
					quoted[i++] = '"';
					quoted[i] = 0;
				} catch (Index_out_of_range) {
					PERR("Could not set PWD, buffer too small");
					return;
				}

				Arg_string::set_arg(_env, ENV_DS_SIZE, "PWD", quoted);
				PINF("changed current work directory to %s", _pwd_path.base());
			}
	};
}
