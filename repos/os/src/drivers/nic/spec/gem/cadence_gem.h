/*
 * \brief  Base EMAC driver for the Xilinx EMAC PS used on Zynq devices
 * \author Johannes Schlatow
 * \author Timo Wischer
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__NIC__XILINX_EMACPS_BASE_H_
#define _INCLUDE__DRIVERS__NIC__XILINX_EMACPS_BASE_H_

/* Genode includes */
#include <os/attached_mmio.h>
#include <nic_session/nic_session.h>
#include <irq_session/connection.h>
#include <timer_session/connection.h>
#include <nic/component.h>

/* local includes */
#include "system_control.h"
#include "tx_buffer_descriptor.h"
#include "rx_buffer_descriptor.h"
#include "marvell_phy.h"


namespace Genode
{
	/**
	 * Base driver Xilinx EMAC PS module
	 */
	class Cadence_gem
	:
		private Genode::Attached_mmio,
		public Nic::Session_component,
		public Phyio
	{
		private:
			/**
			* Control register
			*/
			struct Control : Register<0x00, 32>
			{
				struct Local_loopback  : Bitfield<1, 1> {};
				struct Rx_en  : Bitfield<2, 1> {};
				struct Tx_en  : Bitfield<3, 1> {};
				struct Mgmt_port_en  : Bitfield<4, 1> {};
				struct Clear_statistics  : Bitfield<5, 1> {};
				struct Start_tx  : Bitfield<9, 1> {};

				static access_t init() {
					return Mgmt_port_en::bits(1) |
						Tx_en::bits(1) |
						Rx_en::bits(1);
				}

				static access_t start_tx() {
					return (init() | Start_tx::bits(1));
				}
			};

			/**
			* Config register
			*/
			struct Config : Register<0x04, 32>
			{
				struct Speed_100  : Bitfield<0, 1> {};
				struct Full_duplex  : Bitfield<1, 1> {};
				struct Copy_all  : Bitfield<4, 1> {};
				struct No_broadcast  : Bitfield<5, 1> {};
				struct Multi_hash_en  : Bitfield<6, 1> {};
				struct Gige_en  : Bitfield<10, 1> {};
				struct Fcs_remove  : Bitfield<17, 1> {};
				struct Mdc_clk_div  : Bitfield<18, 3> {
					enum {
						DIV_32 = 0b010,
						DIV_224 = 0b111,
					};
				};
				struct Ignore_rx_fcs  : Bitfield<26, 1> {};
			};

			/**
			* Status register
			*/
			struct Status : Register<0x08, 32>
			{
				struct Phy_mgmt_idle  : Bitfield<2, 1> {};
			};



			/**
			* DMA Config register
			*/
			struct Dma_config : Register<0x10, 32>
			{
				struct Rx_pktbuf_memsz_sel  : Bitfield<8, 2> {
					enum {
						SPACE_8KB = 0x3,
					};
				};
				struct Tx_pktbuf_memsz_sel  : Bitfield<10, 1> {
					enum {
						SPACE_4KB = 0x1,
					};
				};
				struct Ahb_mem_rx_buf_size  : Bitfield<16, 8> {
					enum {
						BUFFER_1600B = 0x19,
					};
				};

				static access_t init()
				{
					return Ahb_mem_rx_buf_size::bits(Ahb_mem_rx_buf_size::BUFFER_1600B) |
						Rx_pktbuf_memsz_sel::bits(Rx_pktbuf_memsz_sel::SPACE_8KB) |
						Tx_pktbuf_memsz_sel::bits(Tx_pktbuf_memsz_sel::SPACE_4KB);
				}

				// TODO possibly enable transmition check sum offloading
			};

			/**
			* Tx_Status register
			*/
			struct Tx_status : Register<0x14, 32>
			{
				struct Tx_complete  : Bitfield<5, 1> {};
				struct Tx_go  : Bitfield<3, 1> {};
			};

			/**
			* Receiving queue base addresse register
			*/
			struct Rx_qbar : Register<0x18, 32>
			{
				struct Addr : Bitfield<0, 32> {};
			};

			/**
			* Transmition queue base addresse register
			*/
			struct Tx_qbar : Register<0x1C, 32>
			{
				struct Addr : Bitfield<0, 32> {};
			};


			/**
			* Receive status register
			*/
			struct Rx_status : Register<0x20, 32>
			{
				struct Frame_reveived : Bitfield<1, 1> {};
				struct Buffer_not_available : Bitfield<0, 1> {};
			};


			/**
			* Interrupt status register
			*/
			struct Interrupt_status : Register<0x24, 32>
			{
				struct Rx_used_read : Bitfield<3, 1> {};
				struct Rx_complete : Bitfield<1, 1> {};
			};


			/**
			* Interrupt enable register
			*/
			struct Interrupt_enable : Register<0x28, 32>
			{
				struct Rx_complete : Bitfield<1, 1> {};
			};


			/**
			* Interrupt disable register
			*/
			struct Interrupt_disable : Register<0x2C, 32>
			{
				struct Rx_complete : Bitfield<1, 1> {};
			};


			/**
			* PHY maintenance register
			*/
			struct Phy_maintenance : Register<0x34, 32>
			{
				struct Clause_22 : Bitfield<30, 1> {};
				struct Operation : Bitfield<28, 2> {
					enum Type {
						READ = 0b10,
						WRITE = 0b01
					};
				};
				struct Phy_addr : Bitfield<23, 5> {};
				struct Reg_addr : Bitfield<18, 5> {};
				struct Must_10 : Bitfield<16, 2> {
					enum { INIT = 0b10 };
				};
				struct Data : Bitfield<0, 16> {};
			};


			/**
			* MAC hash register
			*/
			struct Hash_register : Register<0x80, 64>
			{
				struct Low_hash   : Bitfield<0, 32> { };
				struct High_hash   : Bitfield<32, 16> { };
			};


			/**
			* MAC Addresse
			*/
			struct Mac_addr_1 : Register<0x88, 64>
			{
				struct Low_addr   : Bitfield<0, 32> { };
				struct High_addr   : Bitfield<32, 16> { };
			};


			/**
			* Counter for the successfully transmitted frames
			*/
			struct Frames_transmitted : Register<0x108, 32>
			{
				struct Counter : Bitfield<0, 32> { };
			};


			/**
			* Counter for the successfully received frames
			*/
			struct Frames_received : Register<0x158, 32>
			{
				struct Counter : Bitfield<0, 32> { };
			};


			/**
			* Counter for the successfully received frames
			*/
			struct Rx_overrun_errors : Register<0x1A4, 32>
			{
				struct Counter : Bitfield<0, 10> { };
			};


			class Phy_timeout_for_idle : public Genode::Exception {};
			class Unkown_ethernet_speed : public Genode::Exception {};


			Timer::Connection                   _timer;
			System_control                      _sys_ctrl;
			Tx_buffer_descriptor                _tx_buffer;
			Rx_buffer_descriptor                _rx_buffer;
			Genode::Irq_connection              _irq;
			Genode::Signal_handler<Cadence_gem> _irq_handler;
			Marvel_phy                          _phy;


			void _init()
			{
				// TODO checksum offloading and pause frames
				/* see 16.3.2 Configure the Controller */

				/* 1. Program the Network Configuration register (gem.net_cfg) */
				write<Config>(
					Config::Speed_100::bits(1) |
					Config::Full_duplex::bits(1) |
					Config::Multi_hash_en::bits(1) |
					Config::Mdc_clk_div::bits(Config::Mdc_clk_div::DIV_32) |
					Config::Fcs_remove::bits(1)
				);


				write<Rx_qbar>( _rx_buffer.phys_addr() );
				write<Tx_qbar>( _tx_buffer.phys_addr() );


				/* 3. Program the DMA Configuration register (gem.dma_cfg) */
				write<Dma_config>( Dma_config::init() );


				/*
				 * 4. Program the Network Control Register (gem.net_ctrl)
				 * Enable MDIO, transmitter and receiver
				 */
				write<Control>(Control::init());



				_phy.init();

				/* change emac clocks depending on phy autonegation result */
				uint32_t rclk = 0;
				uint32_t clk = 0;
				switch (_phy.eth_speed()) {
				case SPEED_1000:
					write<Config::Gige_en>(1);
					rclk = (0 << 4) | (1 << 0);
					clk = (1 << 20) | (8 << 8) | (0 << 4) | (1 << 0);
					break;
				case SPEED_100:
					write<Config::Gige_en>(0);
					write<Config::Speed_100>(1);
					rclk = 1 << 0;
					clk = (5 << 20) | (8 << 8) | (0 << 4) | (1 << 0);
					break;
				case SPEED_10:
					write<Config::Gige_en>(0);
					write<Config::Speed_100>(0);
					rclk = 1 << 0;
					/* FIXME untested */
					clk = (5 << 20) | (8 << 8) | (0 << 4) | (1 << 0);
					break;
				default:
					throw Unkown_ethernet_speed();
				}
				_sys_ctrl.set_clk(clk, rclk);


				/* 16.3.6 Configure Interrupts */
				write<Interrupt_enable>(Interrupt_enable::Rx_complete::bits(1));
			}

			void _deinit()
			{
				/* 16.3.1 Initialize the Controller */

				/* Disable all interrupts */
				write<Interrupt_disable>(0x7FFFEFF);

				/* Disable the receiver & transmitter */
				write<Control>(0);
				write<Control>(Control::Clear_statistics::bits(1));

				write<Tx_status>(0xFF);
				write<Rx_status>(0x0F);
				write<Phy_maintenance>(0);

				write<Rx_qbar>(0);
				write<Tx_qbar>(0);

				/* Clear the Hash registers for the mac address
				 * pointed by AddressPtr
				 */
				write<Hash_register>(0);
			}


			void _mdio_wait()
			{
				uint32_t timeout = 200;

				/* Wait till MDIO interface is ready to accept a new transaction. */
				while (!read<Status::Phy_mgmt_idle>()) {
					if (timeout <= 0) {
						PWRN("%s: Timeout\n", __func__);
						throw Phy_timeout_for_idle();
					}

					_timer.msleep(1);
					timeout--;
				}
			}


			void _phy_setup_op(const uint8_t phyaddr, const uint8_t regnum, const uint16_t data, const Phy_maintenance::Operation::Type op)
			{
				_mdio_wait();

				/* Write mgtcr and wait for completion */
				write<Phy_maintenance>(
							Phy_maintenance::Clause_22::bits(1) |
							Phy_maintenance::Operation::bits(op) |
							Phy_maintenance::Phy_addr::bits(phyaddr) |
							Phy_maintenance::Reg_addr::bits(regnum) |
							Phy_maintenance::Must_10::bits(Phy_maintenance::Must_10::INIT) |
							Phy_maintenance::Data::bits(data) );

				_mdio_wait();
			}

			virtual void _handle_irq()
			{
				/* 16.3.9 Receiving Frames */
				/* read interrupt status, to detect the interrupt reasone */
				const Interrupt_status::access_t status = read<Interrupt_status>();
				const Rx_status::access_t rxStatus = read<Rx_status>();

				/* FIXME strangely, this handler is also called without any status bit set in Interrupt_status */
				if ( Interrupt_status::Rx_complete::get(status) ) {

					while (_rx_buffer.package_available()) {
						// TODO use this buffer directly as the destination for the DMA controller
						// to minimize the overrun errors
						const size_t buffer_size = _rx_buffer.package_length();

						/* allocate rx packet buffer */
						Nic::Packet_descriptor p;
						try {
							p = _rx.source()->alloc_packet(buffer_size);
						} catch (Session::Rx::Source::Packet_alloc_failed) { return; }

						char *dst = (char *)_rx.source()->packet_content(p);

						/*
						 * copy data from rx buffer to new allocated buffer.
						 * Has to be copied,
						 * because the extern allocater possibly is using the cache.
						 */
						if ( _rx_buffer.get_package(dst, buffer_size) != buffer_size ) {
							PWRN("Package not fully copiied. Package ignored.");
							break;
						}

						/* clearing error flags */
						write<Interrupt_status::Rx_used_read>(1);
						write<Rx_status::Buffer_not_available>(1);

						/* comit buffer to system services */
						_rx.source()->submit_packet(p);
					}

					/* check, if there was lost some packages */
					const uint16_t lost_packages = read<Rx_overrun_errors::Counter>();
					if (lost_packages > 0) {
						PWRN("%d packages lost (%d packages successfully received)!",
							 lost_packages, read<Frames_received>());
					}

					/* reset reveive complete interrupt */
					write<Rx_status>(Rx_status::Frame_reveived::bits(1));
					write<Interrupt_status>(Interrupt_status::Rx_complete::bits(1));
				}

				_irq.ack_irq();
			}

		public:
			/**
			 * Constructor
			 *
			 * \param  base       MMIO base address
			 */
			Cadence_gem(Genode::size_t const tx_buf_size,
			            Genode::size_t const rx_buf_size,
			            Genode::Allocator   &rx_block_md_alloc,
			            Genode::Ram_session &ram_session,
			            Server::Entrypoint  &ep,
			            addr_t const base, size_t const size, const int irq)
			:
				Genode::Attached_mmio(base, size),
				Session_component(tx_buf_size, rx_buf_size, rx_block_md_alloc, ram_session, ep),
				_irq(irq),
				_irq_handler(ep, *this, &Cadence_gem::_handle_irq),
				_phy(*this)
			{
				_irq.sigh(_irq_handler);
				_irq.ack_irq();
				_deinit();
				_init();
			}

			~Cadence_gem()
			{
				// TODO dsiable tx and rx and clean up irq registration
				_deinit();
			}

			using Nic::Session_component::cap;


			void phy_write(const uint8_t phyaddr, const uint8_t regnum, const uint16_t data)
			{
				_phy_setup_op(phyaddr, regnum, data, Phy_maintenance::Operation::WRITE);
			}


			void phy_read(const uint8_t phyaddr, const uint8_t regnum, uint16_t& data)
			{
				_phy_setup_op(phyaddr, regnum, 0, Phy_maintenance::Operation::READ);

				data = read<Phy_maintenance::Data>();
			}


			void mac_address(const Nic::Mac_address &mac)
			{
				const uint32_t* const low_addr_pointer = reinterpret_cast<const uint32_t*>(&mac.addr[0]);
				const uint16_t* const high_addr_pointer = reinterpret_cast<const uint16_t*>(&mac.addr[4]);

				write<Mac_addr_1::Low_addr>(*low_addr_pointer);
				write<Mac_addr_1::High_addr>(*high_addr_pointer);
			}


			bool _send()
			{
				if (!_tx.sink()->ready_to_ack())
					return false;

				if (!_tx.sink()->packet_avail())
					return false;

				Genode::Packet_descriptor packet = _tx.sink()->get_packet();
				if (!packet.size()) {
					PWRN("Invalid tx packet");
					return true;
				}

				char *src = (char *)_tx.sink()->packet_content(packet);

				_tx_buffer.add_to_queue(src, packet.size());
				write<Control>(Control::start_tx());

				_tx.sink()->acknowledge_packet(packet);
				return true;
			}


			/**************************************
			 ** Nic::Session_component interface **
			 **************************************/

			virtual Nic::Mac_address mac_address()
			{
				Nic::Mac_address mac;
				uint32_t* const low_addr_pointer = reinterpret_cast<uint32_t*>(&mac.addr[0]);
				uint16_t* const high_addr_pointer = reinterpret_cast<uint16_t*>(&mac.addr[4]);

				*low_addr_pointer = read<Mac_addr_1::Low_addr>();
				*high_addr_pointer = read<Mac_addr_1::High_addr>();

				return mac;
			}

			virtual bool link_state()
			{
				/* XXX return always true for now */
				return true;
			}

			void _handle_packet_stream() override
			{
				while (_rx.source()->ack_avail())
					_rx.source()->release_packet(_rx.source()->get_acked_packet());

				while (_send()) ;
			}
	};
}

#endif /* _INCLUDE__DRIVERS__NIC__XILINX_EMACPS_BASE_H_ */

