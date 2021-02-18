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

namespace Vmm { class Virtio_net; }


class Vmm::Virtio_net : public Virtio_device<Virtio_split_queue, 2>
{
	private:

		enum Queue_id { RX, TX };

		Genode::Env &_env;

		Genode::Heap          _heap     { _env.ram(), _env.rm() };
		Genode::Allocator_avl _tx_alloc { &_heap };

		enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128,
		       NIC_HEADER_SIZE = 12 };

		Nic::Connection  _nic { _env, &_tx_alloc, BUF_SIZE, BUF_SIZE };
		Nic::Mac_address _mac { _nic.mac_address() };

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
				if (!_nic.rx()->packet_avail() || !_nic.rx()->ready_to_ack())
					return 0ul;

				Nic::Packet_descriptor const rx_packet = _nic.rx()->get_packet();

				size_t sz = Genode::min(size, rx_packet.size() + NIC_HEADER_SIZE);

				Genode::memcpy((void *)(data + NIC_HEADER_SIZE),
				               _nic.rx()->packet_content(rx_packet),
				               sz - NIC_HEADER_SIZE);
				_nic.rx()->acknowledge_packet(rx_packet);

				Genode::memset((void*)data, 0, NIC_HEADER_SIZE);

				return sz;
			};

			if (!_queue[RX].constructed()) return;

			bool irq = _queue[RX]->notify(recv);

			if (irq) _assert_irq();
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
				return 0ul; }

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
			Genode::Mutex::Guard guard(_mutex);

			_rx();
			_tx();
		}

		void _notify(unsigned /* idx */) override
		{
			_tx();
			_rx();
		}

		enum Device_id { NIC = 0x1 };

		struct Config_area : Reg
		{
			Nic::Mac_address & mac;

			Register read(Address_range & range,  Cpu&) override
			{
				if (range.start > 5) return 0;

				return mac.addr[range.start];
			}

			Config_area(Virtio_net & device, Nic::Mac_address & mac)
			: Reg(device, "ConfigArea", Mmio_register::RO, 0x100, 8),
			  mac(mac)
			{ }
		} _config_area { *this, _mac };

	public:

		Virtio_net(const char * const name,
		           const uint64_t addr,
		           const uint64_t size,
		           unsigned irq,
		           Cpu      &cpu,
		           Mmio_bus &bus,
		           Ram      &ram,
		           Genode::Env &env)
		:
			Virtio_device<Virtio_split_queue, 2>(name, addr, size,
			                                     irq, cpu, bus, ram, NIC),
			_env(env),
			_handler(cpu, _env.ep(), *this, &Virtio_net::_handle)
		{
			enum { VIRTIO_NET_F_MAC = 1u << 5 };
			_dev_feature.set(VIRTIO_NET_F_MAC);

			_nic.tx_channel()->sigh_ready_to_submit(_handler);
			_nic.tx_channel()->sigh_ack_avail      (_handler);
			_nic.rx_channel()->sigh_ready_to_ack   (_handler);
			_nic.rx_channel()->sigh_packet_avail   (_handler);
		}
};
#endif /* _VIRTIO_NET_H_ */
