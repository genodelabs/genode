/*
 * \brief  Parent client that issues resource requests on demand
 * \author Norman Feske
 * \date   2013-09-25
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__EXPANDING_PARENT_CLIENT_H_
#define _INCLUDE__BASE__INTERNAL__EXPANDING_PARENT_CLIENT_H_

/* Genode includes */
#include <base/signal.h>
#include <util/arg_string.h>
#include <util/retry.h>
#include <parent/client.h>

/* base-internal includes */
#include <base/internal/upgradeable_client.h>

namespace Genode { class Expanding_parent_client; }


class Genode::Expanding_parent_client : public Parent_client
{
	public:

		struct Emergency_ram_reserve
		{
			virtual void release() = 0;
		};

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
		                           Affinity     const &affinity) override
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
					snprintf(buf, sizeof(buf), "ram_quota=%lu", ram_quota);

					resource_request(Resource_args(buf));
				},
				NUM_ATTEMPTS);
		}

		void upgrade(Session_capability to_session, Upgrade_args const &args) override
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

		void resource_avail_sigh(Signal_context_capability sigh) override
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

		void resource_request(Resource_args const &args) override
		{
			Lock::Guard guard(_lock);

			log("resource_request: ", args.string());

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

#endif /* _INCLUDE__BASE__INTERNAL__EXPANDING_PARENT_CLIENT_H_ */
