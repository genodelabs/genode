/*
 * \brief  Dummy - platform session device interface
 * \author Stefan Kalkowski
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM_SESSION__DEVICE_H_
#define _PLATFORM_SESSION__DEVICE_H_

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/exception.h>
#include <io_mem_session/client.h>
#include <irq_session/client.h>
#include <platform_session/connection.h>
#include <util/mmio.h>
#include <util/string.h>


namespace Platform {
	struct Connection;
	class Device;

	using namespace Genode;
}


class Platform::Device : Interface, Noncopyable
{
	public:

		struct Range { addr_t start; size_t size; };

		struct Mmio;
		struct Irq;
		struct Config_space;

		using Name = String<64>;

	private:

		Connection &_platform;
		Legacy_platform::Device_capability _device_cap { };
		Name _name { };

	public:

		int _bar_checked_for_size[6] { };

		struct Index { unsigned value; };

		explicit Device(Connection &platform)
		: _platform { platform } { }

		struct Type { String<64> name; };

		Device(Connection &platform, Type type);

		Device(Connection &platform, Name name);

		~Device() { }

		Name const &name() const
		{
			return _name;
		}
};


class Platform::Device::Mmio : Range
{
	public:

		struct Index { unsigned value; };

	private:

		Constructible<Attached_dataspace> _attached_ds { };

		void *_local_addr();

		Device &_device;
		Index   _index;

	public:

		Mmio(Device &device, Index index)
		: _device { device }, _index { index } { }

		explicit Mmio(Device &device)
		: _device { device }, _index { ~0u } { }

		size_t size() const;

		template <typename T>
		T *local_addr() { return reinterpret_cast<T *>(_local_addr()); }
};


class Platform::Device::Irq : Noncopyable
{
	public:

		struct Index { unsigned value; };

	private:

		Device &_device;
		Index   _index;

		Constructible<Irq_session_client> _irq { };

	public:

		Irq(Device &device, Index index);

		explicit Irq(Device &device)
		: _device { device }, _index { ~0u } { }

		void ack();
		void sigh(Signal_context_capability);
		void sigh_omit_initial_signal(Signal_context_capability);
};


class Platform::Device::Config_space : Noncopyable
{
	public:

		Device &_device;

		enum class Access_size : unsigned { ACCESS_8BIT, ACCESS_16BIT, ACCESS_32BIT };

		Config_space(Device &device) : _device { device } { }

		unsigned read(unsigned char address, Access_size size);
		void     write(unsigned char address, unsigned value, Access_size size);
};


#endif /* _PLATFORM_SESSION__DEVICE_H_ */
