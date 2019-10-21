/*
 * \brief  Virtio networking implementation
 * \author Sebastian Sumpf
 * \date   2019-10-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _VIRTIO_NET_H_
#define _VIRTIO_NET_H_

#include <nic/packet_allocator.h>
#include <nic_session/connection.h>

#include <virtio_device.h>

namespace Vmm
{
	class Virtio_net;
}

class Vmm::Virtio_net : public Virtio_device
{
	private:

		Genode::Env &_env;

		Genode::Heap          _heap     { _env.ram(), _env.rm() };
		Genode::Allocator_avl _tx_alloc { &_heap };

		enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128,
		       NIC_HEADER_SIZE = 12 };

		Nic::Connection _nic { _env, &_tx_alloc, BUF_SIZE, BUF_SIZE };

		Cpu::Signal_handler<Virtio_net> _handler;

		void _free_packets()
		{
			while (_nic.tx()->ack_avail()) {
				Nic::Packet_descriptor packet = _nic.tx()->get_acked_packet();
				_nic.tx()->release_packet(packet);
			}
		}

		void _rx()
		{
			/* RX */
			auto recv = [&] (addr_t data, size_t size)
			{
				Nic::Packet_descriptor const rx_packet = _nic.rx()->get_packet();

				size_t sz = Genode::min(size, rx_packet.size() + NIC_HEADER_SIZE);
				Genode::memcpy((void *)(data + NIC_HEADER_SIZE),
				               _nic.rx()->packet_content(rx_packet),
				               sz);
				_nic.rx()->acknowledge_packet(rx_packet);

				return sz;
			};

			if (!_queue[RX].constructed()) return;

			bool progress = false;
			while (_nic.rx()->packet_avail() && _nic.rx()->ready_to_ack()) {
				if (!_queue[RX]->notify(recv)) break;
				progress = true;
			}

			if (progress) _assert_irq();
		}

		void _tx()
		{
			auto send = [&] (addr_t data, size_t size)
			{
				if (!_nic.tx()->ready_to_submit()) return 0lu;

				data += NIC_HEADER_SIZE; size -= NIC_HEADER_SIZE;

				Nic::Packet_descriptor tx_packet;
				try {
					tx_packet = _nic.tx()->alloc_packet(size); }
				catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
				return 0lu; }

				Genode::memcpy(_nic.tx()->packet_content(tx_packet),
				               (void *)data, size);
				_nic.tx()->submit_packet(tx_packet);
				return size;
			};

			if (!_queue[TX].constructed()) return;

			if (_queue[TX]->notify(send)) _assert_irq();
			_free_packets();
		}

		void _handle()
		{
			_rx();
			_tx();
		}

		void _notify(unsigned /* idx */) override
		{
			_rx();
			_tx();
		}

	public:

		Virtio_net(const char * const name,
		           const uint64_t addr,
		           const uint64_t size,
		           unsigned irq,
		           Cpu      &cpu,
		           Mmio_bus &bus,
		           Ram      &ram,
		           Genode::Env &env)
		: Virtio_device(name, addr, size, irq, cpu, bus, ram, 128),
		  _env(env),
		  _handler(cpu, _env.ep(), *this, &Virtio_net::_handle)
		{
			/* set device ID to network */
			_device_id(0x1);

			_nic.tx_channel()->sigh_ready_to_submit(_handler);
			_nic.tx_channel()->sigh_ack_avail      (_handler);
			_nic.rx_channel()->sigh_ready_to_ack   (_handler);
			_nic.rx_channel()->sigh_packet_avail   (_handler);
		}
};
#endif /* _VIRTIO_NET_H_ */
