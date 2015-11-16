/*
 * \brief  Device PD handling for the platform driver
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#pragma once

#include <os/slave.h>

enum { STACK_SIZE = 2 * sizeof(void *) * 1024 };

namespace Platform { class Device_pd_policy; }

class Platform::Device_pd_policy : public Genode::Slave_policy
{
	private:

		Genode::Root_capability _cap;
		Genode::Lock _lock;

		Genode::Ram_session_capability _ram_ref_cap;
		Genode::Slave                  _device_pd_slave;

	protected:

		char const **_permitted_services() const
		{
			static char const *permitted_services[] = { "LOG", "CAP", "RM", 0 };
			return permitted_services;
		};

	public:

		Device_pd_policy(Genode::Rpc_entrypoint &slave_ep,
		                 Genode::Ram_session_capability ram_ref_cap,
		                 Genode::addr_t device_pd_ram_quota)
		:
			Slave_policy("device_pd", slave_ep),
			_lock(Genode::Lock::LOCKED),
			_ram_ref_cap(ram_ref_cap),
			_device_pd_slave(slave_ep, *this, device_pd_ram_quota, ram_ref_cap)
		{ }

		bool announce_service(const char             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc,
		                      Genode::Server         *server)
		{
			/* wait for 'platform_drv' to announce the DEVICE_PD service */
			if (Genode::strcmp(service_name, "DEVICE_PD"))
				return false;

			_cap = root;

			_lock.unlock();

			return true;
		}

		Genode::Root_capability root() {
			if (!_cap.valid())
				_lock.lock();
			return _cap;
		}

		Genode::Ram_connection &ram_slave() { return _device_pd_slave.ram(); }

		/**
		 * Override struct Genode::Child_policy::ref_ram_cap with our ram cap
		 */
		Genode::Ram_session_capability ref_ram_cap() const override {
			return _ram_ref_cap; }

};


