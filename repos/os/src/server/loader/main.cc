/*
 * \brief  Loader service
 * \author Norman Feske
 * \date   2012-04-17
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <loader_session/loader_session.h>
#include <root/component.h>
#include <os/session_policy.h>

/* local includes */
#include <child.h>
#include <nitpicker.h>
#include <ram_session_client_guard.h>
#include <rom.h>


namespace Loader {

	using namespace Genode;

	class Session_component;
	class Root;
}


class Loader::Session_component : public Rpc_object<Session>
{
	private:

		struct Local_rom_factory : Local_service<Rom_session_component>::Factory
		{
			Entrypoint                 &_ep;
			Allocator                  &_md_alloc;
			Rom_module_registry        &_rom_modules;
			Lock                        _lock { };
			List<Rom_session_component> _rom_sessions { };

			void _close(Rom_session_component &rom)
			{
				_rom_sessions.remove(&rom);
				Genode::destroy(_md_alloc, &rom);
			}

			Local_rom_factory(Entrypoint          &ep,
			                  Allocator           &md_alloc,
			                  Rom_module_registry &rom_modules)
			:
				_ep(ep), _md_alloc(md_alloc), _rom_modules(rom_modules)
			{ }

			~Local_rom_factory()
			{
				Lock::Guard guard(_lock);

				while (_rom_sessions.first())
					_close(*_rom_sessions.first());
			}

			Rom_session_component &create(Args const &args, Affinity) override
			{
				/* try to find ROM module at local ROM service */
				try {
					Lock::Guard guard(_lock);

					Session_label const label = label_from_args(args.string());
					Session_label const name  = label.last_element();

					Rom_module &module = _rom_modules.lookup_and_lock(name.string());

					Rom_session_component *rom = new (_md_alloc)
						Rom_session_component(_ep, module);

					_rom_sessions.insert(rom);

					return *rom;

				} catch (...) { }

				throw Service_denied();
			}

			void upgrade(Rom_session_component &, Args const &) override { }

			void destroy(Rom_session_component &session) override
			{
				Lock::Guard guard(_lock);

				_close(session);
			}
		};

		typedef Local_service<Rom_session_component> Local_rom_service;

		/**
		 * Common base class of 'Local_cpu_service' and 'Local_pd_service'
		 */
		struct Intercepted_parent_service : Genode::Parent_service
		{
			Signal_context_capability fault_sigh { };

			Intercepted_parent_service(Env &env, Service::Name const &name)
			: Parent_service(env, name) { }
		};

		/**
		 * Intercept CPU session requests to install default exception
		 * handler
		 */
		struct Local_cpu_service : Intercepted_parent_service
		{
			Local_cpu_service(Env &env) : Intercepted_parent_service(env, "CPU") { }

			void initiate_request(Session_state &session) override
			{
				Intercepted_parent_service::initiate_request(session);

				if (session.phase != Session_state::AVAILABLE)
					return;

				Cpu_session_client cpu(reinterpret_cap_cast<Cpu_session>(session.cap));
				cpu.exception_sigh(fault_sigh);
			}
		};

		/**
		 * Intercept PD session requests to install default fault handler
		 */
		struct Local_pd_service : Intercepted_parent_service
		{
			Local_pd_service(Env &env) : Intercepted_parent_service(env, "PD") { }

			void initiate_request(Session_state &session) override
			{
				Intercepted_parent_service::initiate_request(session);

				if (session.phase != Session_state::AVAILABLE)
					return;

				Pd_session_client pd(reinterpret_cap_cast<Pd_session>(session.cap));

				Region_map_client(pd.address_space()).fault_handler(fault_sigh);
				Region_map_client(pd.stack_area())   .fault_handler(fault_sigh);
				Region_map_client(pd.linker_area())  .fault_handler(fault_sigh);
			}
		};

		struct Local_nitpicker_factory : Local_service<Nitpicker::Session_component>::Factory
		{
			Entrypoint    &_ep;
			Env           &_env;
			Region_map    &_rm;
			Ram_allocator &_ram;

			Area                       _max_size    { };
			Nitpicker::View_capability _parent_view { };

			Signal_context_capability view_ready_sigh { };

			Constructible<Nitpicker::Session_component> session { };

			Local_nitpicker_factory(Entrypoint &ep, Env &env,
			                        Region_map &rm, Ram_allocator &ram)
			: _ep(ep), _env(env), _rm(rm), _ram(ram) { }

			void constrain_geometry(Area size) { _max_size = size; }

			void parent_view(Nitpicker::View_capability view)
			{
				_parent_view = view;
			}

			Nitpicker::Session_component &create(Args const &args, Affinity) override
			{
				if (session.constructed()) {
					warning("attempt to open more than one nitpicker session");
					throw Service_denied();
				}

				session.construct(_ep, _env, _rm, _ram, _max_size,
				                  _parent_view, view_ready_sigh, args.string());
				return *session;
			}

			void upgrade(Nitpicker::Session_component &, Args const &) override { }
			void destroy(Nitpicker::Session_component &) override { }
		};

		typedef Local_service<Nitpicker::Session_component> Local_nitpicker_service;

		enum { STACK_SIZE = 2*4096 };

