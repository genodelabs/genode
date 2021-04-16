/*
 * \brief  ARM-device interface
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2020-04-15
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__PLATFORM_SESSION__DEVICE_H_
#define _INCLUDE__SPEC__ARM__PLATFORM_SESSION__DEVICE_H_

#include <util/mmio.h>
#include <util/string.h>
#include <base/rpc.h>
#include <base/exception.h>
#include <io_mem_session/client.h>
#include <irq_session/client.h>
#include <platform_session/connection.h>

class Platform::Device : Interface, Noncopyable
{
	public:

		struct Mmio;
		struct Irq;

		typedef Platform::Session::Device_name Name;

	private:

		typedef Device_interface::Range Range;

		friend class Mmio;

		::Platform::Connection &_platform;

		Capability<Device_interface> _cap;

		Irq_session_capability _irq(unsigned index)
		{
			return _cap.call<Device_interface::Rpc_irq>(index);
		}

		Io_mem_session_capability _io_mem(unsigned index, Range &range, Cache cache)
		{
			return _cap.call<Device_interface::Rpc_io_mem>(index, range, cache);
		}

		Region_map &_rm() { return _platform._rm; }

	public:

		struct Index { unsigned value; };

		explicit Device(Connection &platform)
		:
			_platform(platform), _cap(platform.acquire_device())
		{ }

		struct Type { String<64> name; };

		Device(Connection &platform, Type type)
		:
			_platform(platform), _cap(platform.device_by_type(type.name.string()))
		{ }

		~Device() { _platform.release_device(_cap); }
};


class Platform::Device::Mmio : Range, Attached_dataspace, public Genode::Mmio
{
	private:

		Dataspace_capability _ds_cap(Device &device, unsigned id)
		{
			Io_mem_session_client io_mem(device._io_mem(id, *this, UNCACHED));
			return io_mem.dataspace();
		}

		addr_t _local_addr()
		{
			return (addr_t)Attached_dataspace::local_addr<char>() + Range::start;
		}

	public:

		struct Index { unsigned value; };

		Mmio(Device &device, Index index)
		:
			Attached_dataspace(device._rm(), _ds_cap(device, index.value)),
			Genode::Mmio(_local_addr())
		{ }

		explicit Mmio(Device &device) : Mmio(device, Index { 0 }) { }

		size_t size() const { return Range::size; }

		template <typename T>
		T *local_addr() { return reinterpret_cast<T *>(_local_addr()); }
};


class Platform::Device::Irq : Noncopyable
{
	private:

		Irq_session_client _irq;

	public:

		struct Index { unsigned value; };

		Irq(Device &device, Index index) : _irq(device._irq(index.value)) { }

		explicit Irq(Device &device) : Irq(device, Index { 0 }) { }

		/**
		 * Acknowledge interrupt
		 *
		 * This method must be called by the interrupt handler.
		 */
		void ack() { _irq.ack_irq(); }

		/**
		 * Register interrupt signal handler
		 *
		 * The call of this method implies a one-time trigger of the interrupt
		 * handler once the driver component becomes receptive to signals. This
		 * artificial interrupt signal alleviates the need to place an explicit
		 * 'Irq::ack' respectively a manual call of the interrupt handler
		 * routine during the driver initialization.
		 *
		 * Furthermore, this artificial interrupt reforces drivers to be robust
		 * against spurious interrupts.
		 */
		void sigh(Signal_context_capability sigh)
		{
			_irq.sigh(sigh);

			/* trigger initial interrupt */
			if (sigh.valid())
				Signal_transmitter(sigh).submit();
		}
};

#endif /* _INCLUDE__SPEC__ARM__PLATFORM_SESSION__DEVICE_H_ */
