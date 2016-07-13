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

	private:

		Ram &_ram;

		Genode::Session_label const _label;

		size_t _ram_quota;
		size_t _ram_limit;

		struct Resources
		{
			Genode::Pd_connection  pd;
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;

			Resources(const char *label, Genode::size_t ram_quota)
			: pd(label), ram(label), cpu(label)
			{
				if (ram_quota >  DONATED_RAM_QUOTA)
					ram_quota -= DONATED_RAM_QUOTA;
				else
					throw Quota_exceeded();
				ram.ref_account(Genode::env()->ram_session_cap());
				if (Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota) != 0)
					throw Quota_exceeded();
			}
		} _resources;

		Genode::Child::Initial_thread _initial_thread { _resources.cpu, _resources.pd,
		                                                _label.string() };

		Genode::Region_map_client _address_space { _resources.pd.address_space() };
		Genode::Service_registry  _parent_services;
		Genode::Rom_connection    _binary_rom;

		enum { ENTRYPOINT_STACK_SIZE = 12*1024 };
		Genode::Rpc_entrypoint _entrypoint;

		Init::Child_policy_enforce_labeling   _labeling_policy;
		Init::Child_policy_provide_rom_file   _binary_policy;
		Genode::Child_policy_dynamic_rom_file _config_policy;
		Genode::Child                         _child;

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

	public:

		Child_base(Ram                              &ram,
		           char                       const *label,
		           char                       const *binary,
		           Genode::Cap_session              &cap_session,
		           Genode::size_t                    ram_quota,
		           Genode::size_t                    ram_limit,
		           Genode::Signal_context_capability yield_response_sig_cap,
		           Genode::Signal_context_capability exit_sig_cap,
		           Genode::Dataspace_capability      ldso_ds)
		:
			_ram(ram),
			_label(label),
			_ram_quota(ram_quota),
			_ram_limit(ram_limit),
			_resources(_label.string(), _ram_quota),
			_binary_rom(Genode::prefixed_label(Genode::Session_label(label),
			                                   Genode::Session_label(binary)).string()),
			_entrypoint(&cap_session, ENTRYPOINT_STACK_SIZE, _label.string(), false),
			_labeling_policy(_label.string()),
			_binary_policy("binary", _binary_rom.dataspace(), &_entrypoint),
			_config_policy("config", _entrypoint, &_resources.ram),
			_child(_binary_rom.dataspace(), ldso_ds, _resources.pd, _resources.pd,
			       _resources.ram, _resources.ram, _resources.cpu, _initial_thread,
			       *Genode::env()->rm_session(), _address_space,
			       _entrypoint, *this),
			_yield_response_sigh_cap(yield_response_sig_cap),
			_exit_sig_cap(exit_sig_cap)
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
			Genode::snprintf(buf, sizeof(buf), "ram_quota=%zd", amount);
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

			_ram.withdraw_from(_resources.ram.cap(), amount);
			_ram_quota -= amount;
		}

		/**
		 * Upgrade quota of child
		 *
		 * \throw Ram::Transfer_quota_failed
		 */
		void upgrade_ram_quota(size_t amount)
		{
			_ram.transfer_to(_resources.ram.cap(), amount);
			_ram_quota += amount;

			/* wake up child if resource request is in flight */
			size_t const req = requested_ram_quota();
			if (req && _resources.ram.avail() >= req) {
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
			                  _ram_quota - _resources.ram.quota(),
			                  _resources.ram.used(),
			                  _resources.ram.avail(),
			                  requested_ram_quota());
		}

		/**
		 * Return true if child exited and should be destructed
		 */
		bool exited() const { return _exited; }


		/****************************
		 ** Child_policy interface **
		 ****************************/

		const char *name() const { return _label.string(); }

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			Genode::Service *service = 0;

			/* check for binary file request */
			if ((service = _binary_policy.resolve_session_request(service_name, args)))
				return service;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(service_name, args)))
				return service;

			/* fill parent service registry on demand */
			if (!(service = _parent_services.find(service_name))) {
				service = new (Genode::env()->heap())
				          Genode::Parent_service(service_name);
				_parent_services.insert(service);
			}

			/* return parent service */
			return service;
		}

		void filter_session_args(const char *service,
		                         char *args, Genode::size_t args_len)
		{
			_labeling_policy.filter_session_args(service, args, args_len);
		}

		void yield_response()
		{
			if (_withdraw_on_yield_response) {
				enum { RESERVE = 4*1024*1024 };

				size_t amount = _resources.ram.avail() < RESERVE
				                ? 0 : _resources.ram.avail() - RESERVE;

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
