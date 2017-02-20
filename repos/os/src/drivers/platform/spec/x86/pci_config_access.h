/*
 * \brief  Interface for accessing PCI configuration registers
 * \author Norman Feske
 * \date   2008-01-29
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _X86_PCI_CONFIG_ACCESS_H_
#define _X86_PCI_CONFIG_ACCESS_H_

#include <util/bit_array.h>
#include <io_port_session/connection.h>
#include <platform_device/platform_device.h>

namespace Platform {

	class Config_access
	{
		private:

			Genode::Env &_env;

			enum { REG_ADDR = 0xcf8, REG_DATA = 0xcfc, REG_SIZE = 4 };

			/**
			 * Request interface to access an I/O port
			 */
			template <unsigned port>
			Genode::Io_port_session *_io_port()
			{
				/*
				 * Open I/O-port session when first called.
				 * The number of instances of the io_port_session_client
				 * variable depends on the number of instances of the
				 * template function.
				 *
				 * Thanks to this mechanism, the sessions to the PCI
				 * ports are created lazily when PCI service is first
				 * used. If the PCI bus driver is just started but not
				 * used yet, other processes are able to access the PCI
				 * config space.
				 *
				 * Once created, each I/O-port session persists until
				 * the PCI driver gets killed by its parent.
				 */
				static Genode::Io_port_connection io_port(_env, port, REG_SIZE);
				return &io_port;
			}

			/**
			 * Generate configuration address
			 *
			 * \param bus       target PCI bus ID  (0..255)
			 * \param device    target device ID   (0..31)
			 * \param function  target function ID (0..7)
			 * \param addr      target byte within targeted PCI config space (0..255)
			 *
			 * \return configuration address (written to REG_ADDR register)
			 */
			unsigned _cfg_addr(int bus, int device, int function, int addr) {
				return ( (1        << 31) |
				         (bus      << 16) |
				         (device   << 11) |
				         (function <<  8) |
				         (addr      & ~3) ); }

			Genode::Bit_array<256> _used;

			void _use_register(unsigned char addr, unsigned short width)
			{
				for (unsigned i = 0; i < width; i++)
					if (!_used.get(addr + i, 1))
						_used.set(addr + i, 1);
			}

		public:

			Config_access(Genode::Env &env) : _env(env) { }

			/**
			 * Read value from config space of specified device/function
			 *
			 * \param bus       target PCI bus ID
			 * \param device    target device ID
			 * \param function  target function ID
			 * \param addr      target byte within targeted PCI config space
			 * \param size      bit width of read access
			 *
			 * \return          value read from specified config-space address
			 *
			 * There is no range check for the input values.
			 */
			unsigned read(int bus, int device, int function,
			              unsigned char addr, Device::Access_size size,
			              bool track = true)
			{
				/* write target address */
				_io_port<REG_ADDR>()->outl(REG_ADDR, _cfg_addr(bus, device, function, addr));

				/* return read value */
				switch (size) {
				case Device::ACCESS_8BIT:
					if (track)
						_use_register(addr, 1);

					return _io_port<REG_DATA>()->inb(REG_DATA + (addr & 3));
				case Device::ACCESS_16BIT:
					if (track)
						_use_register(addr, 2);

					return _io_port<REG_DATA>()->inw(REG_DATA + (addr & 2));
				case Device::ACCESS_32BIT:
					if (track)
						_use_register(addr, 4);

					return _io_port<REG_DATA>()->inl(REG_DATA);
				default:
					return ~0U;
				}
			}

			/**
			 * Write to config space of specified device/function
			 *
			 * \param bus       target PCI bus ID
			 * \param device    target device ID
			 * \param function  target function ID
			 * \param addr      target byte within targeted PCI config space
			 * \param value     value to be written
			 * \param size      bit width of write access
			 *
			 * There is no range check for the input values.
			 */
			void write(int bus, int device, int function, unsigned char addr,
			           unsigned value, Device::Access_size size,
			           bool track = true)
			{
				/* write target address */
				_io_port<REG_ADDR>()->outl(REG_ADDR, _cfg_addr(bus, device,
				                                               function, addr));

				/* write value to targeted address */
				switch (size) {
				case Device::ACCESS_8BIT:
					if (track)
						_use_register(addr, 1);

					_io_port<REG_DATA>()->outb(REG_DATA + (addr & 3), value);
					break;
				case Device::ACCESS_16BIT:
					if (track)
						_use_register(addr, 2);

					_io_port<REG_DATA>()->outw(REG_DATA + (addr & 2), value);
					break;
				case Device::ACCESS_32BIT:
					if (track)
						_use_register(addr, 4);

					_io_port<REG_DATA>()->outl(REG_DATA, value);
					break;
				}
			}

			bool reg_in_use(unsigned char addr, Device::Access_size size)
			{
				switch (size) {
				case Device::ACCESS_8BIT:
					return _used.get(addr, 1);
				case Device::ACCESS_16BIT:
					return _used.get(addr, 2);
				case Device::ACCESS_32BIT:
					return _used.get(addr, 4);
				default:
					return true;
				}
			}

			bool reset_support(unsigned reg, unsigned reg_size) const
			{
				return (REG_ADDR <= reg) &&
				       reg + reg_size <= REG_ADDR + REG_SIZE;
			}

			bool system_reset(unsigned reg, unsigned long long value,
			                  const Device::Access_size &access_size)
			{
				switch (access_size) {
				case Device::ACCESS_8BIT:
					_io_port<REG_ADDR>()->outb(reg, value);
					break;
				case Device::ACCESS_16BIT:
					_io_port<REG_ADDR>()->outw(reg, value);
					break;
				case Device::ACCESS_32BIT:
					_io_port<REG_ADDR>()->outl(reg, value);
					break;
				default:
					return false;
				}

				return true;
			}
	};
}

#endif /* _X86_PCI_CONFIG_ACCESS_H_ */
