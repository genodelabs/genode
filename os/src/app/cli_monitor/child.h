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

#ifndef _CHILD_H_
#define _CHILD_H_

/* Genode includes */
#include <util/list.h>
#include <base/child.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>

class Child : public List<Child>::Element, Genode::Child_policy
{
	public:

		/*
		 * XXX derive donated quota from information to be provided by
		 *     the used 'Connection' interfaces
		 */
		enum { DONATED_RAM_QUOTA = 128*1024 };

		class Quota_exceeded : public Genode::Exception { };

		Argument const argument;

	private:

		Ram &_ram;

		struct Label
		{
			enum { LABEL_MAX_LEN = 128 };
			char buf[LABEL_MAX_LEN];
			Label(char const *label) { strncpy(buf, label, sizeof(buf)); }
		};

		Label const _label;

		struct Resources
		{
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;
			Genode::Rm_connection  rm;

			Resources(const char *label, Genode::size_t ram_quota)
			: ram(label), cpu(label)
			{
				if (ram_quota >  DONATED_RAM_QUOTA)
					ram_quota -= DONATED_RAM_QUOTA;
				else
					throw Quota_exceeded();
				ram.ref_account(Genode::env()->ram_session_cap());
				if (Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota) != 0)
					throw Quota_exceeded();
			}
		};

		size_t                   _ram_quota;
		size_t                   _ram_limit;
		Resources                _resources;
		Genode::Service_registry _parent_services;
		Genode::Rom_connection   _binary_rom;

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

	public:

		Child(Ram                              &ram,
		      char                       const *label,
		      char                       const *binary,
		      Genode::Cap_session              &cap_session,
		      Genode::size_t                    ram_quota,
		      Genode::size_t                    ram_limit,
		      Genode::Signal_context_capability yield_response_sig_cap)
		:
			argument(label, "subsystem"),
			_ram(ram),
			_label(label),
			_ram_quota(ram_quota),
			_ram_limit(ram_limit),
			_resources(_label.buf, _ram_quota),
			_binary_rom(binary, _label.buf),
			_entrypoint(&cap_session, ENTRYPOINT_STACK_SIZE, _label.buf, false),
			_labeling_policy(_label.buf),
			_binary_policy("binary", _binary_rom.dataspace(), &_entrypoint),
			_config_policy("config", _entrypoint, &_resources.ram),
			_child(_binary_rom.dataspace(),
			       _resources.ram.cap(), _resources.cpu.cap(),
			       _resources.rm.cap(), &_entrypoint, this),
			_yield_response_sigh_cap(yield_response_sig_cap)
		{ }

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
			char buf[128];
			snprintf(buf, sizeof(buf), "ram_quota=%zd", amount);
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
		 * This function evaluates the conditions, under which a resource
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
		 * XXX should be a const function, but the 'Ram_session' accessors
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


		/****************************
		 ** Child_policy interface **
		 ****************************/

		const char *name() const { return _label.buf; }

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

				/* try to immediately withdraw freed-up resources */
				try { withdraw_ram_quota(_resources.ram.avail()); }
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
};

#endif /* _CHILD_H_ */
