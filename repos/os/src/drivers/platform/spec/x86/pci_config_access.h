/*
 * \brief  Interface for accessing PCI configuration registers
 * \author Norman Feske
 * \author Reto Buerki
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
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <platform_device/platform_device.h>

using namespace Genode;

namespace Platform {

	class Config_access
	{
		private:

			Attached_io_mem_dataspace &_pciconf;
			Genode::size_t const       _pciconf_size;

			/**
			 * Calculate device offset from BDF
			 *
			 * \param bus       target PCI bus ID  (0..255)
			 * \param device    target device ID   (0..31)
			 * \param function  target function ID (0..7)
			 *
			 * \return device base address
			 */
			unsigned _dev_base(int bus, int device, int function)
			{
				return ((bus      << 20) |
				        (device   << 15) |
				        (function << 12));
			}

			Genode::Bit_array<256> _used { };

			void _use_register(unsigned char addr, unsigned short width)
			{
				for (unsigned i = 0; i < width; i++)
					if (!_used.get(addr + i, 1))
						_used.set(addr + i, 1);
			}

		public:

			class Invalid_mmio_access : Genode::Exception { };

			Config_access(Attached_io_mem_dataspace &pciconf)
			:
				_pciconf(pciconf),
				_pciconf_size(Dataspace_client(_pciconf.cap()).size())
			{ }

			Config_access(Config_access &c)
			: _pciconf(c._pciconf), _pciconf_size(c._pciconf_size) { }

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
				unsigned ret;
				unsigned const offset = _dev_base(bus, device, function) + addr;
				char const * const field = _pciconf.local_addr<char>() + offset;

				if (offset >= _pciconf_size)
					throw Invalid_mmio_access();

				/*
				 * Memory access code is implemented in a way to make it work
				 * with Muen subject monitor (SM) device emulation and also
				 * general x86 targets. On Muen, the simplified device
				 * emulation code (which also works for Linux) always returns
				 * 0xffff in EAX to indicate a non-existing device. Therefore,
				 * we enforce the usage of EAX in the following assembly
				 * templates. Also clear excess bits before return to guarantee
				 * the requested size.
				 */
				switch (size) {
				case Device::ACCESS_8BIT:
					if (track)
						_use_register(addr, 1);
					asm volatile("movb %1,%%al" :"=a" (ret) :"m" (*((volatile unsigned char *)field)) :"memory");
					return ret & 0xff;
				case Device::ACCESS_16BIT:
					if (track)
						_use_register(addr, 2);

					asm volatile("movw %1,%%ax" :"=a" (ret) :"m" (*(volatile unsigned short *)field) :"memory");
					return ret & 0xffff;
				case Device::ACCESS_32BIT:
					if (track)
						_use_register(addr, 4);

					asm volatile("movl %1,%%eax" :"=a" (ret) :"m" (*(volatile unsigned int *)field) :"memory");
					return ret;
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
				unsigned const offset = _dev_base(bus, device, function) + addr;
				char const * const field = _pciconf.local_addr<char>() + offset;

				if (offset >= _pciconf_size)
					throw Invalid_mmio_access();

				/*
				 * Write value to targeted address, see read() comment above
				 * for an explanation of the assembly templates
				 */
				switch (size) {
				case Device::ACCESS_8BIT:
					if (track)
						_use_register(addr, 1);

					asm volatile("movb %%al,%1" : :"a" (value), "m" (*(volatile unsigned char *)field) :"memory");
					break;
				case Device::ACCESS_16BIT:
					if (track)
						_use_register(addr, 2);

					asm volatile("movw %%ax,%1" : :"a" (value), "m" (*(volatile unsigned char *)field) :"memory");
					break;
				case Device::ACCESS_32BIT:
					if (track)
						_use_register(addr, 4);

					asm volatile("movl %%eax,%1" : :"a" (value), "m" (*(volatile unsigned char *)field) :"memory");
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
	};
}

#endif /* _X86_PCI_CONFIG_ACCESS_H_ */
