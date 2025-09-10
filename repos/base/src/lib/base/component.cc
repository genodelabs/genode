/*
 * \brief  Component bootstrap
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2016-01-13
 */

/*
 * Copyright (C) 2016-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/retry.h>
#include <base/component.h>
#include <base/connection.h>
#include <base/service.h>
#include <base/env.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/runtime.h>

namespace Genode { struct Component_env; }

using namespace Genode;


/**
 * Excecute pending static constructors
 *
 * The weak function is used for statically linked binaries. The dynamic linker
 * provides the real implementation for dynamically linked components.
 */
void Genode::exec_static_constructors() __attribute__((weak));
void Genode::exec_static_constructors() { }


struct Genode::Component_env : Env
{
	Runtime &_runtime;

	using Local_rm = Local::Constrained_region_map;

	Parent      &_parent   = _runtime.parent;
	Pd_session  &_pd       = _runtime.pd;
	Cpu_session &_cpu      = _runtime.cpu;
	Local_rm    &_local_rm = _runtime.local_rm;

	Capability<Pd_session>  _pd_cap  = _runtime.pd.rpc_cap();
	Capability<Cpu_session> _cpu_cap = _runtime.cpu.rpc_cap();

	Pd_ram_allocator _ram { _pd };

	Entrypoint &_ep;

	/**
	 * Mutex for serializing 'session' and 'close'
	 */
	Mutex _mutex { };

	/**
	 * Utility to used block for single signal
	 */
	struct Blockade
	{
		Parent          &_parent;
		Signal_receiver _sig_rec { };
		Signal_context  _sig_ctx { };

		Blockade(Parent &parent) : _parent(parent)
		{
			_parent.session_sigh(_sig_rec.manage(_sig_ctx));
		}

		void block() { _sig_rec.wait_for_signal(); }
	};

	Constructible<Blockade> _session_blockade { };

	Component_env(Runtime &runtime, Entrypoint &ep)
	:
		_runtime(runtime), _ep(ep)
	{ }

	Parent        &parent()  override { return _parent; }
	Cpu_session   &cpu()     override { return _cpu; }
	Local_rm      &rm()      override { return _local_rm; }
	Pd_session    &pd()      override { return _pd; }
	Ram_allocator &ram()     override { return _ram; }
	Entrypoint    &ep()      override { return _ep; }
	Runtime       &runtime() override { return _runtime; }

	Cpu_session_capability cpu_session_cap() override { return _cpu_cap; }
	Pd_session_capability  pd_session_cap()  override { return _pd_cap; }

	Id_space<Parent::Client> &id_space() override
	{
		return env_session_id_space();
	}

	void _block_for_session()
	{
		/*
		 * Construct blockade lazily be avoid it being used in core where
		 * all session requests are immediately answered.
		 */
		if (!_session_blockade.constructed())
			_session_blockade.construct(_parent);

		_session_blockade->block();
	}

	Session_result try_session(Parent::Service_name const &name,
	                           Parent::Client::Id          id,
	                           Parent::Session_args const &args,
	                           Affinity             const &affinity) override
	{
		if (!args.valid_string()) {
			warning(name.string(), " session denied because of truncated arguments");
			return Session_error::DENIED;
		}

		Mutex::Guard guard(_mutex);

		return _parent.session(id, name, args, affinity).convert<Session_result>(
			[&] (Capability<Session> cap) -> Session_result {
				if (cap.valid())
					return cap;

				_block_for_session();

				return _parent.session_cap(id).convert<Session_result>(
					[&] (Capability<Session> cap) { return cap; },
					[&] (Parent::Session_cap_error const e) -> Session_result {
						using Error = Parent::Session_cap_error;
						switch (e) {
						case Error::INSUFFICIENT_RAM:  return Session_error::INSUFFICIENT_RAM;
						case Error::INSUFFICIENT_CAPS: return Session_error::INSUFFICIENT_CAPS;
						case Error::DENIED:            break;
						}
						return Session_error::DENIED; }
				);
			},
			[&] (Session_error e) { return e; });
	}

