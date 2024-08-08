/*
 * \brief  Virtio GPU implementation
 * \author Stefan Kalkowski
 * \date   2021-02-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIRTIO_GPU_H_
#define _VIRTIO_GPU_H_

#include <base/attached_ram_dataspace.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <base/registry.h>

#include <gui_session/connection.h>
#include <virtio_device.h>

namespace Vmm {
	class Virtio_gpu_queue;
	class Virtio_gpu_control_request;
	class Virtio_gpu_device;
	using namespace Genode;
}


class Vmm::Virtio_gpu_queue : public Virtio_split_queue
{
	private:

		Ring_index _used_idx {};

		friend class Virtio_gpu_control_request;

	public:

		enum { CONTROL, CURSOR, QUEUE_COUNT };

		using Virtio_split_queue::Virtio_split_queue;

		void notify(Virtio_gpu_device &);
};


class Vmm::Virtio_gpu_control_request
{
	private:

		using Index            = Virtio_gpu_queue::Descriptor_index;
		using Descriptor       = Virtio_gpu_queue::Descriptor;
		using Descriptor_array = Virtio_gpu_queue::Descriptor_array;

		template <size_t SIZE>
		struct Control_header_tpl : Mmio<SIZE>
		{
			using Base = Mmio<SIZE>;

			struct Type : Base::template Register<0,  32>
			{
				enum Commands {
					/* 2D commands */
					GET_DISPLAY_INFO = 0x0100,
					RESOURCE_CREATE_2D,
					RESOURCE_UNREF,
					SET_SCANOUT,
					RESOURCE_FLUSH,
					TRANSFER_TO_HOST_2D,
					RESOURCE_ATTACH_BACKING,
					RESOURCE_DETACH_BACKING,
					GET_CAPSET_INFO,
					GET_CAPSET,
					GET_EDID,

					/* cursor commands */
					UPDATE_CURSOR = 0x0300,
					MOVE_CURSOR,
				};

				enum Responses {
					OK_NO_DATA = 0x1100,
					OK_DISPLAY_INFO,
					OK_CAPSET_INFO,
					OK_CAPSET,
					OK_EDID,
					ERR_UNSPEC = 0x1200,
					ERR_OUT_OF_MEMORY,
					ERR_INVALID_SCANOUT_ID,
					ERR_INVALID_RESOURCE_ID,
					ERR_INVALID_CONTEXT_ID,
					ERR_INVALID_PARAMETER,
				};
			};
			struct Flags    : Base::template Register<0x4,  32> {};
			struct Fence_id : Base::template Register<0x8,  64> {};
			struct Ctx_id   : Base::template Register<0x10, 32> {};

			using Base::Mmio;
		};

		using Control_header = Control_header_tpl<24>;

		struct Display_info_response : Control_header_tpl<Control_header::SIZE + 24*16>
		{
			struct X       : Register<0x18, 32> {};
			struct Y       : Register<0x1c, 32> {};
			struct Width   : Register<0x20, 32> {};
			struct Height  : Register<0x24, 32> {};
			struct Enabled : Register<0x28, 32> {};
			struct Flags   : Register<0x2c, 32> {};

			using Control_header_tpl::Control_header_tpl;
		};

		struct Resource_create_2d : Control_header_tpl<Control_header::SIZE + 16>
		{
			struct Resource_id : Register<0x18, 32> {};

			struct Format : Register<0x1c, 32>
			{
				enum {
					B8G8R8A8 = 1,
					B8G8R8X8 = 2,
					A8R8G8B8 = 3,
					X8R8G8B8 = 4,
					R8G8B8A8 = 67,
					X8B8G8R8 = 68,
					A8B8G8R8 = 121,
					R8G8B8X8 = 134,
				};
			};

			struct Width       : Register<0x20, 32> {};
			struct Height      : Register<0x24, 32> {};

			using Control_header_tpl::Control_header_tpl;
		};

		struct Resource_unref : Control_header_tpl<Control_header::SIZE + 8>
		{
			struct Resource_id : Register<0x18, 32> {};

			using Control_header_tpl::Control_header_tpl;
		};

		struct Resource_attach_backing : Control_header_tpl<Control_header::SIZE + 8>
		{
			struct Resource_id : Register<0x18, 32> {};
			struct Nr_entries  : Register<0x1c, 32> {};

			struct Memory_entry : Mmio<16>
			{
				struct Address : Register<0x0, 64> {};
				struct Length  : Register<0x8, 32> {};

				using Mmio::Mmio;
			};

			using Control_header_tpl::Control_header_tpl;
		};

		struct Set_scanout : Control_header_tpl<Control_header::SIZE + 24>
		{
			struct X           : Register<0x18, 32> {};
			struct Y           : Register<0x1c, 32> {};
			struct Width       : Register<0x20, 32> {};
			struct Height      : Register<0x24, 32> {};
			struct Scanout_id  : Register<0x28, 32> {};
			struct Resource_id : Register<0x2c, 32> {};

			using Control_header_tpl::Control_header_tpl;
		};

		struct Resource_flush : Control_header_tpl<Control_header::SIZE + 24>
		{
			struct X           : Register<0x18, 32> {};
			struct Y           : Register<0x1c, 32> {};
			struct Width       : Register<0x20, 32> {};
			struct Height      : Register<0x24, 32> {};
			struct Resource_id : Register<0x28, 32> {};

			using Control_header_tpl::Control_header_tpl;
		};

		struct Transfer_to_host_2d :Control_header_tpl<Control_header::SIZE + 32>
		{
			struct X           : Register<0x18, 32> {};
			struct Y           : Register<0x1c, 32> {};
			struct Width       : Register<0x20, 32> {};
			struct Height      : Register<0x24, 32> {};
			struct Offset      : Register<0x28, 64> {};
			struct Resource_id : Register<0x30, 32> {};

			using Control_header_tpl::Control_header_tpl;
		};

		Descriptor_array  & _array;
		Ram               & _ram;
		Virtio_gpu_device & _device;
		Index               _idx;

		Index _next(Descriptor desc)
		{
			if (!Descriptor::Flags::Next::get(desc.flags()))
				throw Exception("Invalid request, no next descriptor");
			return desc.next();
		}

		Descriptor _desc(unsigned i)
		{
			Index idx = _idx;
			for (; i; i--)
				idx = _next(_array.get(idx));
			return _array.get(idx);
		}

		Byte_range_ptr _desc_range(unsigned i)
		{
			Descriptor d = _desc(i);
			/* we only support 32-bit ram addresses by now */
			return _ram.to_local_range({(char *)d.address(), d.length()});
		}

		Control_header _ctrl_hdr { _desc_range(0) };

		void _get_display_info();
		void _resource_create_2d();
		void _resource_delete();
		void _resource_attach_backing();
		void _set_scanout();
		void _resource_flush();
		void _transfer_to_host_2d();
		void _update_cursor();
		void _move_cursor();

	public:

		Virtio_gpu_control_request(Index               id,
		                           Descriptor_array  & array,
		                           Ram               & ram,
		                           Virtio_gpu_device & device)
		: _array(array), _ram(ram), _device(device), _idx(id)
		{
			switch (_ctrl_hdr.read<Control_header::Type>()) {
			case Control_header::Type::GET_DISPLAY_INFO:
				_get_display_info();
				break;
			case Control_header::Type::RESOURCE_CREATE_2D:
				_resource_create_2d();
				break;
			case Control_header::Type::RESOURCE_UNREF:
				_resource_delete();
				break;
			case Control_header::Type::RESOURCE_ATTACH_BACKING:
				_resource_attach_backing();
				break;
			case Control_header::Type::SET_SCANOUT:
				_set_scanout();
				break;
			case Control_header::Type::RESOURCE_FLUSH:
				_resource_flush();
				break;
			case Control_header::Type::TRANSFER_TO_HOST_2D:
				_transfer_to_host_2d();
				break;
			case Control_header::Type::UPDATE_CURSOR:
				_update_cursor();
				break;
			case Control_header::Type::MOVE_CURSOR:
				_move_cursor();
				break;
			default:
				error("Unknown control request ",
				      _ctrl_hdr.read<Control_header::Type>());
			};
		}

		size_t size() { return Control_header::SIZE; }
};


