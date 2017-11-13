/*
 * \brief  Application child
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _APP_CHILD_H_
#define _APP_CHILD_H_

/* Genode includes */
#include <base/child.h>
#include <base/service.h>
#include <init/child_policy.h>
#include <os/session_requester.h>
#include <util/arg_string.h>

/* init private includes */
#include <init/server.h>

/* GDB monitor includes */
#include "genode_child_resources.h"
#include "cpu_session_component.h"
#include "pd_session_component.h"
#include "rom.h"
#include "child_config.h"

namespace Gdb_monitor {
	using namespace Genode;

	class App_child;
}

class Gdb_monitor::App_child : public Child_policy,
                               public Async_service::Wakeup,
                               public Init::Report_update_trigger,
                               public Init::Routed_service::Ram_accessor,
                               public Init::Routed_service::Pd_accessor
{
	private:

		typedef Registered<Genode::Parent_service> Parent_service;

		typedef Registry<Parent_service>           Parent_services;
		typedef Registry<Init::Routed_service>     Child_services;

		/**
		 * gdbserver blocks in 'select()', so a separate entrypoint is used.
		 */
		struct Local_env : Genode::Env
		{
			Genode::Env &genode_env;

			Genode::Entrypoint  local_ep {
				genode_env, 4*1024*sizeof(addr_t), "target_ep" };

			Local_env(Env &genode_env) : genode_env(genode_env) { }

			Parent &parent()                         { return genode_env.parent(); }
			Ram_session &ram()                       { return genode_env.ram(); }
			Cpu_session &cpu()                       { return genode_env.cpu(); }
			Region_map &rm()                         { return genode_env.rm(); }
			Pd_session &pd()                         { return genode_env.pd(); }
			Entrypoint &ep()                         { return local_ep; }
			Ram_session_capability ram_session_cap() { return genode_env.ram_session_cap(); }
			Cpu_session_capability cpu_session_cap() { return genode_env.cpu_session_cap(); }
			Pd_session_capability pd_session_cap()   { return genode_env.pd_session_cap(); }
			Id_space<Parent::Client> &id_space()     { return genode_env.id_space(); }

			Session_capability session(Parent::Service_name const &service_name,
			                           Parent::Client::Id id,
			                           Parent::Session_args const &session_args,
			                           Affinity             const &affinity)
			{ return genode_env.session(service_name, id, session_args, affinity); }

			void upgrade(Parent::Client::Id id, Parent::Upgrade_args const &args)
			{ return genode_env.upgrade(id, args); }

			void close(Parent::Client::Id id) { return genode_env.close(id); }

			void exec_static_constructors() override { }

			void reinit(Native_capability::Raw raw) override {
				genode_env.reinit(raw);
			}

			void reinit_main_thread(Capability<Region_map> &stack_area_rm) override {
				genode_env.reinit_main_thread(stack_area_rm);
			}
		};

		Local_env                           _env;

		Allocator                          &_alloc;

		Pd_session_capability               _ref_pd_cap { _env.pd_session_cap() };
		Pd_session                         &_ref_pd     { _env.pd() };

		const char                         *_unique_name;

		Dataspace_capability                _elf_ds;

		Region_map                         &_rm;

		Ram_quota                           _ram_quota;
		Cap_quota                           _cap_quota;

		Parent_services                     _parent_services;
		Child_services                      _child_services;

		Init::Child_config                  _child_config;

		Init::Child_policy_provide_rom_file _config_policy;

		Genode_child_resources              _genode_child_resources;

		Signal_dispatcher<App_child>        _unresolved_page_fault_dispatcher;

		Dataspace_pool                      _managed_ds_map;

		Pd_session_component                _pd { _env.ep().rpc_ep(),
		                                          _env,
		                                          _alloc,
		                                          _unique_name,
		                                          _managed_ds_map };

		Pd_service::Single_session_factory  _pd_factory { _pd };
		Pd_service                          _pd_service { _pd_factory };

		Local_cpu_factory                   _cpu_factory;
		Cpu_service                         _cpu_service { _cpu_factory };

		Local_rom_factory                   _rom_factory;
		Rom_service                         _rom_service { _rom_factory };

		Genode::Session_requester           _session_requester { _env.ep().rpc_ep(),
		                                                         _env.ram(),
		                                                         _env.rm() };

		Init::Server                        _server { _env, _alloc, _child_services, *this };

		Child                              *_child;

		void _dispatch_unresolved_page_fault(unsigned)
		{
			_genode_child_resources.cpu_session_component().handle_unresolved_page_fault();
		}

		template <typename T>
		static Service *_find_service(Registry<T> &services,
		                              Service::Name const &name)
		{
			Service *service = nullptr;
			services.for_each([&] (T &s) {
				if (!service && (s.name() == name))
					service = &s; });
			return service;
		}

		/**
		 * Child_service::Wakeup callback
		 */
		void wakeup_async_service() override
		{
			_session_requester.trigger_update();
		}

		/**
		 * Init::Report_update_trigger callback
		 */
		void trigger_report_update() override { }

		/**
		 * Init::Routed_service::Ram_accessor interface
		 */
		Ram_session            &ram()           override { return _child->ram(); }
		Ram_session_capability  ram_cap() const override { return _child->ram_session_cap(); }

		/**
		 * Init::Routed_service::Pd_accessor interface
		 */
		Pd_session            &pd()            override { return _child->pd(); }
		Pd_session_capability  pd_cap()  const override { return _child->pd_session_cap(); }

	public:

		/**
		 * Constructor
		 */
		App_child(Env             &env,
		          Allocator       &alloc,
		          char const      *unique_name,
		          Ram_quota        ram_quota,
		          Cap_quota        cap_quota,
		          Signal_receiver &signal_receiver,
		          Xml_node         target_node)
		:
			_env(env),
			_alloc(alloc),
			_unique_name(unique_name),
			_rm(_env.rm()),
			_ram_quota(ram_quota), _cap_quota(cap_quota),
			_child_config(_env.ram(), _rm, target_node),
			_config_policy("config", _child_config.dataspace(), &_env.ep().rpc_ep()),
			_unresolved_page_fault_dispatcher(signal_receiver,
			                                  *this,
			                                  &App_child::_dispatch_unresolved_page_fault),
			_cpu_factory(_env, _env.ep().rpc_ep(), _alloc, _pd.core_pd_cap(),
			             signal_receiver, &_genode_child_resources),
			_rom_factory(_env, _env.ep().rpc_ep(), _alloc)
		{
			_genode_child_resources.region_map_component(&_pd.region_map());
			_pd.region_map().fault_handler(_unresolved_page_fault_dispatcher);
		}

		~App_child()
		{
			_child_services.for_each([&] (Init::Routed_service &service) {
				destroy(_alloc, &service);
			});

			destroy(_alloc, _child);
		}

		Genode_child_resources *genode_child_resources()
		{
			return &_genode_child_resources;
		}

		void start()
		{
			_child = new (_alloc) Child(_rm, _env.ep().rpc_ep(), *this);
		}

		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return _unique_name; }

		Pd_session &ref_pd() override { return _ref_pd; }

		Pd_session_capability ref_pd_cap() const override { return _ref_pd_cap; }

		Id_space<Parent::Server> &server_id_space() override
		{
			return _session_requester.id_space();
		}

		void init(Pd_session &session,
		          Pd_session_capability cap) override
		{
			session.ref_account(_ref_pd_cap);

			_env.ep().rpc_ep().apply(cap, [&] (Pd_session_component *pd) {
				if (pd) {
					_ref_pd.transfer_quota(pd->core_pd_cap(), _cap_quota);
					_ref_pd.transfer_quota(pd->core_pd_cap(), _ram_quota);
				}
			});
		}

		Service &resolve_session_request(Service::Name const &service_name,
		                                 Session_state::Args const &args) override
		{
			Service *service = nullptr;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(service_name.string(), args.string())))
					return *service;

			/* check for "session_requests" ROM request */
			Genode::Session_label const label(Genode::label_from_args(args.string()));
			if ((service_name == Genode::Rom_session::service_name()) &&
			    (label.last_element() == Genode::Session_requester::rom_name()))
				return _session_requester.service();

			if (service_name == "CPU")
				return _cpu_service;

			if (service_name == "PD")
				return _pd_service;

			if (service_name == "ROM")
				return _rom_service;

			service = _find_service(_parent_services, service_name);
			if (!service)
				service = new (_alloc) Parent_service(_parent_services, _env, service_name);

			if (!service)
				throw Service_denied();

			return *service;
		}

		void announce_service(Service::Name const &service_name) override
		{
			if (_find_service(_child_services, service_name)) {
				Genode::warning(name(), ": service ", service_name, " is already registered");
				return;
			}

			new (_alloc) Init::Routed_service(_child_services,
			                                  "target",
			                                  *this,
			                                  *this,
			                                  _session_requester.id_space(),
			                                  _child->session_factory(),
			                                  service_name,
			                                  *this);

			char server_config[4096];

			try {
				Xml_generator xml(server_config, sizeof(server_config),
				                  "config", [&] () {
					_child_services.for_each([&] (Service &service) {
						xml.node("service", [&] () {
							xml.attribute("name", service_name);
							xml.node("default-policy", [&] () {
								xml.node("child", [&] () {
									xml.attribute("name", "target");
								});
							});
						});
					});
				});

				_server.apply_config(Xml_node(server_config));

			} catch(Xml_generator::Buffer_exceeded &) {
				error("XML buffer for server configuration exceeded");
			}

			_env.parent().announce(service_name.string());
		}
};

#endif /* _APP_CHILD_H_ */
