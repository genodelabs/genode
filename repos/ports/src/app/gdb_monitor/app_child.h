/*
 * \brief  Application child
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _APP_CHILD_H_
#define _APP_CHILD_H_

#include <stdio.h>
#include <sys/stat.h>

#include <base/child.h>
#include <base/service.h>

#include <init/child_config.h>
#include <init/child_policy.h>

#include <util/arg_string.h>

#include "genode_child_resources.h"
#include "cpu_session_component.h"
#include "pd_session_component.h"
#include "rom.h"


namespace Gdb_monitor { class App_child; }

class Gdb_monitor::App_child : public Child_policy
{
	public:

		typedef Genode::Registered<Genode::Parent_service> Parent_service;
		typedef Genode::Registry<Parent_service> Parent_services;

	private:

		enum { STACK_SIZE = 4*1024*sizeof(long) };

		Genode::Env                        &_env;

		Genode::Ram_session_capability      _ref_ram_cap { _env.ram_session_cap() };
		Genode::Ram_session_client          _ref_ram { _ref_ram_cap };

		const char                         *_unique_name;

		Genode::Dataspace_capability        _elf_ds;

		Genode::Region_map                 &_rm;

		Genode::size_t                      _ram_quota;

		Rpc_entrypoint                      _entrypoint;

		Parent_services                     _parent_services;

		Init::Child_config                  _child_config;

		Init::Child_policy_provide_rom_file _config_policy;

		Genode_child_resources              _genode_child_resources;

		Signal_dispatcher<App_child>        _unresolved_page_fault_dispatcher;

		Dataspace_pool                      _managed_ds_map;

		Pd_session_component                _pd {_unique_name, _entrypoint, _managed_ds_map};
		Pd_service::Single_session_factory  _pd_factory { _pd };
		Pd_service                          _pd_service { _pd_factory };

		Local_cpu_factory                   _cpu_factory;
		Cpu_service                         _cpu_service { _cpu_factory };

		Local_rom_factory                   _rom_factory;
		Rom_service                         _rom_service { _rom_factory };

		Child                              *_child;

		/* FIXME */
#if 0
		/**
		 * Proxy for a service provided by the child
		 */
		class Child_service_root : public Genode::Rpc_object<Genode::Root>
		{
			private:

				/**
				 * Root interface of the real service, provided by the child
				 */
				Genode::Root_client _child_root;

				/**
				 * Child's RAM session used for quota transfers
				 */
				Genode::Ram_session_capability _child_ram;

				struct Child_session;
				typedef Genode::Object_pool<Child_session> Session_pool;

				/**
				 * Per-session meta data
				 *
				 * For each session, we need to keep track of the quota
				 * attached to it - in order to revert the quota donation
				 * when the session gets closed.
				 */
				struct Child_session : public Session_pool::Entry
				{
					Genode::size_t ram_quota;

					Child_session(Genode::Session_capability cap,
								  Genode::size_t ram_quota)
					: Session_pool::Entry(cap), ram_quota(ram_quota) { }
				};

				/**
				 * Data base containing per-session meta data
				 */
				Session_pool _sessions;

			public:

				/**
				 * Constructor
				 */

				Child_service_root(Genode::Ram_session_capability child_ram,
					               Genode::Root_capability        child_root)
				: _child_root(child_root), _child_ram(child_ram)
				{ }

				/********************
				 ** Root interface **
				 ********************/

				Session_capability session(Session_args const &args,
					                       Affinity const &affinity)
				{
					using namespace Genode;

					Genode::size_t ram_quota =
						Arg_string::find_arg(args.string(),
							                 "ram_quota").ulong_value(0);

					/* forward session quota to child */
					env()->ram_session()->transfer_quota(_child_ram, ram_quota);

					Session_capability cap = _child_root.session(args, affinity);

					/*
					 * Keep information about donated quota in '_sessions'
					 * data base.
					 */
					_sessions.insert(new (env()->heap())
						             Child_session(cap, ram_quota));
					return cap;
				}

				void upgrade(Session_capability session_cap,
					         Upgrade_args const &args)
				{
					using namespace Genode;

					auto lambda = [&] (Child_session *session) {
						if (!session) {
							error("attempt to upgrade unknown session");
							return;
						}

						Genode::size_t ram_quota =
							Arg_string::find_arg(args.string(),
								                 "ram_quota").ulong_value(0);

						/* forward session quota to child */
						env()->ram_session()->transfer_quota(_child_ram, ram_quota);

						session->ram_quota += ram_quota;

						/* inform child about quota upgrade */
						_child_root.upgrade(session_cap, args);
					};

					_sessions.apply(session_cap, lambda);
				}

				void close(Session_capability session_cap)
				{
					using namespace Genode;

					Child_session *session;

					auto lambda = [&] (Child_session *s) {
						session = s;

						if (!session) {
							error("attempt to close unknown session");
							return;
						}
						_sessions.remove(session);
					};
					_sessions.apply(session_cap, lambda);

					Genode::size_t ram_quota = session->ram_quota;
					destroy(env()->heap(), session);

					_child_root.close(session_cap);

					/*
					 * The child is expected to free enough quota to revert
					 * the quota donation.
					 */

					Ram_session_client child_ram(_child_ram);
					child_ram.transfer_quota(env()->ram_session_cap(), ram_quota);
				}
		};
