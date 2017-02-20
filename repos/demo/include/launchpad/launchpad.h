/*
 * \brief  Launchpad interface
 * \author Norman Feske
 * \date   2006-09-01
 *
 * This class is a convenience-wrapper for starting and killing
 * child processes. To support the Launchpad application, is
 * also provides an interface to glues the child handling and
 * the GUI together.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LAUNCHPAD__LAUNCHPAD_H_
#define _INCLUDE__LAUNCHPAD__LAUNCHPAD_H_

#include <base/allocator.h>
#include <base/service.h>
#include <base/lock.h>
#include <timer_session/timer_session.h>
#include <pd_session/client.h>
#include <init/child.h>

class Launchpad;


class Launchpad_child : public Genode::Child_policy,
                        public Genode::List<Launchpad_child>::Element,
                        public Genode::Child_service::Wakeup
{
	public:

		typedef Genode::Child_policy::Name Name;

		typedef Genode::Registered<Genode::Child_service>  Child_service;
		typedef Genode::Registered<Genode::Parent_service> Parent_service;

		typedef Genode::Registry<Child_service>  Child_services;
		typedef Genode::Registry<Parent_service> Parent_services;

	private:

		Name const _name;

		Binary_name const _elf_name;

		Genode::Env &_env;

		Genode::Allocator &_alloc;

		Genode::Ram_session_capability _ref_ram_cap;
		Genode::Ram_session_client     _ref_ram { _ref_ram_cap };
		Genode::size_t           const _ram_quota;

		Parent_services &_parent_services;
		Child_services  &_child_services;

		Genode::Dataspace_capability _config_ds;

		Genode::Session_requester _session_requester;

		Init::Child_policy_provide_rom_file _config_policy;

		Genode::Child _child;

		/**
		 * Child_service::Wakeup callback
		 */
		void wakeup_child_service() override
		{
			_session_requester.trigger_update();
		}

		template <typename T>
		static Genode::Service *_find_service(Genode::Registry<T> &services,
		                                      Genode::Service::Name const &name)
		{
			Genode::Service *service = nullptr;
			services.for_each([&] (T &s) {
				if (!service && (s.name() == name))
					service = &s; });
			return service;
		}

	public:

		Launchpad_child(Genode::Env                 &env,
		                Genode::Allocator           &alloc,
		                Genode::Session_label const &label,
		                Binary_name           const &elf_name,
		                Genode::size_t               ram_quota,
		                Parent_services             &parent_services,
		                Child_services              &child_services,
		                Genode::Dataspace_capability config_ds)
		:
			_name(label), _elf_name(elf_name),
			_env(env), _alloc(alloc),
			_ref_ram_cap(env.ram_session_cap()), _ram_quota(ram_quota),
			_parent_services(parent_services),
			_child_services(child_services),
			_session_requester(env.ep().rpc_ep(), _env.ram(), _env.rm()),
			_config_policy("config", config_ds, &_env.ep().rpc_ep()),
			_child(_env.rm(), _env.ep().rpc_ep(), *this)
		{ }

		~Launchpad_child()
		{
			using namespace Genode;

			/* unregister services */
			_child_services.for_each(
				[&] (Child_service &service) {
					if (service.has_id_space(_session_requester.id_space()))
						Genode::destroy(_alloc, &service); });
		}


		/****************************
		 ** Child_policy interface **
		 ****************************/

		Name name() const override { return _name; }

		Binary_name binary_name() const override { return _elf_name; }

		Genode::Ram_session &ref_ram() override { return _ref_ram; }

		Genode::Ram_session_capability ref_ram_cap() const override { return _ref_ram_cap; }

		void init(Genode::Ram_session &session,
		          Genode::Ram_session_capability cap) override
		{
			session.ref_account(_ref_ram_cap);
			_ref_ram.transfer_quota(cap, _ram_quota);
		}

		Genode::Id_space<Genode::Parent::Server> &server_id_space() override {
			return _session_requester.id_space(); }

		Genode::Service &resolve_session_request(Genode::Service::Name const &service_name,
		                                         Genode::Session_state::Args const &args) override
		{
			Genode::Service *service = nullptr;

			/* check for config file request */
			if ((service = _config_policy
			               .resolve_session_request(service_name.string(), args.string())))
				return *service;

			/* check for "session_requests" ROM request */
			Genode::Session_label const label(Genode::label_from_args(args.string()));
			if (service_name == Genode::Rom_session::service_name()
			 && label.last_element() == Genode::Session_requester::rom_name())
				return _session_requester.service();

			/* if service is provided by one of our children, use it */
			if ((service = _find_service(_child_services, service_name)))
				return *service;

			/*
			 * Handle special case of the demo scenario when the user uses
			 * a nested Launchad for starting another Nitpicker instance
			 * before starting Liquid_fb. In this case, we do not want to
			 * delegate Nitpicker's session requests to the parent. The
			 * parent may be a launchpad again trying to apply the same
			 * policy. This instance will recognise that the session is not
			 * available at init (because the actual input and framebuffer
			 * drivers are singletons, and would therefore block for
			 * Framebuffer or Input to become available at one of its own
			 * children. The launchpad with which the user originally
			 * interacted, however, would block at the parent interface
			 * until this condition gets satisfied.
			 */
			if (service_name != "Input"
			 && service_name != "Framebuffer"
			 && ((service = _find_service(_parent_services, service_name))))
				return *service;

			Genode::warning(name(), ": service ", service_name, " not available");
			throw Genode::Parent::Service_denied();
		}

		void announce_service(Genode::Service::Name const &service_name) override
		{
			if (_find_service(_child_services, service_name)) {
				Genode::warning(name(), ": service ", service_name, " is already registered");
				return;
			}

			new (_alloc)
				Child_service(_child_services, _session_requester.id_space(),
				              _child.session_factory(), service_name,
				              _child.ram_session_cap(), *this);
		}
};


class Launchpad
{
	private:

		Genode::Env &_env;

		Genode::Heap _heap { _env.ram(), _env.rm() };

		unsigned long _initial_quota;

		Launchpad_child::Parent_services _parent_services;
		Launchpad_child::Child_services  _child_services;

		Genode::Lock                  _children_lock;
		Genode::List<Launchpad_child> _children;

		bool _child_name_exists(Launchpad_child::Name const &);

		Launchpad_child::Name _get_unique_child_name(Launchpad_child::Name const &);

		Genode::Sliced_heap _sliced_heap;

	protected:

		int _ypos;

	public:

		Launchpad(Genode::Env &env, unsigned long initial_quota);

		unsigned long initial_quota() { return _initial_quota; }

		virtual ~Launchpad() { }

		/**
		 * Process launchpad XML configuration
		 */
		void process_config(Genode::Xml_node);


		/*************************
		 ** Configuring the GUI **
		 *************************/

		virtual void quota(unsigned long quota) { }

		virtual void add_launcher(Launchpad_child::Name const &binary_name,
		                          unsigned long default_quota,
		                          Genode::Dataspace_capability config_ds) { }

		virtual void add_child(Launchpad_child::Name const &,
		                       unsigned long quota,
		                       Launchpad_child &,
		                       Genode::Allocator &) { }

		virtual void remove_child(Launchpad_child::Name const &,
		                          Genode::Allocator &) { }

		Launchpad_child *start_child(Launchpad_child::Name const &binary_name,
		                             unsigned long quota,
		                             Genode::Dataspace_capability config_ds);

		/**
		 * Exit child and close all its sessions
		 */
		void exit_child(Launchpad_child &child);
};

#endif /* _INCLUDE__LAUNCHPAD__LAUNCHPAD_H_ */
