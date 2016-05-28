/*
 * \brief  Input event merger
 * \author Christian Prochaska
 * \date   2014-09-29
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <input/component.h>
#include <input_session/connection.h>
#include <os/attached_dataspace.h>
#include <os/config.h>
#include <os/server.h>
#include <os/static_root.h>
#include <util/list.h>


namespace Input_merger
{
	using namespace Genode;

	class Input_source : public List<Input_source>::Element
	{
		private:

			Input::Session_capability _create_session(const char *label)
			{
				char session_args[sizeof(Parent::Session_args)] { 0 };

				Arg_string::set_arg(session_args, sizeof(Parent::Session_args),
				                    "ram_quota", "16K");
				Arg_string::set_arg_string(session_args, sizeof(Parent::Session_args),
				                    "label", label);

				return env()->parent()->session<Input::Session>(session_args);
			}

		public:

			Input::Session_client session_client;
			Attached_dataspace    dataspace;

			Input_source(const char *label)
			: session_client(_create_session(label)),
		  	  dataspace(session_client.dataspace()) { }

		  	~Input_source()
		  	{
				env()->parent()->close(session_client);
		  	}
	};

	struct Main;
}


/******************
 ** Main program **
 ******************/

struct Input_merger::Main
{
	Server::Entrypoint &ep;
	List<Input_source> input_source_list;

	/*
	 * Input session provided to our client
	 */
	Input::Session_component input_session_component;

	/*
	 * Attach root interface to the entry point
	 */
	Static_root<Input::Session> input_root { ep.manage(input_session_component) };

	void handle_input(unsigned)
	{
		for (Input_source *input_source = input_source_list.first();
		     input_source;
		     input_source = input_source->next()) {

			Input::Event const * const events =
				input_source->dataspace.local_addr<Input::Event>();

			unsigned const num = input_source->session_client.flush();
			for (unsigned i = 0; i < num; i++)
				input_session_component.submit(events[i]);
		}
	}

	Signal_rpc_member<Main> input_dispatcher =
		{ ep, *this, &Main::handle_input};


	void handle_config_update(unsigned)
	{
		Genode::config()->reload();

		for (Input_source *input_source;
		     (input_source = input_source_list.first()); ) {
			input_source_list.remove(input_source);
			destroy(env()->heap(), input_source);
		}

		try {
			for (Xml_node input_node = config()->xml_node().sub_node("input");
			     ;
			     input_node = input_node.next("input")) {

				char label_buf[128];
				input_node.attribute("label").value(label_buf, sizeof(label_buf));

				Input_source *input_source(new (env()->heap()) Input_source(label_buf));

				input_source_list.insert(input_source);

				input_source->session_client.sigh(input_dispatcher);
			}
		} catch (Xml_node::Nonexistent_sub_node) { }

	}

	Signal_rpc_member<Main> config_update_dispatcher =
		{ ep, *this, &Main::handle_config_update};

	/**
	 * Constructor
	 */
	Main(Server::Entrypoint &ep) : ep(ep)
	{
		input_session_component.event_queue().enabled(true);

		/*
		 * Apply initial configuration
		 */
		handle_config_update(0);

		/*
		 * Register signal handler
		 */
		Genode::config()->sigh(config_update_dispatcher);

		/*
		 * Announce service
		 */
		Genode::env()->parent()->announce(ep.manage(input_root));
	}

};


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "input_merger_ep"; }

	size_t stack_size() { return 4*1024*sizeof(addr_t); }

	void construct(Entrypoint &ep) { static Input_merger::Main inst(ep); }
}
