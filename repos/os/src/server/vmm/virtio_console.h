/*
 * \brief  Virtio console implementation
 * \author Sebastian Sumpf
 * \date   2019-10-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIRTIO_CONSOLE_H_
#define _VIRTIO_CONSOLE_H_

#include <terminal_session/connection.h>

#include <virtio_device.h>

namespace Vmm { class Virtio_console; }


class Vmm::Virtio_console : public Virtio_device<Virtio_split_queue, 2>
{
	private:

		enum Queue_id { RX, TX };

		Terminal::Connection                _terminal;
		Cpu::Signal_handler<Virtio_console> _handler;

		void _read()
		{
			Genode::Mutex::Guard guard(_mutex);

			auto read = [&] (Byte_range_ptr const &data)
			{
				if (!_terminal.avail()) return 0ul;

				size_t length = _terminal.read((void *)data.start, data.num_bytes);
				return length;
			};

			if (!_terminal.avail() || !_queue[RX].constructed()) return;

			_queue[RX]->notify(read);
			_buffer_notification();
		}

		void _notify(unsigned idx) override
		{
			if (idx != TX) return;

			auto write = [&] (Byte_range_ptr const &data)
			{
				_terminal.write((void *)data.start, data.num_bytes);
				return data.num_bytes;
			};

			if (_queue[TX]->notify(write))
				_buffer_notification();
		}

		enum Device_id { CONSOLE = 0x3 };

		struct Config_area : Reg
		{
			Register read(Address_range & range,  Cpu&) override
			{
				switch (range.start()) {
				case 4:   return 1; /* maximum ports */
				default: ;
				}
				return 0;
			}

			void write(Address_range &,  Cpu &, Register) override {}

			Config_area(Virtio_console & console)
			: Reg(console, "ConfigArea", Mmio_register::RW, 0x100, 12) { }
		} _config_area { *this };

	public:

		Virtio_console(const char * const   name,
		               const uint64_t       addr,
		               const uint64_t       size,
		               unsigned             irq,
		               Cpu                & cpu,
		               Mmio_bus           & bus,
		               Ram                & ram,
		               Virtio_device_list & list,
		               Genode::Env        & env)
		:
			Virtio_device<Virtio_split_queue, 2>(name, addr, size, irq,
			                                     cpu, bus, ram, list, CONSOLE),
			_terminal(env, "console"),
			_handler(cpu, env.ep(), *this, &Virtio_console::_read)
		{
			_terminal.read_avail_sigh(_handler);
		}
};

#endif /* _VIRTIO_CONSOLE_H_ */
