/*
 * \brief  Component bootstrap
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2016-01-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
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

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/platform.h>

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
	Platform &_platform;

	Parent      &_parent = _platform.parent;
	Pd_session  &_pd     = _platform.pd;
	Cpu_session &_cpu    = _platform.cpu;
	Region_map  &_rm     = _platform.rm;

	Capability<Pd_session>  _pd_cap  = _platform.pd.rpc_cap();
	Capability<Cpu_session> _cpu_cap = _platform.cpu.rpc_cap();

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
			_parent.session_sigh(_sig_rec.manage(&_sig_ctx));
		}

		void block() { _sig_rec.wait_for_signal(); }
	};

	Constructible<Blockade> _session_blockade { };

	Component_env(Platform &platform, Entrypoint &ep)
	:
		_platform(platform), _ep(ep)
	{ }

	Parent      &parent() override { return _parent; }
	Cpu_session &cpu()    override { return _cpu; }
	Region_map  &rm()     override { return _rm; }
	Pd_session  &pd()     override { return _pd; }
	Entrypoint  &ep()     override { return _ep; }

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

	Session_capability try_session(Parent::Service_name const &name,
	                               Parent::Client::Id          id,
	                               Parent::Session_args const &args,
	                               Affinity             const &affinity) override
	{
		if (!args.valid_string()) {
			warning(name.string(), " session denied because of truncated arguments");
			throw Service_denied();
		}

		Mutex::Guard guard(_mutex);

		Session_capability cap = _parent.session(id, name, args, affinity);

		if (cap.valid())
			return cap;

		_block_for_session();
		return _parent.session_cap(id);
	}

	Session_capability session(Parent::Service_name const &name,
	                           Parent::Client::Id          id,
	                           Parent::Session_args const &args,
	                           Affinity             const &affinity) override
	{
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

			try {

				Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota",
				                    String<32>(ram_quota).string());

				Arg_string::set_arg(argbuf, sizeof(argbuf), "cap_quota",
				                    String<32>(cap_quota).string());

				return try_session(name, id, Parent::Session_args(argbuf), affinity);
			}

			catch (Insufficient_ram_quota) {
				ram_quota = Ram_quota { ram_quota.value + 4096 }; }

			catch (Insufficient_cap_quota) {
				cap_quota = Cap_quota { cap_quota.value + 4 }; }

			catch (Out_of_ram) {
				if (ram_quota.value > pd().avail_ram().value) {
					Parent::Resource_args args(String<64>("ram_quota=", ram_quota));
					_parent.resource_request(args);
				}
			}

			catch (Out_of_caps) {
				if (cap_quota.value > pd().avail_caps().value) {
					Parent::Resource_args args(String<64>("cap_quota=", cap_quota));
					_parent.resource_request(args);
				}
			}

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

		if (_parent.upgrade(id, args) == Parent::UPGRADE_PENDING)
			_block_for_session();
	}

	void close(Parent::Client::Id id) override
	{
		Mutex::Guard guard(_mutex);

		if (_parent.close(id) == Parent::CLOSE_PENDING)
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

	Startup(Platform &platform) : env(platform, ep) { }
};


void Genode::bootstrap_component(Platform &platform)
{
	static Startup startup { platform };

	/* never reached */
}
