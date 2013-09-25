/*
 * \brief  Platform environment of Genode process
 * \author Norman Feske
 * \date   2013-09-25
 *
 * Parts of 'Platform_env' shared accross all base platforms.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_ENV_COMMON_H_
#define _PLATFORM_ENV_COMMON_H_

#include <base/env.h>
#include <util/arg_string.h>
#include <parent/client.h>
#include <ram_session/client.h>
#include <rm_session/client.h>
#include <cpu_session/client.h>
#include <pd_session/client.h>

namespace Genode {
	class Expanding_rm_session_client;
	class Expanding_ram_session_client;
	class Expanding_cpu_session_client;
	class Expanding_parent_client;
}


/**
 * Repeatedly try to execute a function 'func'
 *
 * If the function 'func' throws an exception of type 'EXC', the 'handler'
 * is called and the function call is retried.
 *
 * \param EXC       exception type to handle
 * \param func      functor to execute
 * \param handler   exception handler executed if 'func' raised an exception
 *                  of type 'EXC'
 * \param attempts  number of attempts to execute 'func' before giving up
 *                  and reflecting the exception 'EXC' to the caller. If not
 *                  specified, attempt infinitely.
 */
template <typename EXC, typename FUNC, typename HANDLER>
auto retry(FUNC func, HANDLER handler, unsigned attempts = ~0U) -> decltype(func())
{
	for (unsigned i = 0; attempts == ~0U || i < attempts; i++)
		try { return func(); }
		catch (EXC) { handler(); }

	throw EXC();
}


/**
 * Client object for a session that may get its session quota upgraded
 */
template <typename CLIENT>
struct Upgradeable_client : CLIENT
{
	typedef Genode::Capability<typename CLIENT::Rpc_interface> Capability;

	Capability _cap;

	Upgradeable_client(Capability cap) : CLIENT(cap), _cap(cap) { }

	void upgrade_ram(Genode::size_t quota)
	{
		PINF("upgrading quota donation for Env::%s (%zd bytes)",
		     CLIENT::Rpc_interface::service_name(), quota);

		char buf[128];
		Genode::snprintf(buf, sizeof(buf), "ram_quota=%zd", quota);

		Genode::env()->parent()->upgrade(_cap, buf);
	}
};


struct Genode::Expanding_ram_session_client : Upgradeable_client<Genode::Ram_session_client>
{
	Expanding_ram_session_client(Ram_session_capability cap)
	: Upgradeable_client<Genode::Ram_session_client>(cap) { }

	Ram_dataspace_capability alloc(size_t size, bool cached = true)
	{
		/*
		 * If the RAM session runs out of quota, issue a resource request
		 * to the parent and retry.
		 */
		enum { NUM_ATTEMPTS = 2 };
		return retry<Ram_session::Quota_exceeded>(
			[&] () {
				/*
				 * If the RAM session runs out of meta data, upgrade the
				 * session quota and retry.
				 */
				return retry<Ram_session::Out_of_metadata>(
					[&] () { return Ram_session_client::alloc(size, cached); },
					[&] () { upgrade_ram(8*1024); });
			},
			[&] () {
				char buf[128];

				/*
				 * The RAM service withdraws the meta data for the allocator
				 * from the RAM quota. In the worst case, a new slab block
				 * may be needed. To cover the worst case, we need to take
				 * this possible overhead into account when requesting
				 * additional RAM quota from the parent.
				 *
				 * Because the worst case almost never happens, we request
				 * a bit too much quota for the most time.
				 */
				enum { ALLOC_OVERHEAD = 1024U };
				Genode::snprintf(buf, sizeof(buf), "ram_quota=%zd",
				                 size + ALLOC_OVERHEAD);
				env()->parent()->resource_request(buf);
			},
			NUM_ATTEMPTS);
	}

	int transfer_quota(Ram_session_capability ram_session, size_t amount)
	{
		enum { NUM_ATTEMPTS = 2 };
		int ret = -1;
		for (unsigned i = 0; i < NUM_ATTEMPTS; i++) {

			ret = Ram_session_client::transfer_quota(ram_session, amount);
			if (ret != -3) break;

			/*
			 * The transfer failed because we don't have enough quota. Request
			 * the needed amount from the parent.
			 *
			 * XXX Let transfer_quota throw 'Ram_session::Quota_exceeded'
			 */
			char buf[128];
			Genode::snprintf(buf, sizeof(buf), "ram_quota=%zd", amount);
			env()->parent()->resource_request(buf);
		}
		return ret;
	}
};


struct Emergency_ram_reserve
{
	virtual void release() = 0;
};


class Genode::Expanding_parent_client : public Parent_client
{
	private:

