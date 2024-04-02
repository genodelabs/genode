/*
 * \brief  Record service of the black hole component
 * \author Josef Soentgen
 * \date   2024-04-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RECORD_SESSION_H_
#define _RECORD_SESSION_H_

#include <base/attached_ram_dataspace.h>
#include <record_session/record_session.h>
#include <root/component.h>

namespace Black_hole {

	using namespace Genode;

	class  Record_session;
	class  Record_root;
}


class Black_hole::Record_session : public Session_object<Record::Session, Record_session>
{
	private:

		Env                       &_env;
		Attached_ram_dataspace     _ram_ds;
		Signal_context_capability  _wakeup_sigh;

	public:

		Record_session(Env             &env,
		             Resources const &resources,
		             Label     const &label,
		             Diag      const &diag)
		:
			Session_object(env.ep(), resources, label, diag),
			_env(env),
			_ram_ds(_env.ram(), _env.rm(), Record::Session::DATASPACE_SIZE),
			_wakeup_sigh()
		{ }

		void wakeup()
		{
			if (_wakeup_sigh.valid())
				Signal_transmitter(_wakeup_sigh).submit();
		}

		/****************************
		 ** Record session interface **
		 ****************************/

		Dataspace_capability dataspace() { return _ram_ds.cap(); }

		void wakeup_sigh(Signal_context_capability sigh)
		{
			_wakeup_sigh = sigh;
			wakeup(); /* initial wakeup */
		}

		Record::Session::Record_result record(Record::Num_samples)
		{
			return Record::Time_window { 0, 0 };
		}

		void record_at(Record::Time_window,
		               Record::Num_samples)
		{ }
};


class Black_hole::Record_root : public Root_component<Record_session>
{
	private:

		Env &_env;

	protected:

		Record_session *_create_session(const char *args) override
		{
			if (session_resources_from_args(args).ram_quota.value < Record::Session::DATASPACE_SIZE)
				throw Insufficient_ram_quota();

			return new (md_alloc())
				Record_session(_env,
				             session_resources_from_args(args),
				             session_label_from_args(args),
				             session_diag_from_args(args));
		}

	public:

		Record_root(Env &env, Allocator &md_alloc)
		:
			Root_component<Record_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }
};

#endif /* _RECORD_SESSION_H_ */