	Session_capability session(Parent::Service_name const &name,
	                           Parent::Client::Id          id,
	                           Parent::Session_args const &args,
	                           Affinity             const &affinity) override
	{
		Session_capability result { };

		/*
		 * Since we account for the backing store for session meta data on
		 * the route between client and server, the session quota provided
		 * by the client may become successively diminished by intermediate
		 * components, prompting the server to deny the session request.
		 */

		/* extract session quota as specified by the 'Connection' */
		char argbuf[Parent::Session_args::MAX_SIZE];
		copy_cstring(argbuf, args.string(), sizeof(argbuf));

		Ram_quota ram_quota = ram_quota_from_args(argbuf);
		Cap_quota cap_quota = cap_quota_from_args(argbuf);

		unsigned warn_after_attempts = 2;

		for (unsigned cnt = 0;; cnt++) {

			Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota",
			                    String<32>(ram_quota).string());

			Arg_string::set_arg(argbuf, sizeof(argbuf), "cap_quota",
			                    String<32>(cap_quota).string());

			try_session(name, id, Parent::Session_args(argbuf), affinity).with_result(
				[&] (Session_capability cap) { result = cap; },
				[&] (Session_error e) {
					switch (e) {
					case Session_error::OUT_OF_RAM:
						if (ram_quota.value > pd().avail_ram().value) {
							Parent::Resource_args args(String<64>("ram_quota=", ram_quota));
							_parent.resource_request(args);
						}
						break;

					case Session_error::OUT_OF_CAPS:
						if (cap_quota.value > pd().avail_caps().value) {
							Parent::Resource_args args(String<64>("cap_quota=", cap_quota));
							_parent.resource_request(args);
						}
						break;

					case Session_error::DENIED:
						error("stop because parent denied ", name.string(), "-session: ", Cstring(argbuf));
						sleep_forever();

					case Session_error::INSUFFICIENT_RAM:
						ram_quota = Ram_quota { ram_quota.value + 4096 };
						break;

					case Session_error::INSUFFICIENT_CAPS:
						cap_quota = Cap_quota { cap_quota.value + 4 };
						break;
					}
				}
			);

			if (result.valid())
				return result;

			if (cnt == warn_after_attempts) {
				warning("re-attempted ", name.string(), " session request ",
				        cnt, " times (args: ", Cstring(argbuf), ")");
				warn_after_attempts *= 2;
			}
		}
	}

	void upgrade(Parent::Client::Id id, Parent::Upgrade_args const &args) override
	{
		Mutex::Guard guard(_mutex);

		if (_parent.upgrade(id, args) == Parent::Upgrade_result::PENDING)
			_block_for_session();
	}

	void close(Parent::Client::Id id) override
	{
		Mutex::Guard guard(_mutex);

		if (_parent.close(id) == Parent::Close_result::PENDING)
			_block_for_session();
	}

	void exec_static_constructors() override
	{
		Genode::exec_static_constructors();
	}
};


size_t Component::stack_size() __attribute__((weak));
size_t Component::stack_size() { return 64*1024; }


/**
 * Init program headers of the dynamic linker
 *
 * The weak function is used for statically linked binaries. The
 * dynamic linker provides an implementation that loads the program
 * headers of the linker. This must be done before the first exception
 * is thrown.
 */
void Genode::init_ldso_phdr(Env &) __attribute__((weak));
void Genode::init_ldso_phdr(Env &) { }


/*
 * We need to execute the constructor of the main entrypoint from a
 * class called 'Startup' as 'Startup' is a friend of 'Entrypoint'.
 */
namespace Genode { struct Startup; }


struct Genode::Startup
{
	Component_env env;

	/*
	 * 'init_ldso_phdr' must be called before 'init_exception_handling' because
	 * the initial exception thrown by 'init_exception_handling' involves the
	 * linker's 'dl_iterate_phdr' function.
	 */
	bool const ldso_phdr = (init_ldso_phdr(env), true);
	bool const exception = (init_exception_handling(env.ram(), env.rm()), true);

	/*
	 * The construction of the main entrypoint does never return.
	 */
	Entrypoint ep { env };

	Startup(Runtime &runtime) : env(runtime, ep) { }
};


void Genode::bootstrap_component(Runtime &runtime)
{
	static Startup startup { runtime };

	/* never reached */
}


size_t Genode::Ram_allocator::_legacy_dataspace_size(Dataspace_capability ds)
{
	return Dataspace_client(ds).size();
}
