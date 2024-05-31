/*
 * \brief  VirtIO NIC driver component
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/signal.h>
#include <nic_session/nic_session.h>
#include <util/misc_math.h>
#include <util/register.h>
#include <virtio/queue.h>
#include <util/noncopyable.h>

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>

namespace Virtio_nic {
	using namespace Genode;
	struct Main;
	class Device;
	class Uplink_client;
}


class Virtio_nic::Device : Noncopyable
{
	public:

		/**
		 * See section 5.1.6 of VirtIO 1.0 specification.
		 */
		struct Virtio_net_header
		{
			enum Flags : Genode::uint16_t
			{
				NEEDS_CSUM = 1,
			};

			enum GSO : Genode::uint16_t
			{
				NONE  = 0,
				TCPV4 = 1,
				UDP   = 3,
				TCPV6 = 4,
				ECN   = 0x80,
			};

			uint8_t  flags        = 0;
			uint8_t  gso_type     = GSO::NONE;
			uint16_t hdr_len      = 0;
			uint16_t gso_size     = 0;
			uint16_t csum_start   = 0;
			uint16_t csum_offset  = 0;
			uint16_t num_buffers  = 0;
		};

	private:

		struct Unsupported_version  : Genode::Exception { };
		struct Device_init_failed   : Genode::Exception { };
		struct Features_init_failed : Genode::Exception { };
		struct Queue_init_failed    : Genode::Exception { };

		struct Hardware_features
		{
			Nic::Mac_address mac = { };
			bool link_status_available = false;
		};

		/**
		 * VirtIO feature bits relevant to this VirtIO net driver implementation.
		 */
		struct Features : Genode::Register<64>
		{
			struct CSUM            : Bitfield<0, 1> { };
			struct GUEST_CSUM      : Bitfield<1, 1> { };
			struct MTU             : Bitfield<3, 1> { };
			struct MAC             : Bitfield<5, 1> { };
			struct GSO             : Bitfield<6, 1> { };
			struct GUEST_TSO4      : Bitfield<7, 1> { };
			struct GUEST_TSO6      : Bitfield<8, 1> { };
			struct GUEST_ECN       : Bitfield<9, 1> { };
			struct GUEST_UFO       : Bitfield<10, 1> { };
			struct HOST_TSO4       : Bitfield<11, 1> { };
			struct HOST_TSO6       : Bitfield<12, 1> { };
			struct HOST_ECN        : Bitfield<13, 1> { };
			struct HOST_UFO        : Bitfield<14, 1> { };
			struct MRG_RXBUF       : Bitfield<15, 1> { };
			struct STATUS          : Bitfield<16, 1> { };
			struct CTRL_VQ         : Bitfield<17, 1> { };
			struct CTRL_RX         : Bitfield<18, 1> { };
			struct CTRL_VLAN       : Bitfield<19, 1> { };
			struct GUEST_ANNOUNCE  : Bitfield<21, 1> { };
			struct MQ              : Bitfield<22, 1> { };
			struct CTRL_MAC_ADDR   : Bitfield<23, 1> { };
			struct EVENT_IDX       : Bitfield<29, 1> { };
			struct VERSION_1       : Bitfield<32, 1> { };
		};

		/**
		 * See section 5.1.4 of VirtIO 1.0 specification.
		 */
		enum Config : uint8_t { CONFIG_MAC_BASE = 0, CONFIG_STATUS = 6 };
		enum { STATUS_LINK_UP = 1 << 0 };

		/**
		 * Available VirtIO queue numbers, see section 5.1.2 of VirtIO 1.0 specification.
		 */
		enum Vq_id : Genode::uint16_t { RX_VQ = 0, TX_VQ = 1 };

		/**
		 * Each VirtIO queue contains fixed number of buffers. The most common size of the buffer
		 * is 1526 bytes (size of ethernet frame + Virtio_net_header). VirtIO queue size must be
		 * a power of 2. Each VirtIO queue needs some additional space for the descriptor table,
		 * available and used rings. The default VirtIO queue parameter values defined here have
		 * been selected to make Ram_dataspace used by both TX and RX VirtIO queues consume around
		 * 256kB of RAM.
		 */
		static const uint16_t DEFAULT_VQ_SIZE = 64;
		static const uint16_t DEFAULT_VQ_BUF_SIZE = 2048;

		struct Rx_queue_traits
		{
			static const bool device_write_only = true;
			static const bool has_data_payload = true;
		};

		struct Tx_queue_traits
		{
			static const bool device_write_only = false;
			static const bool has_data_payload = true;
		};

		typedef Virtio::Queue<Virtio_net_header, Rx_queue_traits> Rx_queue_type;
		typedef Virtio::Queue<Virtio_net_header, Tx_queue_traits> Tx_queue_type;

		bool                const _verbose;
		Virtio::Device           &_device;
		Hardware_features   const _hw_features;
		Rx_queue_type             _rx_vq;
		Tx_queue_type             _tx_vq;

		void _init_virtio_device()
		{
			using Status = Virtio::Device::Status;

			if (!_device.set_status(Status::RESET)) {
				error("Failed to reset the device!");
				throw Device_init_failed();
			}

			if (!_device.set_status(Status::ACKNOWLEDGE)) {
				error("Failed to acknowledge the device!");
				throw Device_init_failed();
			}

			if (!_device.set_status(Status::DRIVER)) {
				_device.set_status(Status::FAILED);
				error("Device initialization failed!");
				throw Device_init_failed();
			}
		}

		static Nic::Mac_address _read_mac_address(Virtio::Device &device)
		{
			Nic::Mac_address mac = {};

			/* See section 2.3.1 of VirtIO 1.0 spec for rationale. */
			uint32_t before = 0, after = 0;
			do {
				before = device.get_config_generation();
				for (uint8_t idx = 0; idx < sizeof(mac.addr); ++idx)
					mac.addr[idx] =
						device.read_config<uint8_t>(CONFIG_MAC_BASE + idx);
				after = device.get_config_generation();
			} while (after != before);

			return mac;
		}

		Hardware_features _init_hw_features(Xml_node const &xml)
		{
			_init_virtio_device();

			using Status = Virtio::Device::Status;

			const uint32_t low = _device.get_features(0);
			const uint32_t high = _device.get_features(1);
			const Features::access_t device_features = ((uint64_t)high << 32) | low;
			Features::access_t driver_features = 0;

			/* This driver does not support legacy VirtIO versions. */
			if (!Features::VERSION_1::get(device_features)) {
				error("Unsupprted VirtIO device version!");
				throw Features_init_failed();
			}
			Features::VERSION_1::set(driver_features);

			Hardware_features hw_features;

			if (Features::MAC::get(device_features)) {
				Features::MAC::set(driver_features);
				hw_features.mac = _read_mac_address(_device);
			}

			hw_features.mac = xml.attribute_value("mac", hw_features.mac);

			if (hw_features.mac == Nic::Mac_address()) {
				error("HW mac address missing and not provided via config!");
				throw Features_init_failed();
			}

			if (Features::STATUS::get(device_features)) {
				Features::STATUS::set(driver_features);
				hw_features.link_status_available = true;
			}

			_device.set_features(0, (uint32_t)driver_features);
			_device.set_features(1, (uint32_t)(driver_features >> 32));

			if (!_device.set_status(Status::FEATURES_OK)) {
				_device.set_status(Status::FAILED);
				error("Device feature negotiation failed!");
				throw Features_init_failed();
			}

			return hw_features;
		}

		Genode::uint16_t _vq_size(Vq_id vq, Xml_node const &xml, char const *cfg_attr)
		{
			const uint16_t max_vq_size = _device.get_max_queue_size(vq);

			if (max_vq_size == 0) {
				error("VirtIO queue ", (int)vq, " is not available!");
				throw Queue_init_failed();
			}

			const uint16_t vq_size = Genode::min(xml.attribute_value(cfg_attr, DEFAULT_VQ_SIZE),
			                                     max_vq_size);

			if (_verbose)
				log("VirtIO queue ", (int)vq, " size: ", vq_size, " (max: ", max_vq_size, ")");

			return vq_size;
		}

		uint16_t _buf_size(Vq_id vq, Xml_node const &xml, char const *cfg_attr)
		{
			const uint16_t vq_buf_size = xml.attribute_value(cfg_attr, DEFAULT_VQ_BUF_SIZE );
			if (_verbose)
				log("VirtIO queue ", (int)vq, " buffer size: ", Number_of_bytes(vq_buf_size), "b");
			return vq_buf_size;
		}

		void _setup_virtio_queues()
		{
			if (!_device.configure_queue(RX_VQ, _rx_vq.description())) {
				error("Failed to initialize rx VirtIO queue!");
				throw Queue_init_failed();
			}

			if (!_device.configure_queue(TX_VQ, _tx_vq.description())) {
				error("Failed to initialize tx VirtIO queue!");
				throw Queue_init_failed();
			}

			using Status = Virtio::Device::Status;
			if (!_device.set_status(Status::DRIVER_OK)) {
				_device.set_status(Status::FAILED);
				error("Failed to initialize VirtIO queues!");
				throw Queue_init_failed();
			}
		}

	public:

		Device(Virtio::Device          &device,
		       Platform::Connection    &plat,
		       Genode::Xml_node  const &xml)
		try :
			_verbose     { xml.attribute_value("verbose", false) },
			_device      { device },
			_hw_features { _init_hw_features(xml) },
			_rx_vq       { plat,
			               _vq_size(RX_VQ, xml, "rx_queue_size"),
			               _buf_size(RX_VQ, xml, "rx_buffer_size") },
			_tx_vq       { plat,
			               _vq_size(TX_VQ, xml, "tx_queue_size"),
			               _buf_size(TX_VQ, xml, "tx_buffer_size") }
		{ }
		catch (Tx_queue_type::Invalid_buffer_size)
		{
			error("Invalid TX VirtIO queue buffer size specified!");
			throw;
		}
		catch (Rx_queue_type::Invalid_buffer_size)
		{
			error("Invalid RX VirtIO queue buffer size specified!");
			throw;
		}

		virtual ~Device()
		{
			_device.set_status(Virtio::Device::Status::RESET);
		}

		bool verbose() { return _verbose; }

		template <typename HANDLE_RX,
		          typename HANDLE_LINK_STATE>
		void drv_handle_irq(HANDLE_RX         && handle_rx,
		                    HANDLE_LINK_STATE && handle_link_state)
		{
			const uint32_t reasons = _device.read_isr();

			enum { IRQ_USED_RING_UPDATE = 1, IRQ_CONFIG_CHANGE = 2 };

			if (_tx_vq.has_used_buffers())
				_tx_vq.ack_all_transfers();

			if (reasons & IRQ_USED_RING_UPDATE) {
				handle_rx();
			}

			if ((reasons & IRQ_CONFIG_CHANGE) && _hw_features.link_status_available) {
				handle_link_state();
			}
			_device.irq_ack();
		}

		bool tx_vq_write_pkt(char     const *pkt_base,
		                     Genode::size_t  pkt_size)
		{
			Virtio_net_header hdr;
			return _tx_vq.write_data(hdr, pkt_base, pkt_size);
		}

		template <typename RECEIVE_PKT>
		void rx_vq_read_pkt(RECEIVE_PKT && rcv_pkt)
		{
			while (_rx_vq.has_used_buffers())
				_rx_vq.read_data(rcv_pkt);

			/**
			 * Inform the device the buffers we've just consumed are ready
			 * to used again
			 */
			_device.notify_buffers_available(RX_VQ);
		}

		void _finish_sent_packets()
		{
			_device.notify_buffers_available(TX_VQ);
		}

		void rx_vq_ack_pkts()
		{
			/* Reclaim all buffers processed by the device. */
			if (_tx_vq.has_used_buffers())
				_tx_vq.ack_all_transfers();
		}

		/**
		 * Tell the device we have some buffers for it to process and wait
		 * until its done with at least one of them.
		 */
		void tx_vq_flush()
		{
			_device.notify_buffers_available(TX_VQ);
			while (!_tx_vq.has_used_buffers());
		}

		bool read_link_state()
		{
			/**
			 * According to docs when STATUS feature is not available or has not
			 * been negotiated the driver should assume the link is always active.
			 * See section 5.1.4.2 of VIRTIO 1.0 specification.
			 */
			if (!_hw_features.link_status_available)
				return true;

			uint32_t before = 0, after = 0;
			uint8_t status = 0;
			do {
				before = _device.get_config_generation();
				status = _device.read_config<uint8_t>(CONFIG_STATUS);
				after = _device.get_config_generation();
			} while (after != before);

			return status & STATUS_LINK_UP;
		}

		Nic::Mac_address const &read_mac_address() const
		{
			return _hw_features.mac;
		}

		void init(Genode::Signal_context_capability irq_handler)
		{
			_setup_virtio_queues();
			_device.irq_sigh(irq_handler);
			_device.irq_ack();
		}
};


