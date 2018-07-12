/*
 * \brief  Utilities for handling the 'session_requests' ROM
 * \author Emery Hemingway
 * \date   2018-04-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __SESSION_REQUESTS_H_
#define __SESSION_REQUESTS_H_

#include <base/attached_rom_dataspace.h>
#include <base/session_state.h>
#include <base/component.h>

namespace Genode {
	struct Session_request_handler;
	class Session_requests_rom;
};


struct Genode::Session_request_handler : Interface
{
	virtual void handle_session_create(Session_state::Name const &,
	                                   Parent::Server::Id,
	                                   Session_state::Args const &) = 0;
	virtual void handle_session_upgrade(Parent::Server::Id,
	                                    Session_state::Args const &) { }
	virtual void handle_session_close(Parent::Server::Id) = 0;
};


class Genode::Session_requests_rom : public Signal_handler<Session_requests_rom>
{
	private:

		Parent                  &_parent;
		Session_request_handler &_requests_handler;

		Attached_rom_dataspace  _parent_rom;

		void _process()
		{
			_parent_rom.update();
			Xml_node requests = _parent_rom.xml();

			auto const create_fn = [&] (Xml_node request)
			{
				Parent::Server::Id const id {
					request.attribute_value("id", ~0UL) };

				typedef Session_state::Name Name;
				typedef Session_state::Args Args;

				Name name { };
				Args args { };

				try {
					name = request.attribute_value("service", Name());
					args = request.sub_node("args").decoded_content<Args>();
				} catch (...) {
					Genode::error("failed to parse request ", request);
					return;
				}

				try { _requests_handler.handle_session_create(name, id, args); }
				catch (Service_denied) {
					_parent.session_response(id, Parent::SERVICE_DENIED); }
				catch (Insufficient_ram_quota) {
					_parent.session_response(id, Parent::INSUFFICIENT_RAM_QUOTA); }
				catch (Insufficient_cap_quota) {
					_parent.session_response(id, Parent::INSUFFICIENT_CAP_QUOTA); }
				catch (...) {
					error("unhandled exception while creating session");
					_parent.session_response(id, Parent::SERVICE_DENIED);
					throw;
				}
			};

			auto const upgrade_fn = [&] (Xml_node request)
			{
				Parent::Server::Id const id {
					request.attribute_value("id", ~0UL) };

				typedef Session_state::Args Args;
				Args args { };
				try { args = request.sub_node("args").decoded_content<Args>(); }
				catch (...) {
					Genode::error("failed to parse request ", request);
					return;
				}

				_requests_handler.handle_session_upgrade(id, args);
			};

			auto const close_fn = [&] (Xml_node request)
			{
				Parent::Server::Id const id {
					request.attribute_value("id", ~0UL) };
				_requests_handler.handle_session_close(id);
			};

			/* close sessions to free resources */
			requests.for_each_sub_node("close", close_fn);

			/* service existing sessions */
			requests.for_each_sub_node("upgrade", upgrade_fn);

			/* create new sessions */
			requests.for_each_sub_node("create", create_fn);
		}

	public:

		Session_requests_rom(Genode::Env &env,
		                     Session_request_handler &requests_handler)
		: Signal_handler<Session_requests_rom>(env.ep(), *this, &Session_requests_rom::_process),
		  _parent(env.parent()),
		  _requests_handler(requests_handler),
		  _parent_rom(env, "session_requests")
		{
			_parent_rom.sigh(*this);
		}

		/**
		 * Post a signal to this requests handler
		 */
		void schedule() {
			Signal_transmitter(*this).submit(); }
};

#endif
