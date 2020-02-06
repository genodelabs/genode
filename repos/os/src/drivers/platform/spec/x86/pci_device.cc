/*
 * \brief  PCI device component implementation
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "pci_session_component.h"
#include "pci_device_component.h"

Genode::Io_port_session_capability Platform::Device_component::io_port(Genode::uint8_t const v_id)
{
	Genode::uint8_t const max = sizeof(_io_port_conn) / sizeof(_io_port_conn[0]);
	Genode::uint8_t r_id = 0;

	for (unsigned i = 0; i < max; ++i) {
		Pci::Resource res = _device_config.resource(i);

		if (!res.valid() || res.mem())
			continue;

		if (v_id != r_id) {
			++r_id;
			continue;
		}

		if (_io_port_conn[v_id] != nullptr)
			return _io_port_conn[v_id]->cap();

		try {
			_io_port_conn[v_id] = new (_slab_ioport) Genode::Io_port_connection(_env, res.base(), res.size());
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
	Genode::uint8_t r_id = 0;

	for (unsigned i = 0; i < max; ++i) {
		Pci::Resource res = _device_config.resource(i);

		if (!res.valid() || !res.mem())
			continue;

		if (v_id != r_id) {
			++r_id;
			continue;
		}

		/* limit IO_MEM session size to resource size */
		Genode::size_t const res_size = Genode::min(size, res.size());

		if (offset >= res.size() || offset > res.size() - res_size)
			return Genode::Io_mem_session_capability();

		/* error if MEM64 resource base address above 4G on 32-bit */
		if (res.base() > ~(addr_t)0) {
			Genode::error("request for MEM64 resource of ", _device_config,
			              " at ", Genode::Hex(res.base()),
			              " not supported on 32-bit system");
			return Genode::Io_mem_session_capability();
		}

		try {
			bool const wc = caching == Genode::Cache_attribute::WRITE_COMBINED;
			Io_mem * io_mem = new (_slab_iomem) Io_mem(_env,
			                                           res.base() + offset,
			                                           res_size, wc);
			_io_mem[i].insert(io_mem);
			return io_mem->cap();
		}
		catch (Genode::Out_of_caps) {
			Genode::warning("Out_of_caps in Device_component::io_mem");
			throw;
		}
		catch (Genode::Out_of_ram) {
			Genode::warning("Out_of_ram in Device_component::io_mem");
			throw;
		}
		catch (...) {
			Genode::warning("unhandled exception in 'Device_component::io_mem'");
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
			if (!_device_config.reg_in_use(_config_access, address, size))
				break;

			Genode::error(_device_config, " write access to "
			              "address=", Genode::Hex(address), " "
			              "value=",   Genode::Hex(value),   " "
			              "size=",    Genode::Hex(size),    " "
			              "denied - it is used by the platform driver.");
			return;
		case Device_config::PCI_CMD_REG: /* COMMAND register - first byte */
			if (size == Access_size::ACCESS_16BIT)
				break;
			[[fallthrough]];
		case Device_config::PCI_CMD_REG + 1: /* COMMAND register - second byte */
		case 0xd: /* Latency timer */
			if (size == Access_size::ACCESS_8BIT)
				break;
			[[fallthrough]];
		default:
			Genode::warning(_device_config, " write access to "
			                "address=", Genode::Hex(address), " "
			                "value=",   Genode::Hex(value),   " "
			                "size=",    Genode::Hex(size),    " "
			                "got dropped");
			return;
	}

	/* assign device to device_pd */
	if (address == Device_config::PCI_CMD_REG &&
	    (value & Device_config::PCI_CMD_DMA)) {

		try { _session.assign_device(this); }
		catch (Out_of_ram)  { throw; }
		catch (Out_of_caps) { throw; }
		catch (...) {
			Genode::error("assignment to device failed");
		}
		_enabled_bus_master = true;
	}

	_device_config.write(_config_access, address, value, size,
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
		                                                   _irq_line, ~0UL,
		                                                   _env,
		                                                   _global_heap);

		_env.ep().rpc_ep().manage(_irq_session);
		return _irq_session->cap();
	}

	Genode::uint16_t const msi_cap = _msi_cap();
	Genode::uint16_t const msix_cap = _msix_cap();

	_irq_session = construct_at<Irq_session_component>(_mem_irq_component,
	                                                   _configure_irq(_irq_line, msi_cap, msix_cap),
	                                                   (!_session.msi_usage() || (!msi_cap && !msix_cap)) ? ~0UL : _config_space,
	                                                   _env, _global_heap);
	_env.ep().rpc_ep().manage(_irq_session);

	bool msix_used = false;
	bool msi_used = false;

	if (_irq_session->msi()) {
		if (msix_cap)
			msix_used = _setup_msix(msix_cap);
		if (!msix_used && msi_cap)
			msi_used = _setup_msi(msi_cap);
	}

	if (_irq_session->msi())
		Genode::log(_device_config, " uses ",
		            msix_used ? "MSI-X " : "",
		            msi_used ? "MSI ": "",
		            (!msi_used && !msix_used) ? "no MSI/-X/IRQ " : "",
		            "vector ", Genode::Hex(_irq_session->msi_data()), ", "
		            "address ", Genode::Hex(_irq_session->msi_address()));
	else
		Genode::log(_device_config, " uses IRQ, vector ",
		            Genode::Hex(_irq_line),
		            ", supports: ",
		            msi_cap ? " MSI" : "",
		            msix_cap ? " MSI-X" : "");

	return _irq_session->cap();
}

