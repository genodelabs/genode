/*
 * \brief  Utility to sequence component execution
 * \author Emery Hemingway
 * \date   2017-08-09
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <init/child_policy.h>
#include <base/attached_rom_dataspace.h>
#include <os/child_policy_dynamic_rom.h>
#include <base/sleep.h>
#include <base/child.h>
#include <base/component.h>

namespace Sequence {
	using namespace Genode;

	struct Child;
	struct Main;
}


struct Sequence::Child : Genode::Child_policy
{
	Genode::Env &_env;

	Heap _services_heap { _env.pd(), _env.rm() };

	Xml_node const _start_node;

	Name const _name = _start_node.attribute_value("name", Name());

	Binary_name _start_binary()
	{
		Binary_name name;
		try {
			_start_node.sub_node("binary").attribute("name").value(&name);
			return name != "" ? name : _name;
		}
		catch (...) { return _name; }
	}

	Binary_name const _binary_name = _start_binary();

	Child_policy_dynamic_rom_file _config_policy {
		_env.rm(), "config", _env.ep().rpc_ep(), &_env.pd() };

	class Parent_service : public Genode::Parent_service
	{
		private:

			Registry<Parent_service>::Element _reg_elem;

		public:

			Parent_service(Registry<Parent_service> &registry, Env &env,
			               Service::Name const &name)
			:
				Genode::Parent_service(env, name), _reg_elem(registry, *this)
			{ }
	};

	Registry<Parent_service> _parent_services;

	Genode::Child _child { _env.rm(), _env.ep().rpc_ep(), *this };

	/* queue a child reload from the async Parent interface */
	Signal_transmitter _exit_transmitter;

	bool _have_config = _start_node.has_sub_node("config");

	Child(Genode::Env &env,
	      Xml_node const &start_node,
	      Signal_context_capability exit_handler)
	:
		_env(env),
		_start_node(start_node),
		_exit_transmitter(exit_handler)
	{
		if (_have_config) {
			Xml_node config_node = start_node.sub_node("config");
			_config_policy.load(config_node.addr(), config_node.size());
		}
	}

	~Child()
	{
		_parent_services.for_each([&] (Parent_service &service) {
			destroy(_services_heap, &service); });
	}


	/****************************
	 ** Child_policy interface **
	 ****************************/

	Name name() const override { return _name; }

	Binary_name binary_name() const override { return _binary_name; }

	/**
	 * Provide a "config" ROM if configured to do so,
	 * otherwise forward directly to the parent.
	 */
	Service &resolve_session_request(Service::Name       const &name,
	                                 Session_state::Args const &args)
	{
		if (_have_config) {
			Service *s = _config_policy.resolve_session_request(
				name.string(), args.string());
			if (s)
				return *s;
		}

		return *new (_services_heap) Parent_service(_parent_services, _env, name);
	}

	/**
	 * Only a single child is managed at a time so
	 * no additional PD management is required.
	 */
	Pd_session           &ref_pd()           override { return _env.pd(); }
	Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

	/**
	 * If the exit is successful then queue a child reload via a signal,
	 * otherwise exit this parent component.
	 */
	void exit(int exit_value) override
	{
		if (exit_value == 0)
			_exit_transmitter.submit();
		else {
			error("child \"", name(), "\" exited with exit value ", exit_value);
			_env.parent().exit(exit_value);
			sleep_forever();
		}
	}

	/**
	 * TODO: respond to yield_response by withdrawing
	 * child quota and informing our parent.
	 */

	/**
	 * Upgrade child quotas from our quotas,
	 * otherwise request more quota from our parent.
	 */
	void resource_request(Parent::Resource_args const &args) override
	{
		Ram_quota ram = ram_quota_from_args(args.string());
		Cap_quota caps = cap_quota_from_args(args.string());

		Pd_session_capability pd_cap = _child.pd_session_cap();

		/* XXX: pretty simplistic math here */

		if (ram.value) {
			Ram_quota avail = _env.pd().avail_ram();
			if (avail.value > ram.value) {
				ref_pd().transfer_quota(pd_cap, ram);
			} else {
				ref_pd().transfer_quota(pd_cap, Ram_quota{avail.value >> 1});
				_env.parent().resource_request(args);
			}
		}

		if (caps.value) {
			Cap_quota avail = _env.pd().avail_caps();
			if (avail.value > caps.value) {
				ref_pd().transfer_quota(pd_cap, caps);
			} else {
				ref_pd().transfer_quota(pd_cap, Cap_quota{avail.value >> 1});
				_env.parent().resource_request(args);
			}
		}

		_child.notify_resource_avail();
	}

	/**
	 * Initialize the child Protection Domain session with half of
	 * the initial quotas of this parent component.
	 */
	void init(Pd_session &pd, Pd_session_capability pd_cap) override
	{
		pd.ref_account(ref_pd_cap());
		ref_pd().transfer_quota(pd_cap, Cap_quota{_env.pd().avail_caps().value >> 1});
		ref_pd().transfer_quota(pd_cap, Ram_quota{_env.pd().avail_ram().value >> 1});
	}
};


struct Sequence::Main
{
	Genode::Env &env;

	Constructible<Sequence::Child> child;

	Attached_rom_dataspace config_rom { env, "config" };

	Xml_node const config_xml = config_rom.xml();

	int next_xml_index = 0;

	void start_next_child();

	Signal_handler<Main> exit_handler {
		env.ep(), *this, &Main::start_next_child };

	Main(Genode::Env &e) : env(e) {
		start_next_child(); }
};


void Sequence::Main::start_next_child()
{
	if (child.constructed())
		child.destruct();

	try { while (true) {
		Xml_node sub_node = config_xml.sub_node(next_xml_index++);
		if (sub_node.type() != "start")
			continue;
		child.construct(env, sub_node, exit_handler);
		break;
	} }

	catch (Xml_node::Nonexistent_sub_node) {
		env.parent().exit(0); }
}


void Component::construct(Genode::Env &env) {
	static Sequence::Main main(env); }
