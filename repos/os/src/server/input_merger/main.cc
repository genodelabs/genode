/*
 * \brief  Input event merger
 * \author Christian Prochaska
 * \date   2014-09-29
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <input/component.h>
#include <input_session/connection.h>
#include <os/static_root.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_dataspace.h>
#include <base/heap.h>
#include <base/session_label.h>
#include <util/list.h>


namespace Input_merger
{
	using namespace Genode;

	struct Input_source;
	struct Main;

	typedef List<Input_source> Input_sources;
	typedef String<Session_label::capacity()> Label;
}


struct Input_merger::Input_source : Input_sources::Element
{
	Input::Connection         conn;
	Input::Session_component &sink;

	Genode::Signal_handler<Input_source> event_handler;

	void handle_events()
	{
		conn.for_each_event([&] (Input::Event const &e) { sink.submit(e); });
	}

	Input_source(Genode::Env &env, Label const &label, Input::Session_component &sink)
	:
		conn(env, label.string()), sink(sink),
		event_handler(env.ep(), *this, &Input_source::handle_events)
	{
		conn.sigh(event_handler);
	}
};


/******************
 ** Main program **
 ******************/

struct Input_merger::Main
{
	Env &env;

	Attached_rom_dataspace config_rom { env, "config" };

	Heap heap { env.ram(), env.rm() };

	Input_sources input_source_list;

	/*
	 * Input session provided to our client
	 */
	Input::Session_component input_session_component { env, env.ram() };

	/*
	 * Attach root interface to the entry point
	 */
	Static_root<Input::Session> input_root { env.ep().manage(input_session_component) };

	void handle_config_update()
	{
		config_rom.update();

		while (Input_source *input_source = input_source_list.first()) {
			input_source_list.remove(input_source);
			destroy(heap, input_source);
		}

		config_rom.xml().for_each_sub_node("input", [&] (Xml_node input_node) {
			try {
				Label label;
				input_node.attribute("label").value(&label);

				try {
					Input_source *input_source = new (heap)
						Input_source(env, label.string(), input_session_component);

					input_source_list.insert(input_source);

				} catch (Genode::Parent::Service_denied) {
					error("parent denied input source '", label, "'");
				}
			} catch (Xml_node::Nonexistent_attribute) {
				error("ignoring invalid input node '", input_node);
			}
		});
	}

	Signal_handler<Main> config_update_handler
		{ env.ep(), *this, &Main::handle_config_update };

	/**
	 * Constructor
	 */
	Main(Genode::Env &env) : env(env)
	{
		input_session_component.event_queue().enabled(true);

		/*
		 * Apply initial configuration
		 */
		handle_config_update();

		/*
		 * Register signal handler
		 */
		config_rom.sigh(config_update_handler);

		/*
		 * Announce service
		 */
		env.parent().announce(env.ep().manage(input_root));
	}

};


void Component::construct(Genode::Env &env) { static Input_merger::Main inst(env); }