#endif
		void _dispatch_unresolved_page_fault(unsigned)
		{
			_genode_child_resources.cpu_session_component()->handle_unresolved_page_fault();
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

		/**
		 * Constructor
		 */
		App_child(Genode::Env        &env,
		          const char         *unique_name,
		          Genode::Pd_session &pd,
		          Genode::Region_map &rm,
		          Genode::size_t      ram_quota,
		          Signal_receiver    *signal_receiver,
		          Xml_node            target_node)
		:
			_env(env),
			_unique_name(unique_name),
			_rm(rm),
			_ram_quota(ram_quota),
			_entrypoint(&pd, STACK_SIZE, "GDB monitor entrypoint"),
			_child_config(env.ram(), rm, target_node),
			_config_policy("config", _child_config.dataspace(), &_entrypoint),
			_unresolved_page_fault_dispatcher(*signal_receiver,
			                                  *this,
			                                  &App_child::_dispatch_unresolved_page_fault),
			_cpu_factory(_env, _entrypoint, Genode::env()->heap(), _pd.core_pd_cap(),
			             signal_receiver, &_genode_child_resources),
			_rom_factory(env, _entrypoint)
		{
			_genode_child_resources.region_map_component(&_pd.region_map());
			_pd.region_map().fault_handler(_unresolved_page_fault_dispatcher);
		}

		~App_child()
		{
			destroy(env()->heap(), _child);
		}

		Genode_child_resources *genode_child_resources()
		{
			return &_genode_child_resources;
		}

		void start()
		{
			_child = new (env()->heap()) Child(_rm, _entrypoint, *this);
		}

		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return _unique_name; }

		Genode::Ram_session &ref_ram() override { return _ref_ram; }

		Genode::Ram_session_capability ref_ram_cap() const override { return _ref_ram_cap; }

		void init(Genode::Ram_session &session,
		          Genode::Ram_session_capability cap) override
		{
			session.ref_account(_ref_ram_cap);
			_ref_ram.transfer_quota(cap, _ram_quota);
		}

		Service &resolve_session_request(Genode::Service::Name const &service_name,
										 Genode::Session_state::Args const &args) override
		{
			Service *service = nullptr;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(service_name.string(), args.string())))
					return *service;

			if (service_name == "CPU")
				return _cpu_service;

			if (service_name == "PD")
				return _pd_service;

			if (service_name == "ROM")
				return _rom_service;

			service = _find_service(_parent_services, service_name);
			if (!service)
				service = new (env()->heap()) Parent_service(_parent_services, _env, service_name);

			if (!service)
				throw Parent::Service_denied();

			return *service;
		}

		// XXX adjust to API change, need to serve a "session_requests" rom
		// to the child
		void announce_service(Service::Name const &name) override
		{
			/* FIXME */
#if 0
			/* create and announce proxy for the child's root interface */
			Child_service_root *r = new (alloc)
				Child_service_root(_ram_session, root);
			Genode::env()->parent()->announce(name, _root_ep->manage(r));
#else
			Genode::warning(__PRETTY_FUNCTION__, ": not implemented");
#endif
		}
};

#endif /* _APP_CHILD_H_ */
