/*
 * \brief  Child handling
 * \author Norman Feske
 * \date   2013-10-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CLI_MONITOR__CHILD_H_
#define _INCLUDE__CLI_MONITOR__CHILD_H_

/* Genode includes */
#include <util/list.h>
#include <base/registry.h>
#include <base/child.h>
#include <init/child_policy.h>
#include <os/child_policy_dynamic_rom.h>
#include <cpu_session/connection.h>
#include <pd_session/connection.h>
#include <base/session_label.h>

/* CLI-monitor includes */
#include <cli_monitor/ram.h>


class Child_base : public Genode::Child_policy
{
	public:

		/*
		 * XXX derive donated quota from information to be provided by
		 *     the used 'Connection' interfaces
		 */
		enum { DONATED_RAM_QUOTA = 128*1024 };

		class Quota_exceeded : public Genode::Exception { };

		typedef Genode::size_t size_t;

		typedef Genode::Registered<Genode::Parent_service> Parent_service;
		typedef Genode::Registry<Parent_service>           Parent_services;

	private:

		Ram &_ram;

		Genode::Session_label const _label;
		Binary_name           const _binary_name;

		Genode::Ram_session_capability _ref_ram_cap;
		Genode::Ram_session           &_ref_ram;

		size_t _ram_quota;
		size_t _ram_limit;

		Parent_services _parent_services;

		enum { ENTRYPOINT_STACK_SIZE = 12*1024 };
		Genode::Rpc_entrypoint _entrypoint;

		Genode::Child_policy_dynamic_rom_file _config_policy;

		/**
		 * If set to true, immediately withdraw resources yielded by the child
		 */
		bool _withdraw_on_yield_response = false;

		/**
		 * Arguments of current resource request from the child
		 */
		Genode::Parent::Resource_args _resource_args;

		Genode::Signal_context_capability _yield_response_sigh_cap;

		Genode::Signal_context_capability _exit_sig_cap;

		/* true if child is scheduled for destruction */
		bool _exited = false;

		Genode::Child _child;

	public:

		/**
		 * Constructor
		 *
		 * \param ref_ram  used as reference account for the child'd RAM
		 *                 session and for allocating the backing store
		 *                 for the child's configuration
		 */
		Child_base(Ram                              &ram,
		           Name                       const &label,
		           Binary_name                const &binary_name,
		           Genode::Pd_session               &pd_session,
		           Genode::Ram_session              &ref_ram,
		           Genode::Ram_session_capability    ref_ram_cap,
		           Genode::Region_map               &local_rm,
		           Genode::size_t                    ram_quota,
		           Genode::size_t                    ram_limit,
		           Genode::Signal_context_capability yield_response_sig_cap,
		           Genode::Signal_context_capability exit_sig_cap)
		:
			_ram(ram),
			_label(label), _binary_name(binary_name),
			_ref_ram_cap(ref_ram_cap), _ref_ram(ref_ram),
			_ram_quota(ram_quota), _ram_limit(ram_limit),
			_entrypoint(&pd_session, ENTRYPOINT_STACK_SIZE, _label.string(), false),
			_config_policy("config", _entrypoint, &ref_ram),
			_yield_response_sigh_cap(yield_response_sig_cap),
			_exit_sig_cap(exit_sig_cap),
			_child(local_rm, _entrypoint, *this)
		{ }

		Genode::Session_label label() const { return _label; }

		void configure(char const *config, size_t config_len)
		{
			_config_policy.load(config, config_len);
		}

		void start()
		{
			_entrypoint.activate();
		}

		/**
		 * Issue yield request to the child
		 */
		void yield(size_t amount, bool greedy)
		{
			if (requested_ram_quota())
				return; /* resource request in flight */

			char buf[128];
			Genode::snprintf(buf, sizeof(buf), "ram_quota=%ld", amount);
			_withdraw_on_yield_response = greedy;
			_child.yield(buf);
		}

		/**
		 * Return amount of RAM currently requested by the child
		 */
		size_t requested_ram_quota() const
		{
			return Genode::Arg_string::find_arg(_resource_args.string(), "ram_quota").ulong_value(0);
		}

		/**
		 * Withdraw quota from the child
		 *
		 * \throw Ram::Transfer_quota_failed
		 */
		void withdraw_ram_quota(size_t amount)
		{
			if (!amount)
				return;

			_ram.withdraw_from(_child.ram_session_cap(), amount);
			_ram_quota -= amount;
		}

