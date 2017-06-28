/*
 * \brief  Utility for providing "session_requests" ROM to a child service
 * \author Norman Feske
 * \date   2016-11-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__SESSION_REQUESTER_H_
#define _INCLUDE__OS__SESSION_REQUESTER_H_

#include <parent/parent.h>
#include <os/dynamic_rom_session.h>

namespace Genode { class Session_requester; }


class Genode::Session_requester
{
	private:

		Id_space<Parent::Server> _id_space;

		struct Content_producer : Dynamic_rom_session::Content_producer
		{
			Id_space<Parent::Server> &_id_space;

			Content_producer(Id_space<Parent::Server> &id_space)
			: _id_space(id_space) { }

			void produce_content(char *dst, Genode::size_t dst_len) override
			{
				try {
					Xml_generator xml(dst, dst_len, "session_requests", [&] () {
						_id_space.for_each<Session_state const>([&] (Session_state const &s) {
							s.generate_session_request(xml); }); });
				} catch (Xml_generator::Buffer_exceeded &) {
					throw Buffer_capacity_exceeded();
				}
			}
		} _content_producer { _id_space };

		typedef Local_service<Dynamic_rom_session> Service;

		Dynamic_rom_session             _session;
		Service::Single_session_factory _factory { _session };
		Service                         _service { _factory };

	public:

		typedef String<32> Rom_name;

		static Rom_name rom_name() { return "session_requests"; }

		/**
		 * Constructor
		 *
		 * \param ep   entrypoint serving the local ROM service
		 * \param ram  backing store for the ROM dataspace
		 * \param rm   local address space, needed to populate the dataspace
		 */
		Session_requester(Rpc_entrypoint &ep, Ram_session &ram, Region_map &rm)
		:
			_session(ep, ram, rm, _content_producer)
		{ }

		/**
		 * Inform the child about a new version of the "session_requests" ROM
		 */
		void trigger_update() { _session.trigger_update(); }

		/**
		 * ID space for sessios requests supplied to the child
		 */
		Id_space<Parent::Server> &id_space() { return _id_space; }

		/**
		 * Non-modifiable ID space for sessios requests supplied to the child
		 */
		Id_space<Parent::Server> const &id_space() const { return _id_space; }

		/**
		 * ROM service providing a single "session_requests" session
		 */
		Service &service() { return _service; }
};

#endif /* _INCLUDE__OS__SESSION_REQUESTER_H_ */
