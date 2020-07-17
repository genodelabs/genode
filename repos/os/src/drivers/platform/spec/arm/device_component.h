/*
 * \brief  Platform driver for ARM device component
 * \author Stefan Kalkowski
 * \date   2020-04-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_COMPONENT_H_
#define _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_COMPONENT_H_

#include <base/rpc_server.h>
#include <platform_session/platform_session.h>
#include <platform_device/platform_device.h>

#include <env.h>
#include <device.h>

namespace Driver {
	class Device_component;
	class Session_component;
}


class Driver::Device_component : public Rpc_object<Platform::Device>
{
	public:

		Device_component(Session_component        & session,
		                 Driver::Device::Name const device);
		~Device_component();

		Driver::Device::Name device() const;
		Session_component  & session();

		bool acquire();
		void release();

		void report(Xml_generator&);


		/**************************
		 ** Platform::Device API **
		 **************************/

		Irq_session_capability    irq(unsigned) override;
		Io_mem_session_capability io_mem(unsigned, Cache_attribute) override;

	private:

		friend class Session_component;

		Session_component                    & _session;
		Driver::Device::Name             const _device;
		Platform::Device_capability            _cap {};
		List_element<Device_component>         _list_elem { this };

		/*
		 * Noncopyable
		 */
		Device_component(Device_component const &);
		Device_component &operator = (Device_component const &);
};

#endif /* _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_COMPONENT_H_ */
