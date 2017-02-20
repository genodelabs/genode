/*
 * \brief  Management of subsystems
 * \author Norman Feske
 * \date   2014-10-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SUBSYSTEM_MANAGER_H_
#define _SUBSYSTEM_MANAGER_H_

/* Genode includes */
#include <base/component.h>
#include <decorator/xml_utils.h>

/* CLI-monitor includes */
#include <cli_monitor/child.h>

namespace Launcher {

	class Subsystem_manager;
	using Decorator::string_attribute;
	using Cli_monitor::Ram;
	using namespace Genode;
}


class Launcher::Subsystem_manager
{
	public:

		/**
		 * Exception types
		 */
		class Invalid_config { };

	private:

		Env &        _env;
		Heap         _heap { _env.ram(), _env.rm() };
		size_t const _ram_preservation;

		struct Child : Cli_monitor::Child_base, List<Child>::Element
		{
			template <typename... ARGS>
			Child(ARGS &&... args) : Child_base(args...) { }
		};

		List<Child> _children;

		void _try_response_to_resource_request()
		{
			for (Child *child = _children.first(); child; child = child->next())
				child->try_response_to_resource_request();
		}

		Signal_handler<Subsystem_manager> _yield_broadcast_handler =
			{ _env.ep(), *this, &Subsystem_manager::_handle_yield_broadcast };

		void _handle_yield_broadcast()
		{
			_try_response_to_resource_request();

			/*
			 * XXX copied from 'cli_monitor/main.cc'
			 */

			/*
			 * Compute argument of yield request to be broadcasted to all
			 * processes.
			 */
			size_t amount = 0;

			/* amount needed to reach preservation limit */
			Ram::Status ram_status = _ram.status();
			if (ram_status.avail < ram_status.preserve)
				amount += ram_status.preserve - ram_status.avail;

			/* sum of pending resource requests */
			for (Child *child = _children.first(); child; child = child->next())
				amount += child->requested_ram_quota();

			for (Child *child = _children.first(); child; child = child->next())
				child->yield(amount, true);
		}

		Signal_handler<Subsystem_manager> _resource_avail_handler =
			{ _env.ep(), *this, &Subsystem_manager::_handle_resource_avail };

		void _handle_resource_avail()
		{
			_try_response_to_resource_request();
		}

		Signal_handler<Subsystem_manager> _yield_response_handler =
			{ _env.ep(), *this, &Subsystem_manager::_handle_yield_response };

		void _handle_yield_response()
		{
			_try_response_to_resource_request();
		}

		Genode::Signal_context_capability _exited_child_sig_cap;

		Ram _ram { _env.ram(),
		           _env.ram_session_cap(),
		           _ram_preservation,
		           _yield_broadcast_handler,
		           _resource_avail_handler };

		static Child::Binary_name _binary_name(Xml_node subsystem)
		{
			try {
				return string_attribute(subsystem.sub_node("binary"),
				                        "name", Child::Binary_name(""));
			} catch (Xml_node::Nonexistent_sub_node) {
				Genode::error("missing <binary> definition");
				throw Invalid_config();
			}
		}

		struct Ram_config { Number_of_bytes quantum, limit; };

		static Ram_config _ram_config(Xml_node subsystem)
		{
			Number_of_bytes quantum = 0, limit = 0;
			try {
				subsystem.for_each_sub_node("resource", [&] (Xml_node rsc) {
					if (rsc.attribute("name").has_value("RAM")) {

						rsc.attribute("quantum").value(&quantum);

						if (rsc.has_attribute("limit"))
							rsc.attribute("limit").value(&limit);
					}
				});
			} catch (...) {
				Genode::error("invalid RAM resource declaration");
				throw Invalid_config();
			}

			return Ram_config { quantum, limit };
		}

	public:

		Subsystem_manager(Genode::Env & env,
		                  size_t ram_preservation,
		                  Genode::Signal_context_capability exited_child_sig_cap)
		:
			_env(env), _ram_preservation(ram_preservation),
			_exited_child_sig_cap(exited_child_sig_cap)
		{ }

		/**
		 * Start subsystem
		 *
		 * \throw Invalid_config
		 */
		void start(Xml_node subsystem)
		{
			Child::Binary_name const binary_name = _binary_name(subsystem);

			Label const label = string_attribute(subsystem, "name",
			                                            Label(""));

			Ram_config const ram_config = _ram_config(subsystem);

			Genode::log("starting child '", label.string(), "'");

			try {
				Child *child = new (_heap)
					Child(_ram, _heap, label, binary_name.string(),
					      _env.pd(),
					      _env.ram(),
					      _env.ram_session_cap(),
					      _env.rm(),
					      ram_config.quantum, ram_config.limit,
					      _yield_broadcast_handler,
					      _exited_child_sig_cap);

				/* configure child */
				try {
					Xml_node config_node = subsystem.sub_node("config");
					child->configure(config_node.addr(), config_node.size());
				} catch (...) { }

				_children.insert(child);

				child->start();

			} catch (Parent::Service_denied) {
				Genode::error("failed to start ", binary_name);
				throw Invalid_config();
			}
		}

		void kill(char const *label)
		{
			for (Child *c = _children.first(); c; c = c->next()) {
				if (c->label() == Label(label)) {
					_children.remove(c);
					destroy(_heap, c);
					return;
				}
			}
		}

		/**
		 * Call functor for each exited child
		 *
		 * The functor takes a 'Label' as argument.
		 */
		template <typename FUNC>
		void for_each_exited_child(FUNC const &func)
		{
			Child *next = nullptr;
			for (Child *child = _children.first(); child; child = next) {
				next = child->next();
				if (child->exited())
					func(Label(child->label().string()));
			}
		}
};

#endif /* _SUBSYSTEM_MANAGER_H_ */
