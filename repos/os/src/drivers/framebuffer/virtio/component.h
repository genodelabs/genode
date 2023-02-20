/*
 * \brief  VirtIO MMIO Framebuffer component
 * \author Piotr Tworek
 * \date   2020-02-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/signal.h>
#include <capture_session/connection.h>
#include <irq_session/client.h>
#include <timer_session/connection.h>
#include <util/register.h>
#include <virtio/queue.h>

namespace Virtio_fb {
	using namespace Genode;
	class Driver;
}

/*
 * This driver is based on Virtual I/O Device specification, Version 1.1, chapter 5.7
 * "GPU Device". This document can be found at:
 * https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html
 */
class Virtio_fb::Driver
{
	private:

		/*
		 * Noncopyable
		 */
		Driver(Root const &) = delete;
		Driver &operator = (Driver const &) = delete;

		struct Device_init_failed    : Exception { };
		struct Unsupported_version   : Exception { };
		struct Features_init_failed  : Exception { };
		struct Queue_init_failed     : Exception { };
		struct Display_init_failed   : Exception { };
		struct Display_deinit_failed : Exception { };

		enum : uint16_t { CONTROL_VQ = 0, CURSOR_VQ = 1 };

		struct Features : Register<64>
		{
			struct VIRGL     : Bitfield<0, 1> { };
			struct EDID      : Bitfield<1, 1> { };
			struct VERSION_1 : Bitfield<32, 1> { };
		};

		struct Control_header
		{
			enum Type : uint32_t
			{
				/* 2D commands */
				CMD_GET_DISPLAY_INFO = 0x0100,
				CMD_RESOURCE_CREATE_2D,
				CMD_RESOURCE_UNREF,
				CMD_RESOURCE_SET_SCANOUT,
				CMD_RESOURCE_FLUSH,
				CMD_RESOURCE_TRANSFER_TO_HOST,
				CMD_RESOURCE_ATTACH_BACKING,
				CMD_RESOURCE_DETACH_BACKING,

				/* Success responses */
				RESP_OK_NODATA = 0x1100,
				RESP_OK_DISPLAY_INFO,
				RESP_OK_CAPSET_INFO,
				RESP_OK_CAPSET,
				RESP_OK_EDID,

				/* Error responses */
				RESP_ERROR_UNSPECIFIED = 0x1200,
				RESP_ERROR_OUT_OF_MEMORY,
				RESP_ERROR_INVALID_SCANOUT_ID,
				RESP_ERROR_INVALID_RESOURCE_ID,
				RESP_ERROR_INVALID_CONTEXT_ID,
				RESP_ERROR_INVALID_PARAMETER_ID,
			};

			Type           type;
			uint32_t       flags = 0;
			uint64_t       fence_id = 0;
			uint32_t       ctx_id = 0;
			uint32_t const padding = 0;
		};

		enum { MAX_SCANOUTS = 16 };
		enum : uint32_t { EVENT_DISPLAY = (1 << 0) };

		enum Config : uint8_t
		{
			EVENTS_READ  = 0,
			EVENTS_CLEAR = 4,
			NUM_SCANOUTS = 8,
		};

		struct Rect
		{
			uint32_t x;
			uint32_t y;
			uint32_t width;
			uint32_t height;
		};

		struct Display_info
		{
			Control_header hdr;
			struct
			{
				Rect     rect;
				uint32_t enabled;
				uint32_t flags;
			} modes[MAX_SCANOUTS];
		};

		enum class Resource_Id : uint32_t { FRAMEBUFFER = 1 };

		enum class Format : uint32_t { B8G8R8X8 = 2 };

		struct Resource_create_2d
		{
			Resource_Id const resource_id = Resource_Id::FRAMEBUFFER;
			Format      const format = Format::B8G8R8X8;
			uint32_t          width;
			uint32_t          height;
		};

		struct Resource_destroy_2d
		{
			Resource_Id const resource_id = Resource_Id::FRAMEBUFFER;
			uint32_t    const padding = 0;
		};