class Vmm::Virtio_gpu_device : public Virtio_device<Virtio_gpu_queue, 2>
{
	private:

		friend class Virtio_gpu_control_request;

		Env                                  & _env;
		Heap                                 & _heap;
		Attached_ram_dataspace               & _ram_ds;
		Gui::Connection                      & _gui;
		Cpu::Signal_handler<Virtio_gpu_device> _handler;
		Constructible<Attached_dataspace>      _fb_ds { };
		Framebuffer::Mode                      _fb_mode { _gui.mode() };
		Gui::View_id                           _view = _gui.create_view();

		using Area = Genode::Area<>;
		using Rect = Genode::Rect<>;

		enum { BYTES_PER_PIXEL = 4 };

		struct Resource : Registry<Resource>::Element
		{
			struct Scanout : Registry<Scanout>::Element, Rect
			{
				uint32_t id;

				Scanout(Registry<Scanout> & registry,
				        uint32_t id,
				        uint32_t x, uint32_t y,
				        uint32_t w, uint32_t h)
				:
					Registry<Scanout>::Element(registry, *this),
					Rect(Point((int)x,(int)y), Area((int)w,(int)h)),
					id(id) { }

				using Rect::Rect;
			};

			Virtio_gpu_device & device;
			uint32_t            id;
			Area                area;

