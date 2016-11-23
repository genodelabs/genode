/*
 * \brief  Report-ROM slave
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_
#define _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_

/* Genode includes */
#include <base/lock.h>
#include <os/slave.h>
#include <report_session/connection.h>
#include <rom_session/connection.h>


class Report_rom_slave : public Genode::Noncopyable
{
	private:

		class Policy : public Genode::Slave::Policy
		{
			private:

				Genode::Root_capability _report_root_cap;
				Genode::Root_capability _rom_root_cap;
				bool                    _announced;

			protected:

				char const **_permitted_services() const
				{
					static char const *permitted_services[] = {
						"CPU", "PD", "RAM", "LOG", 0 };

					return permitted_services;
				};

				static Name           _name()  { return "report_rom"; }
				static Genode::size_t _quota() { return 1024*1024; }

			public:

				Policy(Genode::Rpc_entrypoint        &ep,
				       Genode::Region_map            &rm,
				       Genode::Ram_session_capability ram,
				       const char                    *config)
				:
					Genode::Slave::Policy(_name(), _name(), ep, rm, ram, _quota())
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
		 * \param ram  RAM session used to allocate the configuration
		 *             dataspace
		 */
		Report_rom_slave(Genode::Pd_session             &pd,
		                 Genode::Region_map             &rm,
		                 Genode::Ram_session_capability  ram,
		                 char                     const *config)
		:
			_ep(&pd, _ep_stack_size, "report_rom"),
			_policy(_ep, rm, ram, config),
			_child(rm, _ep, _policy)
		{ }

		Genode::Slave::Policy &policy() { return _policy; }
};

#endif /* _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_ */