		struct Attach_backing
		{
			Resource_Id const resource_id = Resource_Id::FRAMEBUFFER;
			uint32_t    const nr_entries = 1;
			uint64_t          addr;
			uint32_t          length;
			uint32_t    const padding = 0;
		};

		using Detach_backing = Resource_destroy_2d;

		struct Set_scanout
		{
			Rect              rect;
			uint32_t          scanout_id;
			Resource_Id const resource_id = Resource_Id::FRAMEBUFFER;
		};

		struct Transfer_to_host_2d
		{
			Rect              rect;
			uint64_t          offset;
			Resource_Id const resource_id = Resource_Id::FRAMEBUFFER;
			uint32_t    const paddiing = 0;
		};

		struct Resource_flush
		{
			Rect              rect;
			Resource_Id const resource_id = Resource_Id::FRAMEBUFFER;
			uint32_t    const padding = 0;
		};

		struct Control_queue_traits
		{
			static const bool device_write_only = false;
			static const bool has_data_payload = true;
		};

		typedef Virtio::Queue<Control_header, Control_queue_traits> Control_queue;

		class Fb_memory_resource : public Platform::Dma_buffer
		{
			private:

				/*
				 * Noncopyable
				 */
				Fb_memory_resource(Fb_memory_resource const &) = delete;
				Fb_memory_resource &operator = (Fb_memory_resource const &) = delete;

				static size_t _fb_size(Capture::Area const &area) {
						return area.w() * area.h() * 4; }

			public:

				Fb_memory_resource(Platform::Connection       &platform,
				                   Capture::Area        const &area)
				: Platform::Dma_buffer(platform, _fb_size(area), UNCACHED) {}
		};

		Env                   &_env;
		Platform::Connection  &_platform;
		Virtio::Device        &_device;
		Control_queue          _ctrl_vq { _platform, 4, 512 };
		uint32_t const         _num_scanouts;
		Signal_handler<Driver> _irq_handler { _env.ep(), *this, &Driver::_handle_irq };

		/*
		 * Capture
		 */
		uint32_t                                   _selected_scanout_id { 0 };
		Capture::Area                              _display_area { 0, 0 };
		Capture::Connection                        _capture { _env };
		Constructible<Capture::Connection::Screen> _captured_screen { };
		Timer::Connection                          _capture_timer { _env };
		Constructible<Fb_memory_resource>          _fb_res { };

		Signal_handler<Driver> _capture_timer_handler {
			_env.ep(), *this, &Driver::_handle_capture_timer };


		static uint32_t _read_num_scanouts(Virtio::Device &device)
		{
			uint32_t num_scanouts;
			uint32_t before = 0, after = 0;
			do {
				num_scanouts = device.read_config<uint32_t>(Config::NUM_SCANOUTS);
			} while (after != before);
			return num_scanouts;
		}

		uint32_t _read_pending_events()
		{
			uint32_t events;
			uint32_t before = 0, after = 0;
			do {
				events = _device.read_config<uint32_t>(Config::EVENTS_READ);
			} while (after != before);
			do {
				_device.write_config(Config::EVENTS_CLEAR, events);
			} while (after != before);
			return events;
		}

