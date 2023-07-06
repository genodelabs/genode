/*
 * \brief  Lx_kit environment
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>


static Lx_kit::Env *_env_ptr;


void Lx_kit::Env::initialize(Genode::Env & env,
                             Genode::Signal_context & sig_ctx)
{
	static Lx_kit::Env environment(env, sig_ctx);
	_env_ptr = &environment;
}


Lx_kit::Env & Lx_kit::env()
{
	if (!_env_ptr) {
		Genode::error("Lx_kit::Env not initialized");
		struct Env_not_initialized { };
		throw Env_not_initialized();
	}

	return *_env_ptr;
}


void Lx_kit::Env::submit_signal()
{
	_signal_dispatcher.local_submit();
}
