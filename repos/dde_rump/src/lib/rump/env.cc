/**
 * \brief  Rump::Env initialization
 * \author Sebastian Sumpf
 * \date   2016-06-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <rump/env.h>

static Rump::Env *_env_ptr;


Rump::Env &Rump::env()
{
	return *_env_ptr;
}


void Rump::construct_env(Genode::Env &env)
{
	static Rump::Env _env(env);
	_env_ptr = &_env;
}


/* constructors in rump.lib.so */
extern "C" void rumpns_modctor_ksem(void);
extern "C" void rumpns_modctor_suser(void);

Rump::Env::Env(Genode::Env &env) : _env(env)
{
	/* call init/constructor functions of rump.lib.so */
	rumpns_modctor_ksem();
	rumpns_modctor_suser();
}
