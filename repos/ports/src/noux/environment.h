/*
 * \brief  Process environment utility
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__ENVIRONMENT_H_
#define _NOUX__ENVIRONMENT_H_

/* Genode includes */
#include <util/string.h>
#include <base/attached_ram_dataspace.h>

/* Noux includes */
#include <path.h>

namespace Noux {
	class Environment;
	using namespace Genode;
}


class Noux::Environment : private Attached_ram_dataspace
{
	private:

		Sysio::Env *_env;

	public:

		/**
		 * Constructor
		 *
		 * \param env  comma-separated list of environment variables
		 */
		Environment(Ram_session &ram, Region_map &local_rm, Sysio::Env const &env)
		:
			Attached_ram_dataspace(ram, local_rm, sizeof(Sysio::Env)),
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

#endif /* _NOUX__ENVIRONMENT_H_ */