		static uint32_t _init_device(Virtio::Device                  &device,
		                             Virtio::Queue_description const &ctrl_vq_desc)
		{
			using Status = Virtio::Device::Status;

			if (!device.set_status(Status::RESET)) {
				error("Failed to reset the device!");
				throw Device_init_failed();
			}

			if (!device.set_status(Status::ACKNOWLEDGE)) {
				error("Failed to acknowledge the device!");
				throw Device_init_failed();
			}

			if (!device.set_status(Status::DRIVER)) {
				device.set_status(Status::FAILED);
				error("Device initialization failed!");
				throw Device_init_failed();
			}

			const uint32_t low = device.get_features(0);
			const uint32_t high = device.get_features(1);
			const Features::access_t device_features = ((uint64_t)high << 32) | low;
			Features::access_t driver_features = 0;

			/* This driver does not support legacy VirtIO versions. */
			if (!Features::VERSION_1::get(device_features)) {
				error("Unsupprted VirtIO device version!");
				throw Unsupported_version();
			}
			Features::VERSION_1::set(driver_features);

			device.set_features(0, (uint32_t)driver_features);
			device.set_features(1, (uint32_t)(driver_features >> 32));

			if (!device.set_status(Status::FEATURES_OK)) {
				device.set_status(Status::FAILED);
				error("Device feature negotiation failed!");
				throw Features_init_failed();
			}

			if (!device.configure_queue(CONTROL_VQ, ctrl_vq_desc)) {
				error("Failed to initialize \"control\" VirtIO queue!");
				throw Queue_init_failed();
			}

			using Status = Virtio::Device::Status;
			if (!device.set_status(Status::DRIVER_OK)) {
				device.set_status(Status::FAILED);
				error("Failed to initialize VirtIO queues!");
				throw Queue_init_failed();
			}

			auto num_scanouts = _read_num_scanouts(device);
			if (num_scanouts > MAX_SCANOUTS) {
				error("Invalid scanout number!");
				throw Device_init_failed();
			}

			return num_scanouts;
		}

		void _configure_display() {
			Control_header res2d_cmd { Control_header::CMD_RESOURCE_CREATE_2D };
			Resource_create_2d res2d_data {
				.width  = _display_area.w(),
				.height = _display_area.h(),
			};

			if (!_exec_cmd(res2d_cmd, res2d_data)) {
				error("Failed to create 2D resource!");
				throw Display_init_failed();
			}

			try {
				_fb_res.construct(_platform, _display_area);
			} catch (...) {
				error("Failed to allocate framebuffer!");
				throw;
			}

			{
				Control_header attach_cmd { Control_header::CMD_RESOURCE_ATTACH_BACKING };
				Attach_backing attach_data {
					.addr   = _fb_res->dma_addr(),
					.length = static_cast<uint32_t>(_fb_res->size())
				};

				if (!_exec_cmd(attach_cmd, attach_data)) {
					error("Failed to attach framebuffer backing!");
					throw Display_init_failed();
				}
			}

			Control_header scanout_cmd { Control_header::CMD_RESOURCE_SET_SCANOUT };
			Set_scanout scanout_data {
				.rect = {
					.x = 0,
					.y = 0,
					.width  = _display_area.w(),
					.height = _display_area.h(),
				},
				.scanout_id = _selected_scanout_id,
			};

			if (!_exec_cmd(scanout_cmd, scanout_data)) {
				error("Failed to assign framebuffer to a scanout!");
				throw Display_init_failed();
			}

			_captured_screen.construct(_capture, _env.rm(), _display_area);
		}

		void _shutdown_display()
		{
			Control_header detach_cmd { Control_header::CMD_RESOURCE_DETACH_BACKING };
			Detach_backing detach_data;

			if (!_exec_cmd(detach_cmd, detach_data)) {
				error("Failed to detatch framebuffer backing!");
				throw Display_deinit_failed();
			}

			Control_header unref_cmd { Control_header::CMD_RESOURCE_UNREF };
			Resource_destroy_2d unref_data;

			if (!_exec_cmd(unref_cmd, unref_data)) {
				error("Failed to unref framebuffer resource!");
				throw Display_deinit_failed();
			}

			_captured_screen.destruct();
			_fb_res.destruct();
		}

		void _reconfigure_display()
		{
			if (!_update_display_info(true)) {
				error("Failed to update display info!");
				throw Display_deinit_failed();
			}

			_shutdown_display();
			_configure_display();
		}

