/*
 * \brief  Event session component and root
 * \author Martin Stein
 * \date   2021-12-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_H_
#define _EVENT_H_

/* Genode includes */
#include <base/session_object.h>
#include <event_session/event_session.h>

namespace Black_hole {

	using namespace Genode;

	class Event_session;
	class Event_root;
}


class Black_hole::Event_session
:
public Session_object<Event::Session, Event_session>
{
	private:

		Constrained_ram_allocator _ram;

		Attached_ram_dataspace _ds;

	public:

		Event_session(Env              &env,
		              Resources  const &resources,
		              Label      const &label,
		              Diag       const &diag)
		:
			Session_object(env.ep(), resources, label, diag),
			_ram(env.ram(), _ram_quota_guard(), _cap_quota_guard()),
			_ds(_ram, env.rm(), 4096)
		{ }

		~Event_session() { }


		/*****************************
		 ** Event session interface **
		 *****************************/

		Dataspace_capability dataspace() { return _ds.cap(); }

		void submit_batch(unsigned const /* count */) { }
};


class Black_hole::Event_root : public Root_component<Event_session>
{
	private:

		Env &_env;

	protected:

		Event_session *_create_session(const char *args) override
		{
			return new (md_alloc())
				Event_session(_env,
				              session_resources_from_args(args),
				              session_label_from_args(args),
				              session_diag_from_args(args));
		}

		void _upgrade_session(Event_session *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Event_session *session) override
		{
			Genode::destroy(md_alloc(), session);
		}

	public:

		Event_root(Env &env, Allocator &md_alloc)
		:
			Root_component<Event_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }
};

#endif /* _EVENT_H_ */
