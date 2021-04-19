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


Genode::Io_port_session_capability Platform::Device_component::io_port(uint8_t const v_id)
{
	uint8_t const max = sizeof(_io_port_conn) / sizeof(_io_port_conn[0]);
	uint8_t r_id = 0;

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
			_io_port_conn[v_id] = new (_slab_ioport)
				Io_port_connection(_env, res.base(), res.size());
			return _io_port_conn[v_id]->cap();
		} catch (...) {
			return Io_port_session_capability();
		}
	}

	return Io_port_session_capability();
}

Genode::Io_mem_session_capability Platform::Device_component::io_mem(uint8_t const v_id,
                                                                     Cache   const caching,
                                                                     addr_t  const offset,
                                                                     size_t  const size)
{
	uint8_t max = sizeof(_io_mem) / sizeof(_io_mem[0]);
	uint8_t r_id = 0;

	for (unsigned i = 0; i < max; ++i) {
		Pci::Resource res = _device_config.resource(i);

		if (!res.valid() || !res.mem())
			continue;

		if (v_id != r_id) {
			++r_id;
			continue;
		}

		/* limit IO_MEM session size to resource size */
		size_t const res_size = min(size, res.size());

		if (offset >= res.size() || offset > res.size() - res_size)
			return Io_mem_session_capability();

		/* error if MEM64 resource base address above 4G on 32-bit */
		if (res.base() > ~(addr_t)0) {
			error("request for MEM64 resource of ", _device_config,
			      " at ", Hex(res.base()), " not supported on 32-bit system");
			return Io_mem_session_capability();
		}

		try {
			bool const wc = caching == Cache::WRITE_COMBINED;
			Io_mem * io_mem = new (_slab_iomem) Io_mem(_env,
			                                           res.base() + offset,
			                                           res_size, wc);
			_io_mem[i].insert(io_mem);
			return io_mem->cap();
		}
		catch (Out_of_caps) {
			warning("Out_of_caps in Device_component::io_mem");
			throw;
		}
		catch (Out_of_ram) {
			warning("Out_of_ram in Device_component::io_mem");
			throw;
		}
		catch (...) {
			warning("unhandled exception in 'Device_component::io_mem'");
			return Io_mem_session_capability();
		}
	}

	return Io_mem_session_capability();
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

			error(_device_config, " write access to "
			      "address=", Hex(address), " "
			      "value=",   Hex(value),   " "
			      "size=",    Hex(size),    " "
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
			warning(_device_config, " write access to "
			        "address=", Hex(address), " "
			        "value=",   Hex(value),   " "
			        "size=",    Hex(size),    " "
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
			error("assignment to device failed");
		}
		_device_used = true;
	}

	_device_config.write(_config_access, address, value, size,
	                     _device_config.DONT_TRACK_ACCESS);
}

Genode::Irq_session_capability Platform::Device_component::irq(uint8_t id)
{
	if (id != 0)
		return Irq_session_capability();

	if (_irq_session)
		return _irq_session->cap();

	if (!_device_config.valid()) {
		/* Non PCI devices */
		_irq_session = construct_at<Irq_session_component>(_mem_irq_component,
		                                                   _irq_line, ~0UL,
		                                                   _env,
		                                                   _global_heap);

		_env.ep().rpc_ep().manage(_irq_session);
		return _irq_session->cap();
	}

	uint16_t const msi_cap  = _msi_cap();
	uint16_t const msix_cap = _msix_cap();
	bool const try_msi_msix = (_session.msi_usage()  && msi_cap) ||
	                          (_session.msix_usage() && msix_cap);
	_irq_session = construct_at<Irq_session_component>(_mem_irq_component,
	                                                   _configure_irq(_irq_line, msi_cap, msix_cap),
	                                                   try_msi_msix ? _config_space : ~0UL,
	                                                   _env, _global_heap);
	_env.ep().rpc_ep().manage(_irq_session);

	bool msix_used = false;
	bool msi_used = false;

	if (_irq_session->msi()) {
		if (_session.msix_usage() && msix_cap)
			msix_used = _setup_msix(msix_cap);
		if (!msix_used && msi_cap)
			msi_used = _setup_msi(msi_cap);
	}

	if (_irq_session->msi())
		log(_device_config, " uses ",
		    msix_used ? "MSI-X " : "",
		    (msix_used && msi_cap) ? "(supports MSI) " : "",
		    msi_used ? "MSI ": "",
		    (msi_used && msix_cap) ? "(supports MSI-X) " : "",
		    (!msi_used && !msix_used) ? "no MSI/-X/IRQ " : "",
		    "vector ", Hex(_irq_session->msi_data()), ", "
		    "address ", Hex(_irq_session->msi_address()));
	else
		log(_device_config, " uses IRQ, vector ",
		    Hex(_irq_line),
		    (msi_cap || msix_cap) ? ", supports:" : "",
		    msi_cap ? " MSI" : "",
		    msix_cap ? " MSI-X" : "");

	return _irq_session->cap();
}