bool Platform::Device_component::_setup_msi(Genode::uint16_t const msi_cap)
{
	try {
		addr_t const msi_address = _irq_session->msi_address();
		uint32_t const msi_value = _irq_session->msi_data();

		uint16_t msi = _device_config.read(_config_access,
		                                   msi_cap + 2,
		                                   Platform::Device::ACCESS_16BIT);

		_device_config.write(_config_access, msi_cap + 0x4, msi_address,
		                     Platform::Device::ACCESS_32BIT);

		if (msi & CAP_MSI_64) {
			uint32_t upper_address = sizeof(msi_address) > 4
			                       ? uint64_t(msi_address) >> 32
			                       : 0UL;

			_device_config.write(_config_access, msi_cap + 0x8,
			                     upper_address,
			                     Platform::Device::ACCESS_32BIT);
			_device_config.write(_config_access, msi_cap + 0xc,
			                     msi_value,
			                     Platform::Device::ACCESS_16BIT);
		} else
			_device_config.write(_config_access, msi_cap + 0x8, msi_value,
			                     Platform::Device::ACCESS_16BIT);

		/* enable MSI */
		_device_config.write(_config_access, msi_cap + 2,
		                     msi ^ MSI_ENABLED,
		                     Platform::Device::ACCESS_8BIT);

		msi = _device_config.read(_config_access,
		                          msi_cap + 2,
		                          Platform::Device::ACCESS_16BIT);
		return msi & MSI_ENABLED;
	} catch (...) { }

	return false;
}

bool Platform::Device_component::_setup_msix(Genode::uint16_t const msix_cap)
{
	try {
		struct Table_pba : Register<32>
		{
			struct Bir : Bitfield<0, 3> { };
			struct Offset : Bitfield<3, 29> { };
		};

		addr_t const msi_address = _irq_session->msi_address();
		uint32_t const msi_value = _irq_session->msi_data();

		uint16_t ctrl = _device_config.read(_config_access,
		                                    msix_cap + 2,
		                                    Platform::Device::ACCESS_16BIT);

		uint32_t const table = _device_config.read(_config_access,
		                                           msix_cap + 4,
		                                           Platform::Device::ACCESS_32BIT);

		uint32_t const slots = Msix_ctrl::Slots::get(ctrl) + 1;

		uint8_t  const table_bir = Table_pba::Bir::masked(table);
		uint32_t const table_off = Table_pba::Offset::masked(table);

		enum { SIZEOF_MSI_TABLE_ENTRY = 16, SIZE_IOMEM = 0x1000 };

		Pci::Resource res = _device_config.resource(table_bir);
		if (!slots || !res.valid() || res.size() < SIZE_IOMEM ||
		    table_off > res.size() - SIZE_IOMEM)
			return false;

		Genode::uint64_t const msix_base = res.base() + table_off;

		Io_mem io_mem(_env, msix_base, SIZE_IOMEM, false);
		Attached_dataspace x(_env.rm(), io_mem.dataspace());

		if (slots * SIZEOF_MSI_TABLE_ENTRY > SIZE_IOMEM)
			return false;

		struct Msi_entry : public Mmio {
			Msi_entry(addr_t const base) : Mmio(base) { }

			struct Address : Register<0x0, 64> { };
			struct Value   : Register<0x8, 32> { };
			struct Vector  : Register<0xc, 32> {
				struct Mask : Bitfield <0, 1> { };
			};
		} msi_entry_0 (reinterpret_cast<addr_t>(x.local_addr<void>()));

		/* setup first msi-x table entry */
		msi_entry_0.write<Msi_entry::Address>(msi_address & ~(0x3UL));
		msi_entry_0.write<Msi_entry::Value>(msi_value);
		msi_entry_0.write<Msi_entry::Vector::Mask>(0);

		/* disable all msi-x table entries beside the first one */
		for (unsigned i = 1; i < slots; i++) {
			struct Msi_entry unused (reinterpret_cast<addr_t>(x.local_addr<void>()) + SIZEOF_MSI_TABLE_ENTRY * i);
			unused.write<Msi_entry::Vector::Mask>(1);
		}

		/* enable MSI-X */
		Msix_ctrl::Fmask::set(ctrl, 0);
		Msix_ctrl::Enable::set(ctrl, 1);
		_device_config.write(_config_access, msix_cap + 2, ctrl,
		                     Platform::Device::ACCESS_16BIT);

		ctrl = _device_config.read(_config_access, msix_cap + 2,
		                           Platform::Device::ACCESS_16BIT);
		return Msix_ctrl::Enable::get(ctrl);
	} catch (...) { }

	return false;
}
