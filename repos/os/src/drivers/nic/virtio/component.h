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

/* Need to come before attached_rom_dataspace.h */
#include <nic/xml_node.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/signal.h>
#include <irq_session/client.h>
#include <nic/component.h>
#include <root/component.h>
#include <util/misc_math.h>
#include <util/register.h>
#include <virtio/queue.h>

namespace Virtio_nic {
	using namespace Genode;
	struct Main;
	class Root;
	class Session_component;
}


class Virtio_nic::Session_component : public Nic::Session_component
{
	private:

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

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
		enum { CONFIG_MAC_BASE = 0, CONFIG_STATUS = 6 };
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
		 * 32Kb of RAM.
		 */
		static const uint16_t DEFAULT_VQ_SIZE = 16;
		static const uint16_t DEFAULT_VQ_BUF_SIZE = 2020;

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
		typedef Genode::Signal_handler<Session_component>         Signal_handler;


		bool                const _verbose;
		Virtio::Device           &_device;
		Hardware_features   const _hw_features;
		Rx_queue_type             _rx_vq;
		Tx_queue_type             _tx_vq;
		Irq_session_client        _irq;
		Signal_handler            _irq_handler;
		bool                      _link_up = false;


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
				for (size_t idx = 0; idx < sizeof(mac.addr); ++idx) {
					mac.addr[idx] = device.read_config(
						CONFIG_MAC_BASE + idx, Virtio::Device::ACCESS_8BIT);
				}
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

		void _handle_irq()
		{
			const uint32_t reasons = _device.read_isr();

			enum { IRQ_USED_RING_UPDATE = 1, IRQ_CONFIG_CHANGE = 2 };

			if (_tx_vq.has_used_buffers())
				_tx_vq.ack_all_transfers();

			if (reasons & IRQ_USED_RING_UPDATE) {
				_receive();
			}

			if ((reasons & IRQ_CONFIG_CHANGE) && _hw_features.link_status_available &&
			    (link_state() != _link_up)) {
				_link_up = !_link_up;
				if (_verbose)
					log("Link status changed: ", (_link_up ? "on-line" : "off-line"));
				_link_state_changed();
			}

			_irq.ack_irq();
		}

		bool _send()
		{
			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			auto packet = _tx.sink()->get_packet();
			if (!packet.size() || !_tx.sink()->packet_valid(packet)) {
				warning("Invalid tx packet");
				return true;
			}

			if (link_state()) {
				Virtio_net_header hdr;
				auto const *data = _tx.sink()->packet_content(packet);
				if (!_tx_vq.write_data(hdr, data, packet.size(), false)) {
					warning("Failed to push packet into tx VirtIO queue!");
					return false;
				}
			}

			_tx.sink()->acknowledge_packet(packet);
			return true;
		}

		void _receive()
		{
			auto rcv_func = [&] (Virtio_net_header const &,
			                     char              const *data,
			                     size_t                   size) {
				if (!_rx.source()->ready_to_submit()) {
					Genode::warning("Not ready to submit!");
					return false;
				}

				try {
					auto p = _rx.source()->alloc_packet(size);
					char *dst = _rx.source()->packet_content(p);
					Genode::memcpy(dst, data, size);
					_rx.source()->submit_packet(p);
				} catch (Session::Rx::Source::Packet_alloc_failed) {
					Genode::warning("Packet alloc failed!");
					return false;
				}

				return true;
			};

			while (_rx_vq.has_used_buffers())
				_rx_vq.read_data(rcv_func);

			/**
			 * Inform the device the buffers we've just consumed are ready
			 * to used again
			 */
			_device.notify_buffers_available(RX_VQ);
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			/* Reclaim all buffers processed by the device. */
			if (_tx_vq.has_used_buffers())
				_tx_vq.ack_all_transfers();

			bool sent_packets = false;
			while (_send())
				sent_packets = true;

			if (sent_packets) {
				_device.notify_buffers_available(TX_VQ);
			}
		}

	public:

		bool link_state() override
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
				status = _device.read_config(CONFIG_STATUS, Virtio::Device::ACCESS_8BIT);
				after = _device.get_config_generation();
			} while (after != before);

			return status & STATUS_LINK_UP;
		}

		Nic::Mac_address mac_address() override { return _hw_features.mac; }

		Session_component(Genode::Env             &env,
		                  Genode::Allocator       &rx_block_md_alloc,
		                  Virtio::Device          &device,
                          Irq_session_capability   irq_cap,
		                  Genode::Xml_node  const &xml,
		                  Genode::size_t    const  tx_buf_size,
		                  Genode::size_t    const  rx_buf_size)
		try : Nic::Session_component(tx_buf_size, rx_buf_size, Genode::CACHED,
		                         rx_block_md_alloc, env),
		      _verbose(xml.attribute_value("verbose", false)),
		      _device(device),
		      _hw_features(_init_hw_features(xml)),
		      _rx_vq(env.ram(), env.rm(),
		             _vq_size(RX_VQ, xml, "rx_queue_size"),
		             _buf_size(RX_VQ, xml, "rx_buffer_size")),
		      _tx_vq(env.ram(), env.rm(),
		             _vq_size(TX_VQ, xml, "tx_queue_size"),
		             _buf_size(TX_VQ, xml, "tx_buffer_size")),
		      _irq(irq_cap),
		      _irq_handler(env.ep(), *this, &Session_component::_handle_irq),
		      _link_up(link_state())
		{
			_setup_virtio_queues();
			_irq.sigh(_irq_handler);
			_irq.ack_irq();

			_link_state_changed();

			if (_verbose)
				Genode::log("Mac address: ", mac_address());
		}
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

		~Session_component() {
			_device.set_status(Virtio::Device::Status::RESET);
		}
};


class Virtio_nic::Root : public Genode::Root_component<Session_component, Genode::Single_client>
{
	private:

		struct Device_not_found   : Genode::Exception { };

		/*
		 * Noncopyable
		 */
		Root(Root const &) = delete;
		Root &operator = (Root const &) = delete;

		Genode::Env            &_env;
		Virtio::Device         &_device;
		Irq_session_capability  _irq_cap;

		Session_component *_create_session(const char *args) override
		{
			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota) {
				error("insufficient 'ram_quota', got ", ram_quota, ", "
				      "need ", tx_buf_size + rx_buf_size);
				throw Genode::Insufficient_ram_quota();
			}

			Attached_rom_dataspace rom(_env, "config");

			try {
				return new (md_alloc()) Session_component(
					_env, *md_alloc(), _device, _irq_cap, rom.xml(),
					tx_buf_size, rx_buf_size);
			} catch (...) { throw Service_denied(); }
		}

	public:

		Root(Env                    &env,
		     Allocator              &md_alloc,
		     Virtio::Device         &device,
		     Irq_session_capability  irq_cap)
		: Root_component<Session_component, Genode::Single_client>(env.ep(), md_alloc),
		  _env(env), _device(device), _irq_cap(irq_cap)
		{ }
};
