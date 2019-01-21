/*
 * \brief  Report-ROM slave
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_
#define _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_

/* Genode includes */
#include <base/lock.h>
#include <os/static_parent_services.h>
#include <os/slave.h>
#include <report_session/connection.h>
#include <rom_session/connection.h>


class Report_rom_slave : public Genode::Noncopyable
{
	private:

		struct Policy_base
		{
			Genode::Static_parent_services<Genode::Rom_session,
			                               Genode::Cpu_session,
			                               Genode::Pd_session,
			                               Genode::Log_session>
				_parent_services;

			Policy_base(Genode::Env &env) : _parent_services(env) { }
		};

		class Policy : Policy_base, public Genode::Slave::Policy
		{
			private:

				Genode::Root_capability _report_root_cap;
				Genode::Root_capability _rom_root_cap;
				bool                    _announced;

				static Name              _name()  { return "report_rom"; }
				static Genode::Ram_quota _quota() { return { 1024*1024 }; }
				static Genode::Cap_quota _caps()  { return { 75 }; }

			public:

				Policy(Genode::Env                   &env,
				       Genode::Rpc_entrypoint        &ep,
				       const char                    *config)
				:
					Policy_base(env),
					Genode::Slave::Policy(env, _name(), _name(),
					                      Policy_base::_parent_services,
					                      ep, _caps(), _quota())
				{
					if (config)
						configure(config);
				}
		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::Child          _child;

	public:

		/**
		 * Constructor
		 *
		 * \param ep   entrypoint used for child thread
		 */
		Report_rom_slave(Genode::Env &env, char const *config)
		:
			_ep(&env.pd(), _ep_stack_size, "report_rom"),
			_policy(env, _ep, config),
			_child(env.rm(), _ep, _policy)
		{ }

		Genode::Slave::Policy &policy() { return _policy; }
};

#endif /* _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_ */