class Virtio_nic::Uplink_client : public Virtio_nic::Device,
                                  public Uplink_client_base
{
	private:

		Signal_handler<Uplink_client> _irq_handler;

		void _receive()
		{
			rx_vq_read_pkt(
				[&] (Virtio_net_header const &,
				     char              const *data,
				     size_t                   size)
			{
				_drv_rx_handle_pkt(
					size,
					[&] (void   *conn_tx_pkt_base,
						 size_t &conn_tx_pkt_size)
				{
					memcpy(conn_tx_pkt_base, data, conn_tx_pkt_size);
					return Write_result::WRITE_SUCCEEDED;
				});
				return true;
			});
		}

		void _handle_irq()
		{
			drv_handle_irq([&] () {

				_receive();

			}, [&] () {

				_drv_handle_link_state(read_link_state());
			});
		}


		/************************
		 ** Uplink_client_base **
		 ************************/

		Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) override
		{
			rx_vq_ack_pkts();
			if (!tx_vq_write_pkt(conn_rx_pkt_base, conn_rx_pkt_size)) {
				/*
				 * VirtIO transmit queue is full, flush it and retry sending the pkt.
				 */
				tx_vq_flush();

				if (!tx_vq_write_pkt(conn_rx_pkt_base, conn_rx_pkt_size)) {
					warning("Failed to send packet after flushing VirtIO queue!");
					return Transmit_result::REJECTED;
				}
			}

			return Transmit_result::ACCEPTED;
		}

		void _drv_finish_transmitted_pkts() override
		{
			_finish_sent_packets();
		}

	public:

		Uplink_client(Env                         &env,
		              Allocator                   &alloc,
		              Virtio::Device              &device,
		              Platform::Connection        &plat,
		              Genode::Xml_node      const &xml)
		:
			Device             { device, plat, xml },
			Uplink_client_base { env, alloc, read_mac_address() },
			_irq_handler       { env.ep(), *this, &Uplink_client::_handle_irq }
		{
			Virtio_nic::Device::init(_irq_handler);
			_drv_handle_link_state(read_link_state());
		}
};
