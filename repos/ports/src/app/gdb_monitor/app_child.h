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

#include <init/child_config.h>
#include <init/child_policy.h>

#include <util/arg_string.h>

/* GDB monitor includes */
#include "genode_child_resources.h"
#include "cpu_session_component.h"
#include "pd_session_component.h"
#include "rom.h"

namespace Gdb_monitor {
	using namespace Genode;

	class App_child;
}

class Gdb_monitor::App_child : public Child_policy
{
	public:

		typedef Registered<Genode::Parent_service> Parent_service;
		typedef Registry<Parent_service>           Parent_services;

	private:

		enum { STACK_SIZE = 4*1024*sizeof(long) };

		Env                                &_env;

		Allocator                          &_alloc;

		Ram_session_capability              _ref_ram_cap { _env.ram_session_cap() };
		Ram_session_client                  _ref_ram { _ref_ram_cap };

		const char                         *_unique_name;

		Dataspace_capability                _elf_ds;

		Region_map                         &_rm;

		size_t                              _ram_quota;

		Rpc_entrypoint                      _entrypoint;

		Parent_services                     _parent_services;

		Init::Child_config                  _child_config;

		Init::Child_policy_provide_rom_file _config_policy;

		Genode_child_resources              _genode_child_resources;

		Signal_dispatcher<App_child>        _unresolved_page_fault_dispatcher;

		Dataspace_pool                      _managed_ds_map;

		Pd_session_component                _pd { _entrypoint, _env, _alloc, _unique_name, _managed_ds_map };
		Pd_service::Single_session_factory  _pd_factory { _pd };
		Pd_service                          _pd_service { _pd_factory };

		Local_cpu_factory                   _cpu_factory;
		Cpu_service                         _cpu_service { _cpu_factory };

		Local_rom_factory                   _rom_factory;
		Rom_service                         _rom_service { _rom_factory };

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

	public:

		/**
		 * Constructor
		 */
		App_child(Env             &env,
		          Allocator       &alloc,
		          char const      *unique_name,
		          size_t           ram_quota,
		          Signal_receiver &signal_receiver,
		          Xml_node         target_node)
		:
			_env(env),
			_alloc(alloc),
			_unique_name(unique_name),
			_rm(_env.rm()),
			_ram_quota(ram_quota),
			_entrypoint(&_env.pd(), STACK_SIZE, "GDB monitor entrypoint"),
			_child_config(env.ram(), _rm, target_node),
			_config_policy("config", _child_config.dataspace(), &_entrypoint),
			_unresolved_page_fault_dispatcher(signal_receiver,
			                                  *this,
			                                  &App_child::_dispatch_unresolved_page_fault),
			_cpu_factory(_env, _entrypoint, _alloc, _pd.core_pd_cap(),
			             signal_receiver, &_genode_child_resources),
			_rom_factory(env, _entrypoint, _alloc)
		{
			_genode_child_resources.region_map_component(&_pd.region_map());
			_pd.region_map().fault_handler(_unresolved_page_fault_dispatcher);
		}

		~App_child()
		{
			destroy(_alloc, _child);
		}

		Genode_child_resources *genode_child_resources()
		{
			return &_genode_child_resources;
		}

		void start()
		{
			_child = new (_alloc) Child(_rm, _entrypoint, *this);
		}

		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return _unique_name; }

		Ram_session &ref_ram() override { return _ref_ram; }

		Ram_session_capability ref_ram_cap() const override { return _ref_ram_cap; }

		void init(Ram_session &session,
		          Ram_session_capability cap) override
		{
			session.ref_account(_ref_ram_cap);
			_ref_ram.transfer_quota(cap, _ram_quota);
		}

		Service &resolve_session_request(Service::Name const &service_name,
										 Session_state::Args const &args) override
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
				service = new (_alloc) Parent_service(_parent_services, _env, service_name);

			if (!service)
				throw Parent::Service_denied();

			return *service;
		}

		// XXX adjust to API change, need to serve a "session_requests" rom
		// to the child
		void announce_service(Service::Name const &name) override
		{
			Genode::warning(__PRETTY_FUNCTION__, ": not implemented");
		}
};

#endif /* _APP_CHILD_H_ */
