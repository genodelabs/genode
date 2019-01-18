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


class Noux::Environment : Noncopyable
{
	private:

		Attached_ram_dataspace _ds;

		Sysio::Env &_env { *_ds.local_addr<Sysio::Env>() };

	public:

		/**
		 * Constructor
		 *
		 * \param env  comma-separated list of environment variables
		 */
		Environment(Ram_allocator &ram, Region_map &local_rm, Sysio::Env const &env)
		: _ds(ram, local_rm, sizeof(Sysio::Env))
		{
			memcpy(&_env, &env, sizeof(Sysio::Env));
		}

		Dataspace_capability cap() { return _ds.cap(); }

		/**
		 * Return list of environment variables as zero-separated list
		 */
		Sysio::Env const &env() { return _env; }
};

#endif /* _NOUX__ENVIRONMENT_H_ */