bool Platform::Device_component::_setup_msi(uint16_t const msi_cap)
{
	try {
		addr_t const msi_address = _irq_session->msi_address();
		uint32_t const msi_value = _irq_session->msi_data();

		uint16_t msi = _read_config_16(msi_cap + 2);

		_write_config_32(msi_cap + 0x4, msi_address);

		if (msi & CAP_MSI_64) {
			uint32_t upper_address = sizeof(msi_address) > 4
			                       ? uint64_t(msi_address) >> 32
			                       : 0UL;

			_write_config_16(msi_cap + 0x8, upper_address);
			_write_config_16(msi_cap + 0xc, msi_value);
		} else
			_write_config_16(msi_cap + 0x8, msi_value);

		/* enable MSI */
		_device_config.write(_config_access, msi_cap + 2,
		                     msi ^ MSI_ENABLED,
		                     Platform::Device::ACCESS_8BIT);

		msi = _read_config_16(msi_cap + 2);

		return msi & MSI_ENABLED;
	} catch (...) { }

	return false;
}

bool Platform::Device_component::_setup_msix(uint16_t const msix_cap)
{
	try {
		struct Table_pba : Register<32>
		{
			struct Bir : Bitfield<0, 3> { };
			struct Offset : Bitfield<3, 29> { };
		};

		addr_t const msi_address = _irq_session->msi_address();
		uint32_t const msi_value = _irq_session->msi_data();

		uint16_t ctrl = _read_config_16(msix_cap + 2);

		uint32_t const slots = Msix_ctrl::Slots::get(ctrl) + 1;

		uint32_t const table = _read_config_32(msix_cap + 4);
		uint8_t  const table_bir = Table_pba::Bir::masked(table);
		uint32_t const table_off = Table_pba::Offset::masked(table);

		enum { SIZEOF_MSI_TABLE_ENTRY = 16, SIZE_IOMEM = 0x1000 };

		Pci::Resource res = _device_config.resource(table_bir);
		if (!slots || !res.valid() || res.size() < SIZE_IOMEM ||
		    table_off > res.size() - SIZE_IOMEM)
			return false;

		if (slots * SIZEOF_MSI_TABLE_ENTRY > SIZE_IOMEM)
			return false;

		uint64_t const msix_table_phys = res.base() + table_off;

		apply_msix_table(res, msix_table_phys, SIZE_IOMEM,
		                 [&](addr_t const msix_table)
		{
			struct Msi_entry : public Mmio {
				Msi_entry(addr_t const base) : Mmio(base) { }

				struct Address_low  : Register<0x0, 32> { };
				struct Address_high : Register<0x4, 32> { };
				struct Value        : Register<0x8, 32> { };
				struct Vector       : Register<0xc, 32> {
					struct Mask : Bitfield <0, 1> { };
				};
			} msi_entry_0 (msix_table);

			/* setup first msi-x table entry */
			msi_entry_0.write<Msi_entry::Address_low>(msi_address & ~(0x3UL));
			msi_entry_0.write<Msi_entry::Address_high>(sizeof(msi_address) == 4 ? 0 : msi_address >> 32);
			msi_entry_0.write<Msi_entry::Value>(msi_value);
			msi_entry_0.write<Msi_entry::Vector::Mask>(0);

			/* disable all msi-x table entries beside the first one */
			for (unsigned i = 1; i < slots; i++) {
				struct Msi_entry unused (msix_table + SIZEOF_MSI_TABLE_ENTRY * i);
				unused.write<Msi_entry::Vector::Mask>(1);
			}

			/* enable MSI-X */
			Msix_ctrl::Fmask::set(ctrl, 0);
			Msix_ctrl::Enable::set(ctrl, 1);
			_write_config_16(msix_cap + 2, ctrl);
		});

		/* check back that MSI-X got enabled */
		ctrl = _read_config_16(msix_cap + 2);

		return Msix_ctrl::Enable::get(ctrl);
	} catch (Out_of_caps) {
		warning("Out_of_caps during MSI-X enablement"); }
	catch (Out_of_ram) {
		warning("Out_of_ram during MSI-X enablement"); }
	catch (...) { warning("MSI-X enablement failed"); }

	return false;
}
