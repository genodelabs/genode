/*
 * \brief  Event service
 * \author Norman Feske
 * \date   2020-07-16
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__EVENT_SESSION_H_
#define _EVENT_FILTER__EVENT_SESSION_H_

/* Genode includes */
#include <event_session/event_session.h>
#include <root/component.h>
#include <base/session_object.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <os/session_policy.h>

/* local includes */
#include <source.h>

namespace Event_filter {

	typedef String<Session_label::capacity()> Input_name;

	class Event_session;
	class Event_root;
}


class Event_filter::Event_session : public Session_object<Event::Session, Event_session>
{
	private:

		Input_name _input_name { };  /* may change on reconfiguration */

		Source::Trigger &_trigger;

		Constrained_ram_allocator _ram;

		Attached_ram_dataspace _ds;

		unsigned _pending_count = 0;

		unsigned _key_cnt = 0;

		template <typename FN>
		void _for_each_pending_event(FN const &fn) const
		{
			Input::Event const * const events = _ds.local_addr<Input::Event const>();

			for (unsigned i = 0; i < _pending_count; i++)
				fn(events[i]);
		}

	public:

		Event_session(Env                 &env,
		              Resources     const &resources,
		              Label         const &label,
		              Diag          const &diag,
		              Source::Trigger     &trigger)
		:
			Session_object(env.ep(), resources, label, diag),
			_trigger(trigger),
			_ram(env.ram(), _ram_quota_guard(), _cap_quota_guard()),
			_ds(_ram, env.rm(), 4096)
		{ }

		/**
		 * Collect pending input from event client
		 *
		 * \param input_name  name of queried input
		 *
		 * This method is called during processing of 'Main::trigger_generate'.
		 */
		template <typename FN>
		void for_each_pending_event(Input_name const &input_name, FN const &fn) const
		{
			if (input_name == _input_name)
				_for_each_pending_event(fn);
		}

		bool idle() const { return (_key_cnt == 0); }

		/**
		 * (Re-)assign input name to session according to session policy
		 */
		void assign_input_name(Xml_node config)
		{
			_input_name = Input_name();

			try {
				Session_policy policy(_label, config);

				_input_name = policy.attribute_value("input", Input_name());

			} catch (Service_denied) { }
		}


		/*****************************
		 ** Event session interface **
		 *****************************/

		Dataspace_capability dataspace() { return _ds.cap(); }

		void submit_batch(unsigned const count)
		{
			size_t const max_events = _ds.size() / sizeof(Input::Event);

			if (count > max_events)
				warning("number of events exceeds dataspace capacity");

			_pending_count = min(count, max_events);

			auto update_key_cnt = [&] (Input::Event const &event)
			{
				if (event.press())   _key_cnt++;
				if (event.release()) _key_cnt--;
			};

			_for_each_pending_event(update_key_cnt);

			_trigger.trigger_generate();

			_pending_count = 0;
		}
};


class Event_filter::Event_root : public Root_component<Event_session>
{
	private:

		Env &_env;

		Source::Trigger &_trigger;

		Attached_rom_dataspace const &_config;

		Registry<Registered<Event_session>> _sessions { };

	protected:

		Event_session *_create_session(const char *args) override
		{
			Event_session &session = *new (md_alloc())
				Registered<Event_session>(_sessions,
				                          _env,
				                          session_resources_from_args(args),
				                          session_label_from_args(args),
				                          session_diag_from_args(args),
				                          _trigger);

			session.assign_input_name(_config.xml());

			return &session;
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

		/**
		 * Constructor
		 */
		Event_root(Env &env, Allocator &md_alloc, Source::Trigger &trigger,
		           Attached_rom_dataspace const &config)
		:
			Root_component<Event_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _trigger(trigger), _config(config)
		{ }

		template <typename FN>
		void for_each_pending_event(Input_name const &input_name, FN const &fn) const
		{
			_sessions.for_each([&] (Event_session const &session) {
				session.for_each_pending_event(input_name, fn); });
		}

		/**
		 * Return true if no client holds any keys pressed
		 */
		bool all_sessions_idle() const
		{
			bool idle = true;

			_sessions.for_each([&] (Event_session const &session) {
				if (!session.idle())
					idle = false; });

			return idle;
		}

		void apply_config(Xml_node const &config)
		{
			_sessions.for_each([&] (Event_session &session) {
				session.assign_input_name(config); });
		}
};

#endif /* _EVENT_FILTER__EVENT_SESSION_H_ */
