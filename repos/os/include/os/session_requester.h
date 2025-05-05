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

		using Local_rm = Local::Constrained_region_map;

		Id_space<Parent::Server> _id_space { };

		struct Content_producer : Dynamic_rom_session::Content_producer
		{
			Id_space<Parent::Server> &_id_space;

			Content_producer(Id_space<Parent::Server> &id_space)
			: _id_space(id_space) { }

			Result produce_content(Byte_range_ptr const &dst) override
			{
				return Xml_generator::generate(dst, "session_requests",
					[&] (Xml_generator &xml) {
						_id_space.for_each<Session_state const>([&] (Session_state const &s) {
							s.generate_session_request(xml); }); }
				).convert<Result>([&] (size_t)         { return Ok(); },
				                  [&] (Buffer_error e) { return e; });
			}
		} _content_producer { _id_space };

		using Service = Local_service<Dynamic_rom_session>;

		Dynamic_rom_session             _session;
		Service::Single_session_factory _factory { _session };
		Service                         _service { _factory };

	public:

		using Rom_name = String<32>;

		static Rom_name rom_name() { return "session_requests"; }

		/**
		 * Constructor
		 *
		 * \param ep   entrypoint serving the local ROM service
		 * \param ram  backing store for the ROM dataspace
		 * \param rm   local address space, needed to populate the dataspace
		 */
		Session_requester(Rpc_entrypoint &ep, Ram_allocator &ram, Local_rm &rm)
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
