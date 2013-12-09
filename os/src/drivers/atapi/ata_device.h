/*
 * \brief  ATA device class
 * \author Sebastian.Sump <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ATA_DEVICE_H_
#define _ATA_DEVICE_H_

#include <base/exception.h>
#include <base/stdint.h>
#include <block/component.h>

namespace Genode {
	class Io_port_session;
	class Irq_connection;
}

namespace Ata {

	class Bus_master;

	class Device : public Block::Driver
	{

			/* I/O back end may access private data (see io.cc) */
			friend Genode::Io_port_session *get_io(Device *dev);
			friend Genode::Irq_connection  *get_irq(Device *dev);
			friend Bus_master              *get_bus_master(Device *dev);

		protected:

			unsigned char            _dev_num;
			Genode::Io_port_session *_pio;
			Genode::Irq_connection  *_irq;
			Bus_master              *_bus_master;
			bool                     _dma;
			unsigned                 _block_start;
			unsigned                 _block_end;
			unsigned                 _block_size;
			bool                     _lba48;
			bool                     _host_protected_area;

		protected:

			void probe_dma();

			void set_bus_master(bool secondary);
			void set_irq(unsigned irq);
			void set_dev_num(unsigned char dev_num) { _dev_num = dev_num; }

			unsigned  char dev_num() { return _dev_num; }
			Bus_master *bus_master(){ return _bus_master; }
			Genode::Irq_connection *irq() { return _irq; }
			Genode::Io_port_session *io() { return _pio; }

			virtual void _read(Block::sector_t block_number,
			                   Genode::size_t  block_count,
			                   char           *out_buffer,
			                   bool            dma);
			virtual void _write(Block::sector_t block_number,
			                    Genode::size_t  block_count,
			                    char const     *buffer,
			                    bool            dma);

		public:

			class Exception      : public ::Genode::Exception { };
			class Io_error       : public Exception { };

			/**
			 * Constructor
			 *
			 * \param base_cmd  I/O port base of command registers
			 * \param base_ctrl I/O prot base of control registers
			 */
			Device(unsigned base_cmd, unsigned base_ctrl);
			~Device();

			/**
			 * Return and set current device
			 */
			static Device * current(Device *dev = 0)
			{
				static Device *_current = 0;
				if (dev) _current = dev;
				return _current;
			}

			/**
			 * Probe legacy bus for device class
			 *
			 * \param search_type REG_CONFIG_TYPE_ATA or REG_CONFIG_TYPE_ATAPI
			 */
			static Device * probe_legacy(int search_type);

			/**
			 * Read block size and block count (from device),
			 * updates block_count and block_size
			 */
			virtual void read_capacity();


			/*******************************
			 **  Block::Driver interface  **
			 *******************************/

			Block::sector_t block_count() {
				return _block_end - _block_start + 1; }
			Genode::size_t block_size() { return _block_size; }

			virtual Block::Session::Operations ops();

			void read(Block::sector_t block_number,
			          Genode::size_t  block_count,
			          char           *buffer,
			          Block::Packet_descriptor &packet)
			{
				_read(block_number, block_count, buffer, false);
				session->ack_packet(packet);
			}

			void write(Block::sector_t block_number,
			           Genode::size_t  block_count,
			           char const     *buffer,
			           Block::Packet_descriptor &packet)
			{
				_write(block_number, block_count, buffer, false);
				session->ack_packet(packet);
			}

			void read_dma(Block::sector_t block_number,
			              Genode::size_t  block_count,
			              Genode::addr_t  phys,
			              Block::Packet_descriptor &packet)
			{
				_read(block_number, block_count, (char*)phys, true);
				session->ack_packet(packet);
			}

			void write_dma(Block::sector_t block_number,
			               Genode::size_t  block_count,
			               Genode::addr_t  phys,
			               Block::Packet_descriptor &packet)
			{
				_write(block_number, block_count, (char*)phys, true);
				session->ack_packet(packet);
			}

			bool dma_enabled() { return _dma; }
	};


	class Atapi_device : public Device
	{
		private:

			int  read_sense(unsigned char *sense, int length);
			void read_capacity();

			void _read(Block::sector_t block_number,
			           Genode::size_t  block_count,
			           char           *out_buffer,
			           bool            dma);
			void _write(Block::sector_t block_number,
			            Genode::size_t  block_count,
			            char const     *buffer,
			            bool            dma);

		public:

			Atapi_device(unsigned base_cmd, unsigned base_ctrl);

			bool test_unit_ready(int level = 0);
			Block::Session::Operations ops();
	};
}
#endif
