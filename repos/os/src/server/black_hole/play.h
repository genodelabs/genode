/*
 * \brief  Play service of the black hole component
 * \author Josef Soentgen
 * \date   2024-04-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLAY_SESSION_H_
#define _PLAY_SESSION_H_

#include <base/attached_ram_dataspace.h>
#include <play_session/play_session.h>
#include <root/component.h>

namespace Black_hole {

	using namespace Genode;

	class  Play_session;
	class  Play_root;
}


class Black_hole::Play_session : public Session_object<Play::Session, Play_session>
{
	private:

		Env                     &_env;
		Attached_ram_dataspace   _ram_ds;

	public:

		Play_session(Env             &env,
		             Resources const &resources,
		             Label     const &label,
		             Diag      const &diag)
		:
			Session_object(env.ep(), resources, label, diag),
			_env(env),
			_ram_ds(_env.ram(), _env.rm(), Play::Session::DATASPACE_SIZE)
		{ }

		/****************************
		 ** Play session interface **
		 ****************************/

		Dataspace_capability dataspace() { return _ram_ds.cap(); }

		Play::Time_window schedule(Play::Time_window,
		                           Play::Duration,
		                           Play::Num_samples)
		{
			return { };
		}

		void stop() { }
};


class Black_hole::Play_root : public Root_component<Play_session>
{
	private:

		Env &_env;

	protected:

		Play_session *_create_session(const char *args) override
		{
			if (session_resources_from_args(args).ram_quota.value < Play::Session::DATASPACE_SIZE)
				throw Insufficient_ram_quota();

			return new (md_alloc())
				Play_session(_env,
				             session_resources_from_args(args),
				             session_label_from_args(args),
				             session_diag_from_args(args));
		}

	public:

		Play_root(Env &env, Allocator &md_alloc)
		:
			Root_component<Play_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }
};

#endif /* _PLAY_SESSION_H_ */
