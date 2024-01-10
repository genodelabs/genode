/*
 * \brief  LAN9118 NIC driver
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2011-05-21
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__NIC__SPEC__LAN9118__LAN9118_H_
#define _DRIVERS__NIC__SPEC__LAN9118__LAN9118_H_

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/log.h>
#include <util/misc_math.h>
#include <irq_session/client.h>
#include <timer_session/connection.h>
#include <nic_session/nic_session.h>

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>

using namespace Genode;

class Lan9118_base
{
	private:

		/*
		 * Noncopyable
		 */
		Lan9118_base(Lan9118_base const &);
		Lan9118_base &operator = (Lan9118_base const &);

	protected:

		/**
		 * MMIO register offsets
		 */
		enum Register {
			RX_DATA_FIFO        = 0x00,
			TX_DATA_FIFO        = 0x20,
			RX_STATUS_FIFO      = 0x40,
			RX_STATUS_FIFO_PEEK = 0x44,
			TX_STATUS_FIFO      = 0x48,
			TX_STATUS_FIFO_PEEK = 0x4c,
			ID_REV              = 0x50,
			IRQ_CFG             = 0x54,
			INT_STS             = 0x58,
			INT_EN              = 0x5c,
			BYTE_TEST           = 0x64,
			TX_CFG              = 0x70,
			HW_CFG              = 0x74,
			RX_FIFO_INF         = 0x7c,
			TX_FIFO_INF         = 0x80,
			MAC_CSR_CMD         = 0xa4,
			MAC_CSR_DATA        = 0xa8,
		};

		/**
		 * MAC registers, indirectly accessed via 'MAC_CSR_CMD' and
		 * 'MAC_CSR_DATA'
		 */
		enum Mac_register {
			MAC_CR    = 1,
			MAC_ADDRH = 2,
			MAC_ADDRL = 3,
		};

		/**
		 * Bits in MAC_CSR_CMD register
		 */
		enum {
			MAC_CSR_CMD_BUSY  = (1 << 31),
			MAC_CSR_CMD_READ  = (1 << 30),
			MAC_CSR_CMD_WRITE = (0 << 30),
		};

		/**
		 * Information about a received packet, used internally
		 */
		struct Rx_packet_info
		{
			size_t size;

			Rx_packet_info(uint32_t status)
			: size((status & 0x3fff0000) >> 16)
			{ }
		};

		Platform::Device::Mmio<0> &_mmio;
		Platform::Device::Irq     &_irq;
		volatile uint32_t *_reg_base { _mmio.local_addr<uint32_t>() };
		Timer::Connection          _timer;
		Nic::Mac_address           _mac_addr { };

		/**
		 * Read 32-bit wide MMIO register
		 */
		uint32_t _reg_read(Register reg)
		{
			return _reg_base[reg >> 2];
		}

		void _reg_write(Register reg, uint32_t value)
		{
			_reg_base[reg >> 2] = value;
		}

		/**
		 * Return true of MAC CSR is busy
		 */
		bool _mac_csr_busy()
		{
			return _reg_read(MAC_CSR_CMD) & MAC_CSR_CMD_BUSY;
		}

		/**
		 * Wait for the completion of a MAC CSR operation
		 */
		void _mac_csr_wait_ready()
		{
			for (unsigned i = 0; i < 10; i++) {
				if (!_mac_csr_busy())
					return;
				_timer.msleep(10);
			}

			error("timeout while waiting for completeness of MAC CSR access");
		}

		/**
		 * Read MAC control / status register
		 *
		 * The MAC CSRs are accessed indirectly via 'MAC_CSR_CMD' and
		 * 'MAC_CSR_DATA'.
		 */
		uint32_t _mac_csr_read(Mac_register reg)
		{
			_reg_write(MAC_CSR_CMD, uint32_t(reg) | MAC_CSR_CMD_READ | MAC_CSR_CMD_BUSY);
			_mac_csr_wait_ready();
			return _reg_read(MAC_CSR_DATA);
		}

		/**
		 * Write MAC control / status register
		 */
		void _mac_csr_write(Mac_register reg, uint32_t value)
		{
			_reg_write(MAC_CSR_DATA, value);
			_reg_write(MAC_CSR_CMD, uint32_t(reg) | MAC_CSR_CMD_WRITE | MAC_CSR_CMD_BUSY);
			_mac_csr_wait_ready();
		}

		/**
		 * Reset device
		 *
		 * \return true on success
		 */
		bool _soft_reset()
		{
			enum { HW_CFG_SRST = (1 << 0) };
			_reg_write(HW_CFG, HW_CFG_SRST);

			for (unsigned i = 0; i < 10; i++) {
				_timer.msleep(10);
				if (!(_reg_read(HW_CFG) & HW_CFG_SRST))
					return true;
			}
			return false;
		}

		/**
		 * Return true if the NIC has least one incoming packet pending
		 */
		bool _rx_packet_avail()
		{
			return _reg_read(RX_FIFO_INF) & 0xff0000;
		}

		/**
		 * Acknowledge packet at the NIC
		 */
		Rx_packet_info _rx_packet_info()
		{
			return Rx_packet_info(_reg_read(RX_STATUS_FIFO));
		}

		/**
		 * Return number of bytes currently available in rx data fifo
		 */
		size_t _rx_data_pending()
		{
			return _reg_read(RX_FIFO_INF) & 0xffff;
		}

		void _drv_tx_transmit_pkt(size_t          packet_size,
		                          uint32_t const *src,
		                          bool           &continue_sending,
		                          bool           &ack_packet)
		{
			/* limit size to 11 bits, the maximum supported by lan9118 */
			enum { MAX_PACKET_SIZE_LOG2 = 11 };
			size_t const max_size = (1 << MAX_PACKET_SIZE_LOG2) - 1;
			if (packet_size > max_size) {
				error("packet size ", packet_size, " too large, "
				      "limit is ", max_size);

				continue_sending = true;
				ack_packet = false;
				return;
			}

			enum { FIRST_SEG = (1 << 13),
			       LAST_SEG  = (1 << 12) };

			uint32_t const cmd_a = packet_size | FIRST_SEG | LAST_SEG,
			               cmd_b = packet_size;

			unsigned count = align_addr(packet_size, 2) >> 2;

			/* check space left in tx data fifo */
			size_t const fifo_avail = _reg_read(TX_FIFO_INF) & 0xffff;
			if (fifo_avail < count*4 + sizeof(cmd_a) + sizeof(cmd_b)) {
				error("tx fifo overrun, ignore packet");
				continue_sending = false;
				ack_packet = true;
				return;
			}

			_reg_write(TX_DATA_FIFO, cmd_a);
			_reg_write(TX_DATA_FIFO, cmd_b);

			/* supply payload to transmit fifo */
			for (; count--; src++)
				_reg_write(TX_DATA_FIFO, *src);

			continue_sending = true;
			ack_packet = true;
			return;
		}

		void _finish_handle_irq()
		{
			/* acknowledge all pending irqs */
			_reg_write(INT_STS, ~0);

			_irq.ack();
		}

		void _drv_rx_copy_pkt(size_t    size,
		                      uint32_t *dst)
		{
			/* calculate number of words to be read from rx fifo */
			size_t count = min(size, _rx_data_pending()) >> 2;

			/* copy payload from rx fifo to client buffer */
			for (; count--; )
				*dst++ = _reg_read(RX_DATA_FIFO);
		}

		size_t _drv_rx_pkt_size()
		{
			/* read packet from NIC, copy to client buffer */
			Rx_packet_info packet = _rx_packet_info();

			/* align size to 32-bit boundary */
			return align_addr(packet.size, 2);
		}

	public:

		/**
		 * Exception type
		 */
		class Device_not_supported { };

		Lan9118_base(Platform::Device::Mmio<0> &mmio,
		             Platform::Device::Irq     &irq,
		             Signal_context_capability  irq_handler,
		             Env                       &env)
		:
			_mmio(mmio), _irq(irq), _timer(env)
		{
			_irq.sigh(irq_handler);

			unsigned long const id_rev     = _reg_read(ID_REV),
			                    byte_order = _reg_read(BYTE_TEST);

			log("id/rev:      ", Hex(id_rev));
			log("byte order:  ", Hex(byte_order));

			enum { EXPECTED_BYTE_ORDER = 0x87654321 };
			if (byte_order != EXPECTED_BYTE_ORDER) {
				error("invalid byte order, expected ", Hex(EXPECTED_BYTE_ORDER));
				throw Device_not_supported();
			}

			enum { EXPECTED_ID = 0x01180000 };
			if ((id_rev & 0xffff0000) != EXPECTED_ID) {
				error("device ID not supported, expected ", Hex(EXPECTED_ID));
				throw Device_not_supported();
			}

			if (!_soft_reset()) {
				error("soft reset timed out");
				throw Device_not_supported();
			}

			/* print MAC address */
			unsigned mac_addr_lo = _mac_csr_read(MAC_ADDRL),
			         mac_addr_hi = _mac_csr_read(MAC_ADDRH);

			_mac_addr.addr[5] = (uint8_t)(mac_addr_hi >>  8);
			_mac_addr.addr[4] = (uint8_t)(mac_addr_hi >>  0);
			_mac_addr.addr[3] = (uint8_t)(mac_addr_lo >> 24);
			_mac_addr.addr[2] = (uint8_t)(mac_addr_lo >> 16);
			_mac_addr.addr[1] = (uint8_t)(mac_addr_lo >>  8);
			_mac_addr.addr[0] = (uint8_t)(mac_addr_lo >>  0);

			log("MAC address: ", _mac_addr);

			/* configure MAC */
			enum { MAC_CR_TXEN = (1 << 3),
			       MAC_CR_RXEN = (1 << 2) };
			_mac_csr_write(MAC_CR, MAC_CR_TXEN | MAC_CR_RXEN);

			enum { TX_CFG_TX_ON = (1 << 1),
			       TX_CFG_TXSAO = (1 << 2), };

			/* enable transmitter, let nic ignore the tx status fifo */
			_reg_write(TX_CFG, TX_CFG_TX_ON | TX_CFG_TXSAO);

			/* reset interrupt state */
			_reg_write(INT_EN,   0);   /* disable */
			_reg_write(INT_STS, ~0);   /* acknowledge all pending irqs */

			/* enable interrupts for reception */
			enum { INT_EN_RSFL   = (1 <<  3),
			       INT_EN_RXSTOP = (1 << 24),
			       INT_EN_SW     = (1 << 31) };
			_reg_write(INT_EN, INT_EN_SW | INT_EN_RSFL | INT_EN_RXSTOP);

			/* enable interrupts at 'IRQ_CFG' */
			enum { IRQ_CFG_EN   = (1 << 8),
			       IRQ_CFG_POL  = (1 << 4), /* active high irq polarity */
			       IRQ_CFG_TYPE = (1 << 0)  /* not open drain */ };
			_reg_write(IRQ_CFG, IRQ_CFG_EN | IRQ_CFG_POL | IRQ_CFG_TYPE);
		}

		virtual ~Lan9118_base()
		{
			log("disable NIC");

			/* disable transmitter */
			_reg_write(TX_CFG, 0);

			/* disable rx and tx at MAX */
			_mac_csr_write(MAC_CR, 0);
		}
};


