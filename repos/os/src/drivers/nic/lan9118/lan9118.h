/*
 * \brief  LAN9118 NIC driver
 * \author Norman Feske
 * \date   2011-05-21
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LAN9118_H_
#define _LAN9118_H_

#include <base/printf.h>
#include <util/misc_math.h>
#include <os/attached_io_mem_dataspace.h>
#include <os/irq_activation.h>
#include <timer_session/connection.h>
#include <nic/component.h>

class Lan9118 : public Nic::Session_component,
                public Genode::Irq_handler
{
	private:

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

		Genode::Attached_io_mem_dataspace _mmio;
		volatile Genode::uint32_t        *_reg_base;
		Timer::Connection                 _timer;
		Nic::Mac_address                  _mac_addr;

		enum { IRQ_STACK_SIZE = 4096 };
		Genode::Irq_activation _irq_activation;

		/**
		 * Information about a received packet, used internally
		 */
		struct Rx_packet_info
		{
			Genode::size_t size;

			Rx_packet_info(Genode::uint32_t status)
			: size((status & 0x3fff0000) >> 16)
			{ }
		};

		/**
		 * Read 32-bit wide MMIO register
		 */
		Genode::uint32_t _reg_read(Register reg)
		{
			return _reg_base[reg >> 2];
		}

		void _reg_write(Register reg, Genode::uint32_t value)
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

			PERR("timeout while waiting for completeness of MAC CSR access");
		}

		/**
		 * Read MAC control / status register
		 *
		 * The MAC CSRs are accessed indirectly via 'MAC_CSR_CMD' and
		 * 'MAC_CSR_DATA'.
		 */
		Genode::uint32_t _mac_csr_read(Mac_register reg)
		{
			_reg_write(MAC_CSR_CMD, reg | MAC_CSR_CMD_READ | MAC_CSR_CMD_BUSY);
			_mac_csr_wait_ready();
			return _reg_read(MAC_CSR_DATA);
		}

		/**
		 * Write MAC control / status register
		 */
		void _mac_csr_write(Mac_register reg, Genode::uint32_t value)
		{
			_reg_write(MAC_CSR_DATA, value);
			_reg_write(MAC_CSR_CMD, reg | MAC_CSR_CMD_WRITE | MAC_CSR_CMD_BUSY);
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
		Genode::size_t _rx_data_pending()
		{
			return _reg_read(RX_FIFO_INF) & 0xffff;
		}

	protected:

		bool _send()
		{
			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Genode::Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.valid()) {
				PWRN("Invalid tx packet");
				return true;
			}

			/* limit size to 11 bits, the maximum supported by lan9118 */
			enum { MAX_PACKET_SIZE_LOG2 = 11 };
			Genode::size_t const max_size = (1 << MAX_PACKET_SIZE_LOG2) - 1;
			if (packet.size() > max_size) {
				PERR("packet size %zd too large, limit is %zd", packet.size(), max_size);
				return true;
			}

			enum { FIRST_SEG = (1 << 13),
			       LAST_SEG  = (1 << 12) };

			Genode::uint32_t const cmd_a = packet.size() | FIRST_SEG | LAST_SEG,
			                       cmd_b = packet.size();

			unsigned count = Genode::align_addr(packet.size(), 2) >> 2;
			Genode::uint32_t *src = (Genode::uint32_t *)_tx.sink()->packet_content(packet);

			/* check space left in tx data fifo */
			Genode::size_t const fifo_avail = _reg_read(TX_FIFO_INF) & 0xffff;
			if (fifo_avail < count*4 + sizeof(cmd_a) + sizeof(cmd_b)) {
				PERR("tx fifo overrun, ignore packet");
				_tx.sink()->acknowledge_packet(packet);
				return false;
			}

			_reg_write(TX_DATA_FIFO, cmd_a);
			_reg_write(TX_DATA_FIFO, cmd_b);

			/* supply payload to transmit fifo */
			for (; count--; src++)
				_reg_write(TX_DATA_FIFO, *src);

			_tx.sink()->acknowledge_packet(packet);
			return true;
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			while (_send()) ;
		}


	public:

		/**
		 * Exception type
		 */
		class Device_not_supported { };

		/**
		 * Constructor
		 *
		 * \throw  Device_not_supported
		 */
		Lan9118(Genode::addr_t mmio_base, Genode::size_t mmio_size, int irq,
		        Genode::size_t const tx_buf_size,
		        Genode::size_t const rx_buf_size,
		        Genode::Allocator   &rx_block_md_alloc,
		        Genode::Ram_session &ram_session,
		        Server::Entrypoint  &ep)
		: Session_component(tx_buf_size, rx_buf_size, rx_block_md_alloc, ram_session, ep),
			_mmio(mmio_base, mmio_size),
			_reg_base(_mmio.local_addr<Genode::uint32_t>()),
			_irq_activation(irq, *this, IRQ_STACK_SIZE)
		{
			unsigned long const id_rev     = _reg_read(ID_REV),
			                    byte_order = _reg_read(BYTE_TEST);

			PINF("id/rev:      0x%lx", id_rev);
			PINF("byte order:  0x%lx", byte_order);

			enum { EXPECTED_BYTE_ORDER = 0x87654321 };
			if (byte_order != EXPECTED_BYTE_ORDER) {
				PERR("invalid byte order, expected 0x%x", EXPECTED_BYTE_ORDER);
				throw Device_not_supported();
			}

			enum { EXPECTED_ID = 0x01180000 };
			if ((id_rev & 0xffff0000) != EXPECTED_ID) {
				PERR("device ID not supported, expected 0x%x", EXPECTED_ID);
				throw Device_not_supported();
			}

			if (!_soft_reset()) {
				PERR("soft reset timed out");
				throw Device_not_supported();
			}

			/* print MAC address */
			unsigned mac_addr_lo = _mac_csr_read(MAC_ADDRL),
			         mac_addr_hi = _mac_csr_read(MAC_ADDRH);

			_mac_addr.addr[5] = (mac_addr_hi >>  8) & 0xff;
			_mac_addr.addr[4] = (mac_addr_hi >>  0) & 0xff;
			_mac_addr.addr[3] = (mac_addr_lo >> 24) & 0xff;
			_mac_addr.addr[2] = (mac_addr_lo >> 16) & 0xff;
			_mac_addr.addr[1] = (mac_addr_lo >>  8) & 0xff;
			_mac_addr.addr[0] = (mac_addr_lo >>  0) & 0xff;

			PINF("MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
			     _mac_addr.addr[5], _mac_addr.addr[4], _mac_addr.addr[3],
			     _mac_addr.addr[2], _mac_addr.addr[1], _mac_addr.addr[0]);

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

		/**
		 * Destructor
		 */
		~Lan9118()
		{
			PINF("disable NIC");
			/* disable transmitter */
			_reg_write(TX_CFG, 0);

			/* disable rx and tx at MAX */
			_mac_csr_write(MAC_CR, 0);
		}

		/**************************************
		 ** Nic::Session_component interface **
		 **************************************/

		Nic::Mac_address mac_address() override
		{
			return _mac_addr;
		}

		bool link_state() override
		{
			/* XXX always return true for now */
			return true;
		}


		/******************************
		 ** Irq_activation interface **
		 ******************************/

		void handle_irq(int)
		{
			using namespace Genode;

			_handle_packet_stream();

			while (_rx_packet_avail() && _rx.source()->ready_to_submit()) {

				/* read packet from NIC, copy to client buffer */
				Rx_packet_info packet = _rx_packet_info();

				/* align size to 32-bit boundary */
				size_t const size = align_addr(packet.size, 2);

				/* allocate rx packet buffer */
				Nic::Packet_descriptor p;
				try {
					p = _rx.source()->alloc_packet(size);
				} catch (Session::Rx::Source::Packet_alloc_failed) { return; }

				uint32_t *dst = (uint32_t *)_rx.source()->packet_content(p);

				/* calculate number of words to be read from rx fifo */
				size_t count = min(size, _rx_data_pending()) >> 2;

				/* copy payload from rx fifo to client buffer */
				for (; count--; )
					*dst++ = _reg_read(RX_DATA_FIFO);

				_rx.source()->submit_packet(p);
			}

			/* acknowledge all pending irqs */
			_reg_write(INT_STS, ~0);
		}
};

#endif /* _LAN9118_H_ */