		/**
		 * Upgrade quota of child
		 *
		 * \throw Ram::Transfer_quota_failed
		 */
		void upgrade_ram_quota(size_t amount)
		{
			_ram.transfer_to(_child.ram_session_cap(), amount);
			_ram_quota += amount;

			/* wake up child if resource request is in flight */
			size_t const req = requested_ram_quota();
			if (req && _child.ram().avail() >= req) {
				_child.notify_resource_avail();

				/* clear request state */
				_resource_args = Genode::Parent::Resource_args("");
			}
		}

		/**
		 * Try to respond to a current resource request issued by the child
		 *
		 * This method evaluates the conditions, under which a resource
		 * request can be answered: There must be enough room between the
		 * current quota and the configured limit, and there must be enough
		 * slack memory available. If both conditions are met, the quota
		 * of the child gets upgraded.
		 */
		void try_response_to_resource_request()
		{
			size_t const req = requested_ram_quota();

			if (!req)
				return; /* no resource request in flight */

			/*
			 * Respond to the current request if the requested quota fits
			 * within the limit and if there is enough free quota available.
			 */
			if (req <= _ram.status().avail && req + _ram_quota <= _ram_limit) {
				try { upgrade_ram_quota(req); }
				catch (Ram::Transfer_quota_failed) { }
			}
		}

		/**
		 * Set limit for on-demand RAM quota expansion
		 */
		void ram_limit(size_t limit)
		{
			_ram_limit = limit;
			try_response_to_resource_request();
		}

		struct Ram_status
		{
			size_t quota = 0, limit = 0, xfer = 0, used = 0, avail = 0, req = 0;

			Ram_status() { }
			Ram_status(size_t quota, size_t limit, size_t xfer, size_t used,
			           size_t avail, size_t req)
			:
				quota(quota), limit(limit), xfer(xfer), used(used),
				avail(avail), req(req)
			{ }
		};

		/**
		 * Return RAM quota status of the child
		 *
		 * XXX should be a const method, but the 'Ram_session' accessors
		 *     are not const
		 */
		Ram_status ram_status()
		{
			return Ram_status(_ram_quota,
			                  _ram_limit,
			                  _ram_quota - _child.ram().quota(),
			                  _child.ram().used(),
			                  _child.ram().avail(),
			                  requested_ram_quota());
		}

		/**
		 * Return true if child exited and should be destructed
		 */
		bool exited() const { return _exited; }


		/****************************
		 ** Child_policy interface **
		 ****************************/

		Name        name()        const override { return _label; }
		Binary_name binary_name() const override { return _binary_name; }

		Genode::Ram_session_capability ref_ram_cap() const override { return _ref_ram_cap; }
		Genode::Ram_session           &ref_ram()           override { return _ref_ram; }

		void init(Genode::Ram_session &session, Genode::Ram_session_capability cap) override
		{
			session.ref_account(_ref_ram_cap);
			_ref_ram.transfer_quota(cap, _ram_quota);
		}

		Genode::Service &resolve_session_request(Genode::Service::Name const &name,
		                                         Genode::Session_state::Args const &args) override
		{
			Genode::Service *service = nullptr;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(name.string(), args.string())))
				return *service;

			/* populate session-local parent service registry on demand */
			_parent_services.for_each([&] (Parent_service &s) {
				if (s.name() == name)
					service = &s; });

			if (service)
				return *service;

			return *new (Genode::env()->heap()) Parent_service(_parent_services, name);
		}

		void yield_response()
		{
			if (_withdraw_on_yield_response) {
				enum { RESERVE = 4*1024*1024 };

				size_t amount = _child.ram().avail() < RESERVE
				                ? 0 : _child.ram().avail() - RESERVE;

				/* try to immediately withdraw freed-up resources */
				try { withdraw_ram_quota(amount); }
				catch (Ram::Transfer_quota_failed) { }
			}

			/* propagate yield-response signal */
			Genode::Signal_transmitter(_yield_response_sigh_cap).submit();
		}

		void resource_request(Genode::Parent::Resource_args const &args)
		{
			_resource_args = args;
			try_response_to_resource_request();
		}

		void exit(int exit_value) override
		{
			Genode::log("subsystem \"", name(), "\" exited with value ", exit_value);
			_exited = true;

			/* trigger destruction of the child */
			Genode::Signal_transmitter(_exit_sig_cap).submit();
		}
};

#endif /* _INCLUDE__CLI_MONITOR__CHILD_H_ */
