/*
 * \brief  Frontend of input service
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INPUT__COMPONENT_H_
#define _INCLUDE__INPUT__COMPONENT_H_

#include <base/env.h>
#include <base/rpc_server.h>
#include <os/attached_ram_dataspace.h>
#include <root/component.h>
#include <input_session/input_session.h>
#include <input/event.h>


namespace Input {

	/********************
	 ** Input back end **
	 ********************/

	/**
	 * Enable/disable input event handling
	 *
	 * \param enable  enable (true) or disable (false) back end
	 *
	 * The front end informs the back end about when to start capturing input
	 * events for an open session. Later, the back end may be deactivated on
	 * session destruction.
	 */
	void event_handling(bool enable);

	/**
	 * Check if an event is pending
	 */
	bool event_pending();

	/**
	 * Wait for an event, Zzz...zz..
	 */
	Input::Event get_event();


	/*****************************
	 ** Input service front end **
	 *****************************/

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			/*
			 * Input event buffer that is shared with the client
			 */
			enum { MAX_EVENTS = 1000 };

			Genode::Attached_ram_dataspace _ev_ds;

		public:

			Session_component()
			: _ev_ds(Genode::env()->ram_session(), MAX_EVENTS*sizeof(Event)) {
				event_handling(true); }

			~Session_component() {
				event_handling(false); }

			Genode::Dataspace_capability dataspace() { return _ev_ds.cap(); }

			bool is_pending() const { return event_pending(); }

			int flush()
			{
				/* dump events into event buffer dataspace */
				int i;
				Input::Event *ev_ds_buf = _ev_ds.local_addr<Input::Event>();
				for (i = 0; (i < MAX_EVENTS) && event_pending(); ++i)
					ev_ds_buf[i] = get_event();

				/* return number of flushed events */
				return i;
			}
	};


	/**
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component, Genode::Single_client> Root_component;


	class Root : public Root_component
	{
		protected:

			Session_component *_create_session(const char *args) {
				return new (md_alloc()) Session_component(); }

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc)
			: Root_component(session_ep, md_alloc) { }
	};
}

#endif /* _INCLUDE__INPUT__COMPONENT_H_ */
