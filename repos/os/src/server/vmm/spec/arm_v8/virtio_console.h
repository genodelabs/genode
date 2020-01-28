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

namespace Vmm
{
	class Virtio_console;
}

class Vmm::Virtio_console : public Virtio_device
{
	private:

		Terminal::Connection                _terminal;
		Cpu::Signal_handler<Virtio_console> _handler;

		void _read()
		{
			auto read = [&] (addr_t data, size_t size)
			{
				if (!_terminal.avail()) return 0ul;

				size_t length = _terminal.read((void *)data, size);
				return length;
			};

			if (!_terminal.avail() || !_queue[RX].constructed()) return;

			_queue[RX]->notify(read);
			_assert_irq();
		}

		void _notify(unsigned idx) override
		{
			if (idx != TX) return;

			auto write = [&] (addr_t data, size_t size)
			{
				_terminal.write((void *)data, size);
				return size;
			};

			if (_queue[TX]->notify(write))
				_assert_irq();
		}

		Register _device_specific_features() { return 0; }
	public:

		Virtio_console(const char * const name,
		               const uint64_t addr,
		               const uint64_t size,
		               unsigned irq,
		               Cpu      &cpu,
		               Mmio_bus &bus,
		               Ram      &ram,
		               Genode::Env &env)
		: Virtio_device(name, addr, size, irq, cpu, bus, ram),
		  _terminal(env, "console"),
		  _handler(cpu, env.ep(), *this, &Virtio_console::_read)
		{
			/* set device ID to console */
			_device_id(0x3);

			_terminal.read_avail_sigh(_handler);
		}
};

#endif /* _VIRTIO_CONSOLE_H_ */