		void _handle_irq()
		{
			const uint32_t reasons = _device.read_isr();

			enum { IRQ_USED_RING_UPDATE = 1, IRQ_CONFIG_CHANGE = 2 };

			/*
			 * This driver does not request interrupts when dealing with control
			 * queue. Some older pre 6.x Qemu versions do signal ctrl ring update
			 * when display size is changed. Just ACK and otherwise ignore such
			 * bogus updates.
			 */
			if ((reasons & IRQ_USED_RING_UPDATE) && _ctrl_vq.has_used_buffers())
				_ctrl_vq.ack_all_transfers();

			if (reasons & IRQ_CONFIG_CHANGE) {
				auto const events = _read_pending_events();
				if (events & EVENT_DISPLAY)
					_reconfigure_display();
			}

			_device.irq_ack();
		}

		void _handle_capture_timer()
		{
			using Pixel = Capture::Pixel;

			if (!_captured_screen.constructed())
				return;

			Capture::Surface<Pixel> surface(_fb_res->local_addr<Pixel>(),
			                                _display_area);
			_captured_screen->apply_to_surface(surface);

			_update_fb();
		}

		void _flush_ctrl_vq()
		{
			_device.notify_buffers_available(CONTROL_VQ);
			while (!_ctrl_vq.has_used_buffers());
		}

		template <typename CMD_DATA_TYPE>
		bool _exec_cmd(Control_header const &cmd,
		               CMD_DATA_TYPE  const &cmd_data)
		{
			return _ctrl_vq.write_data_read_reply<Control_header>(
				cmd, (char const *)&cmd_data, sizeof(CMD_DATA_TYPE),
				[this]() { _flush_ctrl_vq(); },
				[](Control_header const &response) {
					return response.type == Control_header::RESP_OK_NODATA; });
		}

		bool _update_display_info(bool use_current_scanout)
		{
			Control_header cmd { Control_header::CMD_GET_DISPLAY_INFO };

			auto display_info_cb = [&](Display_info const &info) {
				for (uint32_t i = 0; i < _num_scanouts; ++i) {
					if (info.modes[i].enabled) {
						if (use_current_scanout && (_selected_scanout_id != i))
							continue;
						auto const &r = info.modes[i].rect;
						_display_area = Capture::Area{ r.width, r.height };
						_selected_scanout_id = i;
						return true;
					}
				}
				return false;
			};

			if (!_ctrl_vq.write_data_read_reply<Display_info>(
					cmd, [this] { _flush_ctrl_vq(); }, display_info_cb)) {
				error("Failed to request display info!");
				return false;
			}

			return true;
		}

		void _init_display() {
			if (!_update_display_info(false))
				throw Display_init_failed();
			_configure_display();
		}

		void _update_fb()
		{
			Control_header transfer_cmd { Control_header::CMD_RESOURCE_TRANSFER_TO_HOST };
			Transfer_to_host_2d transfer_data {
				.rect = {
					.x = 0,
					.y = 0,
					.width  = _display_area.w(),
					.height = _display_area.h(),
				},
				.offset = 0,
			};
			if (!_exec_cmd(transfer_cmd, transfer_data)) {
				error("Failed to send transfer 2D resource to host command!");
				return;
			}

			Control_header flush_cmd { Control_header::CMD_RESOURCE_FLUSH };
			Resource_flush flush_data {
				.rect = {
					.x = 0,
					.y = 0,
					.width  = _display_area.w(),
					.height = _display_area.h(),
				},
			};
			if (!_exec_cmd(flush_cmd, flush_data)) {
				error("Failed to send flush resource command!");
				return;
			}
		}

	public:

		Driver(Env                    &env,
		       Platform::Connection   &platform,
		       Virtio::Device         &device)
		: _env(env),
		  _platform(platform),
		  _device(device),
		  _num_scanouts(_init_device(_device, _ctrl_vq.description()))
		{
			try {
				_init_display();
			} catch (...) {
				device.set_status(Virtio::Device::Status::RESET);
				throw;
			}

			_device.irq_sigh(_irq_handler);
			_device.irq_ack();
			_capture_timer.sigh(_capture_timer_handler);
			_capture_timer.trigger_periodic(10*1000);
		}

		~Driver()
		{
			_device.set_status(Virtio::Device::Status::RESET);
		}
};