			size_t _size() const {
				return align_addr(area.w * area.h * BYTES_PER_PIXEL, 12); }

			addr_t                 attach_off { 0UL };
			Rm_connection          rm         { device._env };
			Region_map_client      region_map { rm.create(_size()) };
			Attached_dataspace     src_ds     { device._env.rm(),
			                                    region_map.dataspace() };
			Attached_ram_dataspace dst_ds     { device._env.ram(),
			                                    device._env.rm(), _size() };
			Registry<Scanout>      scanouts {};

			Resource(Virtio_gpu_device & dev,
			         uint32_t            id,
			         uint32_t            w,
			         uint32_t            h)
			:
				Registry<Resource>::Element(dev._resources, *this),
				device(dev),
				id(id),
				area((int)w, (int)h) {}

			void attach(addr_t off, size_t sz)
			{
				if (attach_off + sz > _size())
					return;

				for (;;) {
					Region_map::Attach_result const result =
						region_map.attach(device._ram_ds.cap(), {
							.size       = sz,
							.offset     = off,
							.use_at     = true,
							.at         = attach_off,
							.executable = false,
							.writeable  = true
						});
					if (result.ok())
						break;
					using Error = Region_map::Attach_error;
					if      (result == Error::OUT_OF_RAM)  rm.upgrade_ram(8*1024);
					else if (result == Error::OUT_OF_CAPS) rm.upgrade_caps(2);
					else {
						error("failed to locally attach Virtio_gpu_device resource");
						break;
					}
				}
			}
		};

		Registry<Resource> _resources {};

		struct Configuration_area : Mmio_register
		{
			Virtio_gpu_device & dev;

			enum {
				EVENTS_READ  = 0,
				EVENTS_CLEAR = 4,
				SCANOUTS     = 8,
				NUM_CAPSETS  = 12
			};

			enum Events { NONE = 0, DISPLAY = 1 };

			Register read(Address_range & range,  Cpu&) override
			{
				if (range.start() == EVENTS_READ)
					return DISPLAY;

				/* we support no multi-head, just return 1 */
				if (range.start() == SCANOUTS)
					return 1;

				return 0;
			}

			void write(Address_range &, Cpu&, Register) override {}

			Configuration_area(Virtio_gpu_device & device)
			:
				Mmio_register("GPU config area", Mmio_register::RO,
				              0x100, 16, device.registers()),
				dev(device) { }
		} _config_area{ *this };

		void _mode_change()
		{
			Genode::Mutex::Guard guard(_mutex);
			_config_notification();
		}

		void _notify(unsigned idx) override
		{
			if (idx < Virtio_gpu_queue::QUEUE_COUNT)
				_queue[idx]->notify(*this);
		}

		enum Device_id { GPU = 16 };

	public:

		Virtio_gpu_device(const char * const       name,
		                  const uint64_t           addr,
		                  const uint64_t           size,
		                  unsigned                 irq,
		                  Cpu                    & cpu,
		                  Space                  & bus,
		                  Ram                    & ram,
		                  Virtio_device_list     & list,
		                  Env                    & env,
		                  Heap                   & heap,
		                  Attached_ram_dataspace & ram_ds,
		                  Gui::Connection        & gui)
		:
			Virtio_device<Virtio_gpu_queue, 2>(name, addr, size,
			                                   irq, cpu, bus, ram, list, GPU),
			_env(env), _heap(heap), _ram_ds(ram_ds), _gui(gui),
			_handler(cpu, env.ep(), *this, &Virtio_gpu_device::_mode_change)
		{
			_gui.mode_sigh(_handler);
		}

		void buffer_notification()
		{
			_buffer_notification();
		}

		Framebuffer::Mode resize()
		{
			_fb_ds.destruct();

			_fb_mode = _gui.mode();
			_gui.buffer(_fb_mode, false);

			if (_fb_mode.area.count() > 0)
				_fb_ds.construct(_env.rm(), _gui.framebuffer.dataspace());

			using Command = Gui::Session::Command;
			_gui.enqueue<Command::Geometry>(_view, Rect(Point(0, 0), _fb_mode.area));
			_gui.enqueue<Command::Front>(_view);
			_gui.execute();
			return _gui.mode();
		}
};

#endif /* _VIRTIO_GPU_H_ */
