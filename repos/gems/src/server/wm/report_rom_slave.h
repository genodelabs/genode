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

#ifndef _REPORT_ROM_SLAVE_H_
#define _REPORT_ROM_SLAVE_H_

/* Genode includes */
#include <base/lock.h>
#include <os/slave.h>
#include <report_session/report_session.h>
#include <rom_session/capability.h>
#include <root/client.h>


class Report_rom_slave : public Genode::Noncopyable
{
	private:

		class Policy : public Genode::Slave_policy
		{
			private:

				Genode::Root_capability _report_root_cap;
				Genode::Root_capability _rom_root_cap;
				bool                    _announced;
				Genode::Lock mutable    _lock;  /* used to wait for announcement */

			protected:

				char const **_permitted_services() const
				{
					static char const *permitted_services[] = {
						"CAP", "LOG", "SIGNAL", "RM", 0 };

					return permitted_services;
				};

			public:

				Policy(Genode::Rpc_entrypoint &entrypoint,
				       Genode::Ram_session    &ram)
				:
					Slave_policy("report_rom", entrypoint, &ram),
					_lock(Genode::Lock::LOCKED)
				{
					configure("<config> <rom>"
					          "  <policy label=\"window_list\"    report=\"window_list\"/>"
					          "  <policy label=\"window_layout\"  report=\"window_layout\"/>"
					          "  <policy label=\"resize_request\" report=\"resize_request\"/>"
					          "  <policy label=\"pointer\"        report=\"pointer\"/>"
					          "  <policy label=\"hover\"          report=\"hover\"/>"
					          "  <policy label=\"focus\"          report=\"focus\"/>"
					          "</rom> </config>");
				}

				bool announce_service(const char             *service_name,
				                      Genode::Root_capability root,
				                      Genode::Allocator      *,
				                      Genode::Server         *)
				{
					if (Genode::strcmp(service_name, "ROM") == 0)
						_rom_root_cap = root;
					else if (Genode::strcmp(service_name, "Report") == 0)
						_report_root_cap = root;
					else
						return false;

					if (_rom_root_cap.valid() && _report_root_cap.valid())
						_lock.unlock();

					return true;
				}

				Genode::Root_capability report_root() const
				{
					Genode::Lock::Guard guard(_lock);
					return _report_root_cap;
				}

				Genode::Root_capability rom_root() const
				{
					Genode::Lock::Guard guard(_lock);
					return _rom_root_cap;
				}
		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::size_t   const _quota = 1024*1024;
		Genode::Slave          _slave;
		Genode::Root_client    _rom_root;
		Genode::Root_client    _report_root;

	public:

		/**
		 * Constructor
		 *
		 * \param ep   entrypoint used for nitpicker child thread
		 * \param ram  RAM session used to allocate the configuration
		 *             dataspace
		 */
		Report_rom_slave(Genode::Cap_session &cap, Genode::Ram_session &ram)
		:
			_ep(&cap, _ep_stack_size, "report_rom"),
			_policy(_ep, ram),
			_slave(_ep, _policy, _quota),
			_rom_root(_policy.rom_root()),
			_report_root(_policy.report_root())
		{ }

		Genode::Rom_session_capability rom_session(char const *label)
		{
			using namespace Genode;

			enum { ARGBUF_SIZE = 128 };
			char argbuf[ARGBUF_SIZE];
			argbuf[0] = 0;

			/*
			 * Declare ram-quota donation
			 */
			enum { SESSION_METADATA = 4*1024 };
			Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", SESSION_METADATA);

			/*
			 * Set session label
			 */
			Arg_string::set_arg(argbuf, sizeof(argbuf), "label", label);

			Session_capability session_cap = _rom_root.session(argbuf, Affinity());

			return static_cap_cast<Genode::Rom_session>(session_cap);
		}

		Genode::Capability<Report::Session> report_session(char const *label)
		{
			using namespace Genode;

			enum { ARGBUF_SIZE = 128 };
			char argbuf[ARGBUF_SIZE];
			argbuf[0] = 0;

			/*
			 * Declare ram-quota donation
			 */
			enum { BUFFER_SIZE = 4096, SESSION_METADATA = BUFFER_SIZE + 8*1024 };
			Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", SESSION_METADATA);
			Arg_string::set_arg(argbuf, sizeof(argbuf), "buffer_size", BUFFER_SIZE);

			/*
			 * Set session label
			 */
			Arg_string::set_arg(argbuf, sizeof(argbuf), "label", label);

			Session_capability session_cap = _report_root.session(argbuf, Affinity());

			return static_cap_cast<Report::Session>(session_cap);
		}
};

#endif /* _REPORT_ROM_SLAVE_H_ */