		/**
		 * Signal handler state
		 *
		 * UNDEFINED        - No signal handler is effective. If we issue a
		 *                    resource request, use our built-in fallback
		 *                    signal handler.
		 * BLOCKING_DEFAULT - The fallback signal handler is effective.
		 *                    When using this handler, we block for a
		 *                    for a response to a resource request.
		 * CUSTOM           - A custom signal handler was registered. Calls
		 *                    of 'resource_request' won't block.
		 */
		enum State { UNDEFINED, BLOCKING_DEFAULT, CUSTOM };
		State _state = { UNDEFINED };

		/**
		 * Lock used to serialize resource requests
		 */
		Lock _lock;

		/**
		 * Return signal context capability for the fallback signal handler
		 */
		Signal_context_capability _fallback_sig_cap();

		/**
		 * Block for resource response arriving at the fallback signal handler
		 */
		static void _wait_for_resource_response();

		/**
		 * Emergency RAM reserve for constructing the fallback signal handler
		 *
		 * See the comment of '_fallback_sig_cap()' in 'env/env.cc'.
		 */
		Emergency_ram_reserve &_emergency_ram_reserve;

	public:

		Expanding_parent_client(Parent_capability cap,
		                        Emergency_ram_reserve &emergency_ram_reserve)
		:
			Parent_client(cap), _emergency_ram_reserve(emergency_ram_reserve)
		{ }


		/**********************
		 ** Parent interface **
		 **********************/

		Session_capability session(Service_name const &name,
		                           Session_args const &args,
		                           Affinity     const &affinity)
		{
			enum { NUM_ATTEMPTS = 2 };
			return retry<Parent::Quota_exceeded>(
				[&] () { return Parent_client::session(name, args, affinity); },
				[&] () {

					/*
					 * Request amount of session quota from the parent.
					 *
					 * XXX We could deduce the available quota of our
					 *     own RAM session from the request.
					 */
					size_t const ram_quota =
						Arg_string::find_arg(args.string(), "ram_quota")
							.ulong_value(0);

					char buf[128];
					snprintf(buf, sizeof(buf), "ram_quota=%zd", ram_quota);

					resource_request(Resource_args(buf));
				},
				NUM_ATTEMPTS);
		}

		void upgrade(Session_capability to_session, Upgrade_args const &args)
		{
			/*
			 * If the upgrade fails, attempt to issue a resource request twice.
			 *
			 * If the default fallback for resource-available signals is used,
			 * the first request will block until the resources are upgraded.
			 * The second attempt to upgrade will succeed.
			 *
			 * If a custom handler is installed, the resource quest will return
			 * immediately. The second upgrade attempt may fail too if the
			 * parent handles the resource request asynchronously. In this
			 * case, we escalate the problem to caller by propagating the
			 * 'Parent::Quota_exceeded' exception. Now, it is the job of the
			 * caller to issue (and respond to) a resource request.
			 */
			enum { NUM_ATTEMPTS = 2 };
			retry<Parent::Quota_exceeded>(
				[&] () { Parent_client::upgrade(to_session, args); },
				[&] () { resource_request(Resource_args(args.string())); },
				NUM_ATTEMPTS);
		}

		void resource_avail_sigh(Signal_context_capability sigh)
		{
			Lock::Guard guard(_lock);

			/*
			 * If signal hander gets de-installed, let the next call of
			 * 'resource_request' install the fallback signal handler.
			 */
			if (_state == CUSTOM && !sigh.valid())
				_state = UNDEFINED;

			/*
			 * Forward information about a custom signal handler and remember
			 * state to avoid blocking in 'resource_request'.
			 */
			if (sigh.valid()) {
				_state = CUSTOM;
				Parent_client::resource_avail_sigh(sigh);
			}
		}

		void resource_request(Resource_args const &args)
		{
			Lock::Guard guard(_lock);

			PLOG("resource_request: %s", args.string());

			/*
			 * Issue request but don't block if a custom signal handler is
			 * installed.
			 */
			if (_state == CUSTOM) {
				Parent_client::resource_request(args);
				return;
			}

			/*
			 * Install fallback signal handler not yet installed.
			 */
			if (_state == UNDEFINED) {
				Parent_client::resource_avail_sigh(_fallback_sig_cap());
				_state = BLOCKING_DEFAULT;
			}

			/*
			 * Issue resource request
			 */
			Parent_client::resource_request(args);

			/*
			 * Block until we get a response for the outstanding resource
			 * request.
			 */
			if (_state == BLOCKING_DEFAULT)
				_wait_for_resource_response();
		}
};


#endif /* _PLATFORM_ENV_COMMON_H_ */