class Uplink_client : public Signal_handler<Uplink_client>,
                      public Lan9118_base,
                      public Uplink_client_base
{
	private:

		/************************
		 ** Uplink_client_base **
		 ************************/

		Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) override
		{
			bool continue_sending { false };
			bool ack_packet       { false };
			_drv_tx_transmit_pkt(
				conn_rx_pkt_size,
				(uint32_t const*)conn_rx_pkt_base,
				continue_sending,
				ack_packet);

			/*
			 * FIXME This seems to be a bug in the original (NIC client)
			 *       driver. When having packets that don't fit the maximum
			 *       transmittable size (limited by hardware) the driver
			 *       didn't acknowledge the packet but continued forwarding
			 *       packets from the session to the driver. Normally, we
			 *       would expect the driver to acknowledge the packet without
			 *       transmitting it (i.e., reject it).
			 */
			if (!ack_packet && continue_sending) {
				return Transmit_result::REJECTED;
			}
			/*
			 * FIXME This seems to be a bug in the original (NIC client)
			 *       driver. When the hardware TX FIFO doesn't have enough
			 *       space left to transmit the packet, the driver acknowledge
			 *       the packet but didn't continue forwarding packets from
			 *       the session to the hardware. Normally, we would expect
			 *       the driver to not acknowledge the packet nor continue
			 *       forwarding other packets (i.e., retry later).
			 */
			if (ack_packet && !continue_sending) {
				return Transmit_result::RETRY;
			}
			if (ack_packet && continue_sending) {
				return Transmit_result::ACCEPTED;
			}
			class Unexpected_transmit_result { };
			throw Unexpected_transmit_result { };
		}

		void _handle_irq()
		{
			while (_rx_packet_avail()) {

				_drv_rx_handle_pkt(
					_drv_rx_pkt_size(),
					[&] (void   *conn_tx_pkt_base,
					     size_t &conn_tx_pkt_size)
				{
					_drv_rx_copy_pkt(
						conn_tx_pkt_size,
						(uint32_t *)conn_tx_pkt_base);

					return Write_result::WRITE_SUCCEEDED;
				});
			}
			_finish_handle_irq();
		}

	public:

		Uplink_client(Env                       &env,
		              Allocator                 &alloc,
		              Platform::Device::Mmio<0> &mmio,
		              Platform::Device::Irq     &irq)
		:
			Signal_handler<Uplink_client> { env.ep(), *this, &Uplink_client::_handle_irq },
			Lan9118_base                  { mmio, irq, *this, env },
			Uplink_client_base            { env, alloc, _mac_addr }
		{
			_drv_handle_link_state(true);
		}
};

#endif /* _DRIVERS__NIC__SPEC__LAN9118__LAN9118_H_ */