		Env                        &_env;
		Session_label         const _label;
		Xml_node              const _config;
		Cap_quota             const _cap_quota;
		Ram_quota             const _ram_quota;
		Ram_session_client_guard    _local_ram { _env.ram_session_cap(), _ram_quota };
		Heap                        _md_alloc { _local_ram, _env.rm() };
		size_t                      _subsystem_cap_quota_limit = 0;
		size_t                      _subsystem_ram_quota_limit = 0;
		Parent_services             _parent_services { };
		Rom_module_registry         _rom_modules { _env, _config, _local_ram, _md_alloc };
		Local_rom_factory           _rom_factory { _env.ep(), _md_alloc, _rom_modules };
		Local_rom_service           _rom_service { _rom_factory };
		Local_cpu_service           _cpu_service { _env };
		Local_pd_service            _pd_service  { _env };
		Local_nitpicker_factory     _nitpicker_factory { _env.ep(), _env, _env.rm(), _local_ram };
		Local_nitpicker_service     _nitpicker_service { _nitpicker_factory };
		Signal_context_capability   _fault_sigh { };
		Constructible<Child>        _child { };

		/**
		 * Return virtual nitpicker session component
		 */
		Nitpicker::Session_component &_virtual_nitpicker_session()
		{
			if (!_nitpicker_factory.session.constructed())
				throw View_does_not_exist();

			return *_nitpicker_factory.session;
		}

		Nitpicker::Session_component const &_virtual_nitpicker_session() const
		{
			if (!_nitpicker_factory.session.constructed())
				throw View_does_not_exist();

			return *_nitpicker_factory.session;
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Env &env, Session_label const &label, Xml_node config,
		                  Cap_quota cap_quota, Ram_quota ram_quota)
		:
			_env(env), _label(label), _config(config),
			_cap_quota(cap_quota), _ram_quota(ram_quota)
		{
			/* fetch all parent-provided ROMs according to the config */
			config.for_each_sub_node("parent-rom", [&] (Xml_node rom)
			{
				typedef Rom_module::Name Name;
				Name name = rom.attribute_value("name", Name());
				_rom_modules.fetch_parent_rom_module(name);
			});
		}

		~Session_component()
		{
			_child.destruct();

			/*
			 * The parent-service registry is populated by the 'Child'
			 * on demand. Revert those allocations.
			 */
			_parent_services.for_each([&] (Parent_service &service) {
				destroy(_md_alloc, &service); });
		}


		/******************************
		 ** Loader session interface **
		 ******************************/

		Dataspace_capability alloc_rom_module(Name const &name, size_t size) override
		{
			return _rom_modules.alloc_rom_module(name.string(), size);
		}

		void commit_rom_module(Name const &name) override
		{
			try {
				_rom_modules.commit_rom_module(name.string()); }
			catch (Rom_module_registry::Lookup_failed) {
				throw Rom_module_does_not_exist(); }
		}

		void cap_quota(Cap_quota caps) override
		{
			_subsystem_cap_quota_limit = caps.value;
		}

		void ram_quota(Ram_quota quantum) override
		{
			_subsystem_ram_quota_limit = quantum.value;
		}

		void constrain_geometry(Area size) override
		{
			_nitpicker_factory.constrain_geometry(size);
		}

		void parent_view(Nitpicker::View_capability view) override
		{
			_nitpicker_factory.parent_view(view);
		}

		void view_ready_sigh(Signal_context_capability sigh) override
		{
			_nitpicker_factory.view_ready_sigh = sigh;
		}

		void fault_sigh(Signal_context_capability sigh) override
		{
			/*
			 * CPU exception handler for CPU sessions originating from the
			 * subsystem.
			 */
			_cpu_service.fault_sigh = sigh;

			/*
			 * Region-map fault handler for PD sessions originating from the
			 * subsystem.
			 */
			_pd_service.fault_sigh = sigh;

			/*
			 * CPU exception and RM fault handler for the immediate child.
			 */
			_fault_sigh = sigh;
		}

		void start(Name const &binary_name, Name const &label) override
		{
			if (_child.constructed()) {
				warning("cannot start subsystem twice");
				return;
			}

			size_t const cap_quota = (_subsystem_cap_quota_limit > 0)
			                       ? min(_subsystem_cap_quota_limit, _cap_quota.value)
			                       : _cap_quota.value;

			size_t const ram_quota = (_subsystem_ram_quota_limit > 0)
			                       ? min(_subsystem_ram_quota_limit, _ram_quota.value)
			                       : _ram_quota.value;

			try {
				_child.construct(_env, _md_alloc, binary_name.string(),
				                 prefixed_label(_label, Session_label(label.string())),
				                 Cap_quota{cap_quota}, Ram_quota{ram_quota},
				                 _parent_services, _rom_service,
				                 _cpu_service, _pd_service, _nitpicker_service,
				                 _fault_sigh);
			}
			catch (Genode::Service_denied) {
				throw Rom_module_does_not_exist(); }
		}

		void view_geometry(Rect rect, Point offset) override
		{
			_virtual_nitpicker_session().loader_view_geometry(rect, offset);
		}

		Area view_size() const override
		{
			return _virtual_nitpicker_session().loader_view_size();
		}
};


class Loader::Root : public Root_component<Session_component>
{
	private:

		Env           &_env;
		Xml_node const _config;

	protected:

		Session_component *_create_session(const char *args)
		{
			Xml_node session_config("<policy/>");

			Session_label const label = label_from_args(args);

			try { session_config = Session_policy(label, _config); }
			catch (...) { }

			return new (md_alloc()) Session_component(_env, label, session_config,
			                                          cap_quota_from_args(args),
			                                          ram_quota_from_args(args));
		}

	public:

		Root(Env &env, Xml_node config, Allocator &md_alloc)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _config(config)
		{ }
};


namespace Loader { struct Main; }


struct Loader::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Root _root { _env, _config.xml(), _heap };

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Loader::Main main(env); }
