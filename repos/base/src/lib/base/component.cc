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
#include <deprecated/env.h>

/* base-internal includes */
#include <base/internal/globals.h>

/*
 * XXX remove this pointer once 'Env_deprecated' is removed
 */
static Genode::Env *env_ptr = nullptr;

/**
 * Excecute pending static constructors
 *
 * The weak function is used for statically linked binaries. The dynamic linker
 * provides the real implementation for dynamically linked components.
 */
void Genode::exec_static_constructors() __attribute__((weak));
void Genode::exec_static_constructors() { }

namespace {

	using namespace Genode;

	struct Env : Genode::Env
	{
		Genode::Entrypoint &_ep;

		Genode::Parent &_parent = *env_deprecated()->parent();

		/**
		 * Mutex for serializing 'session' and 'close'
		 */
		Genode::Mutex _mutex { };

		/**
		 * Utility to used block for single signal
		 */
		struct Blockade
		{
			Parent                 &_parent;
			Genode::Signal_receiver _sig_rec { };
			Genode::Signal_context  _sig_ctx { };

			Blockade(Parent &parent) : _parent(parent)
			{
				_parent.session_sigh(_sig_rec.manage(&_sig_ctx));
			}

			void block() { _sig_rec.wait_for_signal(); }
		};

		Constructible<Blockade> _session_blockade { };

		Env(Genode::Entrypoint &ep) : _ep(ep) { env_ptr = this; }

		Genode::Parent      &parent() override { return _parent; }
		Genode::Cpu_session &cpu()    override { return *Genode::env_deprecated()->cpu_session(); }
		Genode::Region_map  &rm()     override { return *Genode::env_deprecated()->rm_session(); }
		Genode::Pd_session  &pd()     override { return *Genode::env_deprecated()->pd_session(); }
		Genode::Entrypoint  &ep()     override { return _ep; }

		Genode::Cpu_session_capability cpu_session_cap() override
		{
			return Genode::env_deprecated()->cpu_session_cap();
		}

		Genode::Pd_session_capability pd_session_cap() override
		{
			return Genode::env_deprecated()->pd_session_cap();
		}

		Genode::Id_space<Parent::Client> &id_space() override
		{
			return Genode::env_session_id_space();
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
}


namespace Genode {

	struct Startup;

	extern void bootstrap_component();

	Env &internal_env()
	{
		class Env_ptr_not_initialized { };
		if (!env_ptr)
			throw Env_ptr_not_initialized();

		return *env_ptr;
	}
}


Genode::size_t Component::stack_size() __attribute__((weak));
Genode::size_t Component::stack_size() { return 64*1024; }


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
struct Genode::Startup
{
	::Env env { ep };

	bool const ldso_phdr          = (init_ldso_phdr(env), true);
	bool const exception_handling = (init_exception_handling(env.pd(), env.rm()), true);
	bool const signal_receiver    = (init_signal_receiver(env.pd(), env.parent()), true);

	/*
	 * The construction of the main entrypoint does never return.
	 */
	Entrypoint ep { env };
};


void Genode::bootstrap_component()
{
	static Startup startup;

	/* never reached */
}
