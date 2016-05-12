/*
 * \brief  Loader service
 * \author Norman Feske
 * \date   2012-04-17
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <base/rpc_server.h>
#include <base/signal.h>
#include <base/sleep.h>
#include <loader_session/loader_session.h>
#include <cap_session/connection.h>
#include <root/component.h>

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

		struct Local_rom_service : Service
		{
			Rpc_entrypoint             &_ep;
			Allocator                  &_md_alloc;
			Parent_service              _parent_rom_service;
			Rom_module_registry        &_rom_modules;
			Lock                        _lock;
			List<Rom_session_component> _rom_sessions;

			void _close(Rom_session_component *rom)
			{
				_rom_sessions.remove(rom);
				destroy(&_md_alloc, rom);
			}

			Local_rom_service(Rpc_entrypoint      &ep,
			                  Allocator           &md_alloc,
			                  Rom_module_registry &rom_modules)
			:
				Service("virtual_rom"),
				_ep(ep),
				_md_alloc(md_alloc),
				_parent_rom_service(Rom_session::service_name()),
				_rom_modules(rom_modules)
			{ }

			~Local_rom_service()
			{
				Lock::Guard guard(_lock);

				while (_rom_sessions.first()) {
					_ep.remove(_rom_sessions.first());
					_close(_rom_sessions.first());
				}
			}

			Genode::Session_capability session(char     const *args,
			                                   Affinity const &affinity)
			{
				/* try to find ROM module at local ROM service */
				try {
					Lock::Guard guard(_lock);

					Session_label const label = label_from_args(args);
					Session_label name = label.last_element();

					Rom_module &module = _rom_modules.lookup_and_lock(name.string());

					Rom_session_component *rom = new (&_md_alloc)
						Rom_session_component(module);

					_rom_sessions.insert(rom);

					return _ep.manage(rom);

				} catch (...) { }

				/* fall back to parent_rom_service */
				return _parent_rom_service.session(args, affinity);
			}

			void close(Session_capability session)
			{
				Lock::Guard guard(_lock);

				Rom_session_component *component;

				_ep.apply(session, [&] (Rom_session_component *rsc) {
					component = rsc;
					if (component) _ep.remove(component);
				});

				if (component) {
					_close(component);
					return;
				}

				_parent_rom_service.close(session);
			}

			void upgrade(Session_capability session, const char *) { }
		};

		/**
		 * Common base class of 'Local_cpu_service' and 'Local_pd_service'
		 */
		struct Intercepted_parent_service : Service
		{
			Signal_context_capability fault_sigh;

			Intercepted_parent_service(char const *name) : Service(name) { }

			void close(Session_capability session)
			{
				env()->parent()->close(session);
			}

			void upgrade(Session_capability session, const char *) { }
		};

		/**
		 * Intercept CPU session requests to install default exception
		 * handler
		 */
		struct Local_cpu_service : Intercepted_parent_service
		{
			Local_cpu_service() : Intercepted_parent_service("CPU") { }

			Genode::Session_capability session(char     const *args,
			                                   Affinity const &affinity)
			{
				Capability<Cpu_session> cap = env()->parent()->session<Cpu_session>(args, affinity);
				Cpu_session_client(cap).exception_sigh(fault_sigh);
				return cap;
			}

			void upgrade(Session_capability session, const char *args)
			{
				try { env()->parent()->upgrade(session, args); }
				catch (Genode::Ipc_error)    { throw Unavailable();    }
			}
		};

		/**
		 * Intercept PD session requests to install default fault handler
		 */
		struct Local_pd_service : Intercepted_parent_service
		{
			Local_pd_service() : Intercepted_parent_service("PD") { }

			Genode::Session_capability session(char     const *args,
			                                   Affinity const &affinity)
			{
				Pd_session_client pd(env()->parent()->session<Pd_session>(args, affinity));

				Region_map_client(pd.address_space()).fault_handler(fault_sigh);
				Region_map_client(pd.stack_area())   .fault_handler(fault_sigh);
				Region_map_client(pd.linker_area())  .fault_handler(fault_sigh);

				return pd;
			}
		};

		struct Local_nitpicker_service : Service
		{
			Rpc_entrypoint &_ep;
			Ram_session    &_ram;
			Allocator      &_md_alloc;

			Area                       _max_size;
			Nitpicker::View_capability _parent_view;

			Signal_context_capability view_ready_sigh;

			Nitpicker::Session_component *open_session;

			Local_nitpicker_service(Rpc_entrypoint &ep, Ram_session &ram,
			                        Allocator &md_alloc)
			:
				Service("virtual_nitpicker"),
				_ep(ep),
				_ram(ram),
				_md_alloc(md_alloc),
				open_session(0)
			{ }

			~Local_nitpicker_service()
			{
				if (!open_session)
					return;

				_ep.dissolve(open_session);
				destroy(&_md_alloc, open_session);
			}

			void constrain_geometry(Area size)
			{
				_max_size = size;
			}

			void parent_view(Nitpicker::View_capability view)
			{
				_parent_view = view;
			}

			Genode::Session_capability session(char     const *args,
			                                   Affinity const &)
			{
				if (open_session)
					throw Unavailable();

				open_session = new (&_md_alloc)
					Nitpicker::Session_component(_ep,
					                             _ram,
					                             _max_size,
					                             _parent_view,
					                             view_ready_sigh,
					                             args);

				return _ep.manage(open_session);
			}

			void upgrade(Genode::Session_capability session, const char *) { }
		};

		enum { STACK_SIZE = 2*4096 };

		size_t                    _ram_quota;
		Ram_session_client_guard  _ram_session_client;
		Heap                      _md_alloc;
		size_t                    _subsystem_ram_quota_limit;
		Rpc_entrypoint            _ep;
		Dataspace_capability      _ldso_ds;
		Service_registry          _parent_services;
		Rom_module_registry       _rom_modules;
		Local_rom_service         _rom_service;
		Local_cpu_service         _cpu_service;
		Local_pd_service          _pd_service;
		Local_nitpicker_service   _nitpicker_service;
		Signal_context_capability _fault_sigh;
		Child                    *_child;

		/**
		 * Return virtual nitpicker session component
		 */
		Nitpicker::Session_component &_virtual_nitpicker_session() const
		{
			if (!_nitpicker_service.open_session)
				throw View_does_not_exist();

			return *_nitpicker_service.open_session;
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(size_t quota, Ram_session &ram, Cap_session &cap,
		                  Dataspace_capability ldso_ds)
		:
			_ram_quota(quota),
			_ram_session_client(env()->ram_session_cap(), _ram_quota),
			_md_alloc(&_ram_session_client, env()->rm_session()),
			_subsystem_ram_quota_limit(0),
			_ep(&cap, STACK_SIZE, "session_ep"),
			_ldso_ds(ldso_ds),
			_rom_modules(_ram_session_client, _md_alloc),
			_rom_service(_ep, _md_alloc, _rom_modules),
			_nitpicker_service(_ep, _ram_session_client, _md_alloc),
			_child(0)
		{ }

		~Session_component()
		{
			if (_child)
				destroy(&_md_alloc, _child);

			/*
			 * The parent-service registry is populated by the 'Child'
			 * on demand. Revert those allocations.
			 */
			while (Service *service = _parent_services.find_by_server(0)) {
				_parent_services.remove(service);
				destroy(env()->heap(), service);
			}
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

		void ram_quota(size_t quantum) override
		{
			_subsystem_ram_quota_limit = quantum;
		}

		void constrain_geometry(Area size) override
		{
			_nitpicker_service.constrain_geometry(size);
		}

		void parent_view(Nitpicker::View_capability view) override
		{
			_nitpicker_service.parent_view(view);
		}

		void view_ready_sigh(Signal_context_capability sigh) override
		{
			_nitpicker_service.view_ready_sigh = sigh;
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
			if (_child) {
				PWRN("cannot start subsystem twice");
				return;
			}

			size_t const ram_quota = (_subsystem_ram_quota_limit > 0) ?
			                         min(_subsystem_ram_quota_limit, _ram_session_client.avail()) :
			                         _ram_session_client.avail();

			try {
				_child = new (&_md_alloc)
					Child(binary_name.string(), label.string(), _ldso_ds,
					      _ep, _ram_session_client,
					      ram_quota, _parent_services, _rom_service,
					      _cpu_service, _pd_service, _nitpicker_service,
					      _fault_sigh);
			}
			catch (Genode::Parent::Service_denied) {
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

		Ram_session         &_ram;
		Cap_session         &_cap;
		Dataspace_capability _ldso_ds;

	protected:

		Session_component *_create_session(const char *args)
		{
			size_t quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			return new (md_alloc()) Session_component(quota, _ram, _cap, _ldso_ds);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep  entry point for managing ram session objects
		 * \param md_alloc    meta-data allocator to be used by root
		 *                    component
		 */
		Root(Rpc_entrypoint &session_ep, Allocator &md_alloc,
		     Ram_session &ram, Cap_session &cap, Dataspace_capability ldso_ds)
		:
			Root_component<Session_component>(&session_ep, &md_alloc),
			_ram(ram), _cap(cap), _ldso_ds(ldso_ds)
		{ }
};


Genode::Dataspace_capability request_ldso_ds()
{
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		return rom.dataspace();
	} catch (...) { }
	return Genode::Dataspace_capability();
}


int main()
{
	using namespace Genode;

	enum { STACK_SIZE = 8*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "loader_ep");

	static Loader::Root root(ep, *env()->heap(), *env()->ram_session(), cap,
	                         request_ldso_ds());

	env()->parent()->announce(ep.manage(&root));

	sleep_forever();
	return 0;
}
