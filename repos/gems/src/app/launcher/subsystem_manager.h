/*
 * \brief  Management of subsystems
 * \author Norman Feske
 * \date   2014-10-02
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SUBSYSTEM_MANAGER_H_
#define _SUBSYSTEM_MANAGER_H_

/* Genode includes */
#include <os/server.h>
#include <decorator/xml_utils.h>

/* CLI-monitor includes */
#include <cli_monitor/child.h>

namespace Launcher {

	class Subsystem_manager;
	using Decorator::string_attribute;
}


/***************
 ** Utilities **
 ***************/

/*
 * XXX copied from 'cli_monitor/main.cc'
 */
static Genode::size_t ram_preservation_from_config()
{
	Genode::Number_of_bytes ram_preservation = 0;
	try {
		Genode::Xml_node node =
			Genode::config()->xml_node().sub_node("preservation");

		if (node.attribute("name").has_value("RAM"))
			node.attribute("quantum").value(&ram_preservation);
	} catch (...) { }

	return ram_preservation;
}


class Launcher::Subsystem_manager
{
	public:

		/**
		 * Exception types
		 */
		class Invalid_config { };

	private:

		Server::Entrypoint  &_ep;
		Cap_session         &_cap;
		Dataspace_capability _ldso_ds;

		struct Child : Child_base, List<Child>::Element
		{
			typedef String<128> Binary_name;

			Child(Ram                      &ram,
			      Label              const &label,
			      Binary_name        const &binary,
			      Cap_session              &cap_session,
			      size_t                    ram_quota,
			      size_t                    ram_limit,
			      Signal_context_capability yield_response_sig_cap,
			      Signal_context_capability exit_sig_cap,
			      Dataspace_capability      ldso_ds)
			:
				Child_base(ram,
				           label.string(),
				           binary.string(),
				           cap_session,
				           ram_quota,
				           ram_limit,
				           yield_response_sig_cap,
				           exit_sig_cap,
				           ldso_ds)
			{ }
		};

		List<Child> _children;

		void _try_response_to_resource_request()
		{
			for (Child *child = _children.first(); child; child = child->next())
				child->try_response_to_resource_request();
		}

		Genode::Signal_rpc_member<Subsystem_manager> _yield_broadcast_dispatcher =
			{ _ep, *this, &Subsystem_manager::_handle_yield_broadcast };

		void _handle_yield_broadcast(unsigned)
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

		Genode::Signal_rpc_member<Subsystem_manager> _resource_avail_dispatcher =
			{ _ep, *this, &Subsystem_manager::_handle_resource_avail };

		void _handle_resource_avail(unsigned)
		{
			_try_response_to_resource_request();
		}

		Genode::Signal_rpc_member<Subsystem_manager> _yield_response_dispatcher =
			{ _ep, *this, &Subsystem_manager::_handle_yield_response };

		void _handle_yield_response(unsigned)
		{
			_try_response_to_resource_request();
		}

		Genode::Signal_context_capability _exited_child_sig_cap;

		Ram _ram { ram_preservation_from_config(),
		           _yield_broadcast_dispatcher,
		           _resource_avail_dispatcher };

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

		Subsystem_manager(Server::Entrypoint &ep, Cap_session &cap,
		                  Genode::Signal_context_capability exited_child_sig_cap,
		                  Dataspace_capability ldso_ds)
		:
			_ep(ep), _cap(cap), _ldso_ds(ldso_ds),
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
				Child *child = new (env()->heap())
					Child(_ram, label, binary_name.string(), _cap,
					      ram_config.quantum, ram_config.limit,
					      _yield_broadcast_dispatcher,
					      _exited_child_sig_cap, _ldso_ds);

				/* configure child */
				try {
					Xml_node config_node = subsystem.sub_node("config");
					child->configure(config_node.addr(), config_node.size());
				} catch (...) { }

				_children.insert(child);

				child->start();

			} catch (Rom_connection::Rom_connection_failed) {
				Genode::error("binary \"", binary_name, "\" is missing");
				throw Invalid_config();
			}
		}

		void kill(char const *label)
		{
			for (Child *c = _children.first(); c; c = c->next()) {
				if (c->label() == Label(label)) {
					_children.remove(c);
					destroy(env()->heap(), c);
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
