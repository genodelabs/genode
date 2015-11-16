/*
 * \brief  PCI device component implementation
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "pci_session_component.h"
#include "pci_device_component.h"

Genode::Io_port_session_capability Platform::Device_component::io_port(Genode::uint8_t v_id)
{
	Genode::uint8_t max = sizeof(_io_port_conn) / sizeof(_io_port_conn[0]);
	Genode::uint8_t i = 0, r_id = 0;

	for (Resource res = resource(0); i < max; i++, res = resource(i))
	{
		if (res.type() != Resource::IO)
			continue;

		if (v_id != r_id) {
			r_id ++;
			continue;
		}

		if (_io_port_conn[v_id] != nullptr)
			return _io_port_conn[v_id]->cap();

		try {
			_io_port_conn[v_id] = new (_slab_ioport) Genode::Io_port_connection(res.base(), res.size());
			return _io_port_conn[v_id]->cap();
		} catch (...) {
			return Genode::Io_port_session_capability();
		}
	}

	return Genode::Io_port_session_capability();
}

Genode::Io_mem_session_capability Platform::Device_component::io_mem(Genode::uint8_t const v_id,
                                                                     Genode::Cache_attribute const caching,
                                                                     Genode::addr_t const offset,
                                                                     Genode::size_t const size)
{
	Genode::uint8_t max = sizeof(_io_mem) / sizeof(_io_mem[0]);
	Genode::uint8_t i = 0, r_id = 0;

	for (Resource res = resource(0); i < max; i++, res = resource(i))
	{
		if (res.type() != Resource::MEMORY)
			continue;

		if (v_id != r_id) {
			r_id ++;
			continue;
		}

		/* limit IO_MEM session size to resource size */
		Genode::size_t const res_size = Genode::min(size, res.size());

		if (offset >= res.size() || offset > res.size() - res_size)
			return Genode::Io_mem_session_capability();

		try {
			bool const wc = caching == Genode::Cache_attribute::WRITE_COMBINED;
			Io_mem * io_mem = new (_slab_iomem) Io_mem(res.base() + offset,
			                                           res_size, wc);
			_io_mem[i].insert(io_mem);
			return io_mem->cap();
		} catch (Genode::Allocator::Out_of_memory) {
			throw Quota_exceeded();
		} catch (...) {
			return Genode::Io_mem_session_capability();
		}
	}

	return Genode::Io_mem_session_capability();
}

void Platform::Device_component::config_write(unsigned char address,
                                              unsigned value,
                                              Access_size size)
{
	/* white list of ports which we permit to write */
	switch (address) {
		case 0x40 ... 0xff:
			/* allow access to device-specific registers if not used by us */
			if (!_device_config.reg_in_use(&_config_access, address, size))
				break;

			PERR("%x:%x:%x write access to address=%x value=0x%x"
			     " size=0x%x denied - it is used by the platform driver.",
			     _device_config.bus_number(), _device_config.device_number(),
			     _device_config.function_number(), address, value, size);
			return;
		case PCI_CMD_REG: /* COMMAND register - first byte */
			if (size == Access_size::ACCESS_16BIT)
				break;
		case PCI_CMD_REG + 1: /* COMMAND register - second byte */
		case 0xd: /* Latency timer */
			if (size == Access_size::ACCESS_8BIT)
				break;
		default:
			PWRN("%x:%x:%x write access to address=%x value=0x%x "
			     " size=0x%x got dropped", _device_config.bus_number(),
				_device_config.device_number(),
				_device_config.function_number(),
				address, value, size);
			return;
	}

	/* assign device to device_pd */
	if (address == PCI_CMD_REG && value & PCI_CMD_DMA && _session) {
		try {
			_session->assign_device(this);
		} catch (Platform::Session::Out_of_metadata) {
			throw Quota_exceeded();
		} catch (...) {
			PERR("assignment to device failed");
		}
	}

	_device_config.write(&_config_access, address, value, size,
	                     _device_config.DONT_TRACK_ACCESS);
}

Genode::Irq_session_capability Platform::Device_component::irq(Genode::uint8_t id)
{
	if (id != 0)
		return Genode::Irq_session_capability();

	if (_irq_session)
		return _irq_session->cap();

	using Genode::construct_at;

	if (!_device_config.valid()) {
		/* Non PCI devices */
		_irq_session = construct_at<Irq_session_component>(_mem_irq_component,
		                                                   _irq_line, ~0UL);

		_ep->manage(_irq_session);
		return _irq_session->cap();
	}

	_irq_session = construct_at<Irq_session_component>(_mem_irq_component,
	                                                   _configure_irq(_irq_line),
	                                                   (!_session->msi_usage() || !_msi_cap()) ? ~0UL : _config_space);
	_ep->manage(_irq_session);

	if (_irq_session->msi()) {

		Genode::addr_t msi_address = _irq_session->msi_address();
		Genode::uint32_t msi_value = _irq_session->msi_data();
		Genode::uint16_t msi_cap   = _msi_cap();

		Genode::uint16_t msi = _device_config.read(&_config_access,
		                                           msi_cap + 2,
		                                           Platform::Device::ACCESS_16BIT);

		_device_config.write(&_config_access, msi_cap + 0x4, msi_address,
		                     Platform::Device::ACCESS_32BIT);

		if (msi & CAP_MSI_64) {
			Genode::uint32_t upper_address = sizeof(msi_address) > 4
			                               ? (Genode::uint64_t)msi_address >> 32
			                               : 0UL;

			_device_config.write(&_config_access, msi_cap + 0x8,
			                     upper_address,
			                     Platform::Device::ACCESS_32BIT);
			_device_config.write(&_config_access, msi_cap + 0xc,
			                     msi_value,
			                     Platform::Device::ACCESS_16BIT);
		}
		else
			_device_config.write(&_config_access, msi_cap + 0x8, msi_value,
			                     Platform::Device::ACCESS_16BIT);

		/* enable MSI */
		_device_config.write(&_config_access, msi_cap + 2,
		                     msi ^ MSI_ENABLED,
		                     Platform::Device::ACCESS_8BIT);
	}

	bool msi_64 = false;
	Genode::uint16_t msi_cap   = _msi_cap();
	if (msi_cap) {
		Genode::uint16_t msi = _device_config.read(&_config_access,
		                                           msi_cap + 2,
		                                           Platform::Device::ACCESS_16BIT);
		msi_64 = msi & CAP_MSI_64;
	}

	if (_irq_session->msi())
		PINF("%x:%x.%x uses MSI %s, vector 0x%lx, address 0x%lx",
		     _device_config.bus_number(),
		     _device_config.device_number(),
		     _device_config.function_number(),
		     msi_64 ? "64bit" : "32bit",
		     _irq_session->msi_data(), _irq_session->msi_address());
	else
		PINF("%x:%x.%x uses IRQ, vector 0x%x%s",
		     _device_config.bus_number(),
		     _device_config.device_number(),
		     _device_config.function_number(), _irq_line,
		     msi_cap ? (msi_64 ? ", MSI 64bit capable" :
		                         ", MSI 32bit capable") : "");

	return _irq_session->cap();
}
