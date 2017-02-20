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

/*
 * XXX remove this pointer once 'Env_deprecated' is removed
 */
static Genode::Env *env_ptr = nullptr;

namespace {

	using namespace Genode;

	struct Env : Genode::Env
	{
		Genode::Entrypoint &_ep;

		Genode::Parent &_parent = *env_deprecated()->parent();

		/**
		 * Lock for serializing 'session' and 'close'
		 */
		Genode::Lock _lock;

		/**
		 * Utility to used block for single signal
		 */
		struct Blockade
		{
			Parent                 &_parent;
			Genode::Signal_receiver _sig_rec;
			Genode::Signal_context  _sig_ctx;

			Blockade(Parent &parent) : _parent(parent)
			{
				_parent.session_sigh(_sig_rec.manage(&_sig_ctx));
			}

			void block() { _sig_rec.wait_for_signal(); }
		};

		Constructible<Blockade> _session_blockade;

		Env(Genode::Entrypoint &ep) : _ep(ep) { env_ptr = this; }

		Genode::Parent      &parent() override { return _parent; }
		Genode::Ram_session &ram()    override { return *Genode::env_deprecated()->ram_session(); }
		Genode::Cpu_session &cpu()    override { return *Genode::env_deprecated()->cpu_session(); }
		Genode::Region_map  &rm()     override { return *Genode::env_deprecated()->rm_session(); }
		Genode::Pd_session  &pd()     override { return *Genode::env_deprecated()->pd_session(); }
		Genode::Entrypoint  &ep()     override { return _ep; }

		Genode::Ram_session_capability ram_session_cap() override
		{
			return Genode::env_deprecated()->ram_session_cap();
		}

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

		Session_capability session(Parent::Service_name const &name,
		                           Parent::Client::Id          id,
		                           Parent::Session_args const &args,
		                           Affinity             const &affinity) override
		{
			Lock::Guard guard(_lock);

			/*
			 * Since we account for the backing store for session meta data on
			 * the route between client and server, the session quota provided
			 * by the client may become successively diminished by intermediate
			 * components, prompting the server to deny the session request.
			 *
			 * If the session creation failed due to insufficient session
			 * quota, we try to repeatedly increase the quota up to
			 * 'NUM_ATTEMPTS'.
			 */
			enum { NUM_ATTEMPTS = 10 };

			/* extract session quota as specified by the 'Connection' */
			char argbuf[Parent::Session_args::MAX_SIZE];
			strncpy(argbuf, args.string(), sizeof(argbuf));
			size_t ram_quota = Arg_string::find_arg(argbuf, "ram_quota").ulong_value(0);

			return retry<Parent::Quota_exceeded>([&] () {

				Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota",
				                    String<32>(Number_of_bytes(ram_quota)).string());

				Session_capability cap =
					_parent.session(id, name, Parent::Session_args(argbuf), affinity);

				if (cap.valid())
					return cap;

				_block_for_session();
				return _parent.session_cap(id);
			},
			[&] () {
					/*
					 * If our RAM session has less quota available than the
					 * session quota, the session-quota transfer failed. In
					 * this case, we try to recover by issuing a resource
					 * request to the parent.
					 *
					 * Otherwise, the session-quota transfer succeeded but
					 * the request was denied by the server.
					 */
					if (ram_quota > ram().avail()) {

						/* issue resource request */
						char buf[128];
						snprintf(buf, sizeof(buf), "ram_quota=%lu", ram_quota);

						_parent.resource_request(Parent::Resource_args(buf));
					} else {
						ram_quota += 4096;
					}

			}, NUM_ATTEMPTS);

			warning("giving up to increase session quota for ", name.string(), " session "
			        "after ", (int)NUM_ATTEMPTS, " attempts");

			throw Parent::Quota_exceeded();
		}

		void upgrade(Parent::Client::Id id, Parent::Upgrade_args const &args) override
		{
			Lock::Guard guard(_lock);

			if (_parent.upgrade(id, args) == Parent::UPGRADE_PENDING)
				_block_for_session();
		}

		void close(Parent::Client::Id id) override
		{
			Lock::Guard guard(_lock);

			if (_parent.close(id) == Parent::CLOSE_PENDING)
				_block_for_session();
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


/*
 * We need to execute the constructor of the main entrypoint from a
 * class called 'Startup' as 'Startup' is a friend of 'Entrypoint'.
 */
struct Genode::Startup
{
	::Env env { ep };

	bool const exception_handling = (init_exception_handling(env), true);

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
