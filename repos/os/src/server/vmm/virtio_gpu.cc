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

#include <blit/blit.h>
#include <virtio_gpu.h>


void Vmm::Virtio_gpu_queue::notify(Virtio_gpu_device & dev)
{
	memory_barrier();
	bool inform = false;
	for (Ring_index avail_idx = _avail.current();
	     _cur_idx != avail_idx; _cur_idx.inc()) {
		try {
			Index idx = _avail.get(_cur_idx);
			Virtio_gpu_control_request
				request(idx, _descriptors, _ram, dev);
			_used.add(_cur_idx.idx(), idx, request.size());
		} catch (Exception & e) {
			error(e);
		}
		inform = true;
	}

	if (!inform)
		return;

	_used.write<Used_queue::Idx>((uint16_t)_cur_idx.idx());
	memory_barrier();
	if (_avail.inject_irq()) dev.buffer_notification();
}


void Vmm::Virtio_gpu_control_request::_get_display_info()
{
	Framebuffer::Mode mode = _device.resize();
	Display_info_response dir { _desc_range(1) };
	memset((void*)dir.base(), 0, Display_info_response::SIZE);
	dir.write<Control_header::Type>(Control_header::Type::OK_DISPLAY_INFO);

	dir.write<Display_info_response::X>(0);
	dir.write<Display_info_response::Y>(0);
	dir.write<Display_info_response::Width>(mode.area.w());
	dir.write<Display_info_response::Height>(mode.area.h());
	dir.write<Display_info_response::Enabled>(1);
	dir.write<Display_info_response::Flags>(0);
}


void Vmm::Virtio_gpu_control_request::_resource_create_2d()
{
	Resource_create_2d c2d      { _desc_range(0) };
	Control_header     response { _desc_range(1) };

	if (c2d.read<Resource_create_2d::Format>() !=
	    Resource_create_2d::Format::B8G8R8X8) {
		warning("Unsupported pixel fomat (id=",
		        c2d.read<Resource_create_2d::Format>(), ")!");
		response.write<Control_header::Type>(Control_header::Type::ERR_INVALID_PARAMETER);
		return;
	}

	using Resource = Virtio_gpu_device::Resource;

	try {
		new (_device._heap)
			Resource(_device,
			         c2d.read<Resource_create_2d::Resource_id>(),
			         c2d.read<Resource_create_2d::Width>(),
			         c2d.read<Resource_create_2d::Height>());
		response.write<Control_header::Type>(Control_header::Type::OK_NO_DATA);
	} catch(...) {
		response.write<Control_header::Type>(Control_header::Type::ERR_OUT_OF_MEMORY);
	}
}


void Vmm::Virtio_gpu_control_request::_resource_delete()
{
	using Resource = Virtio_gpu_device::Resource;
	using Scanout  = Resource::Scanout;

	Resource_unref rur      { _desc_range(0) };
	Control_header response { _desc_range(1)  };

	response.write<Control_header::Type>(Control_header::Type::ERR_INVALID_RESOURCE_ID);
	uint32_t id = rur.read<Resource_unref::Resource_id>();

	_device._resources.for_each([&] (Resource & res) {
		if (res.id != id)
			return;

		res.scanouts.for_each([&] (Scanout & sc) {
			destroy(_device._heap, &sc); });
		destroy(_device._heap, &res);
		response.write<Control_header::Type>(Control_header::Type::OK_NO_DATA);
	});
}


void Vmm::Virtio_gpu_control_request::_resource_attach_backing()
{
	using Resource = Virtio_gpu_device::Resource;
	using Entry    = Resource_attach_backing::Memory_entry;

	Resource_attach_backing rab        { _desc_range(0) };
	Byte_range_ptr entry_range         { _desc_range(1) };
	Control_header response            { _desc_range(2) };

	response.write<Control_header::Type>(Control_header::Type::ERR_INVALID_RESOURCE_ID);
	uint32_t id = rab.read<Resource_attach_backing::Resource_id>();
	unsigned nr = rab.read<Resource_attach_backing::Nr_entries>();

	_device._resources.for_each([&] (Resource & res) {
		if (res.id != id)
			return;

		try {
			for (unsigned i = 0; i < nr; i++) {
				off_t offset = i*Entry::SIZE;
				Entry entry({entry_range.start + offset, entry_range.num_bytes - offset});
				size_t sz  = entry.read<Entry::Length>();
				addr_t off = (addr_t)_device._ram.to_local_range({(char *)entry.read<Entry::Address>(), sz}).start
				             - _device._ram.local_base();
				res.attach(off, sz);
			}
			response.write<Control_header::Type>(Control_header::Type::OK_NO_DATA);
		} catch (Exception &) {
			response.write<Control_header::Type>(Control_header::Type::ERR_INVALID_PARAMETER);
		}
	});
}


void Vmm::Virtio_gpu_control_request::_set_scanout()
{
	Set_scanout    scr      { _desc_range(0) };
	Control_header response { _desc_range(1) };

	uint32_t id  = scr.read<Set_scanout::Resource_id>();
	uint32_t sid = scr.read<Set_scanout::Scanout_id>();
	response.write<Control_header::Type>(id ? Control_header::Type::ERR_INVALID_RESOURCE_ID
	                                        : Control_header::Type::OK_NO_DATA);

	using Resource = Virtio_gpu_device::Resource;
	using Scanout  = Resource::Scanout;
	_device._resources.for_each([&] (Resource & res) {
		if (!id || id == res.id)
			res.scanouts.for_each([&] (Scanout & sc) {
				if (sc.id == sid) destroy(_device._heap, &sc); });

		if (res.id != id)
			return;

		try {
			new (_device._heap) Scanout(res.scanouts, sid,
			                            scr.read<Set_scanout::X>(),
			                            scr.read<Set_scanout::Y>(),
			                            scr.read<Set_scanout::Width>(),
			                            scr.read<Set_scanout::Height>());
			response.write<Control_header::Type>(Control_header::Type::OK_NO_DATA);
		} catch(...) {
			response.write<Control_header::Type>(Control_header::Type::ERR_OUT_OF_MEMORY);
		}
	});
}


void Vmm::Virtio_gpu_control_request::_resource_flush()
{
	Resource_flush rf       { _desc_range(0) };
	Control_header response { _desc_range(1) };

	uint32_t id  = rf.read<Resource_flush::Resource_id>();
	response.write<Control_header::Type>(Control_header::Type::ERR_INVALID_RESOURCE_ID);

	if (!_device._fb_ds.constructed())
		return;

	using Resource = Virtio_gpu_device::Resource;
	_device._resources.for_each([&] (Resource & res) {
		if (res.id != id)
			return;

		uint32_t x = rf.read<Resource_flush::X>();
		uint32_t y = rf.read<Resource_flush::Y>();
		uint32_t w = min(rf.read<Resource_flush::Width>(),
		                 _device._fb_mode.area.w() - x);
		uint32_t h = min(rf.read<Resource_flush::Height>(),
		                 _device._fb_mode.area.h() - y);

		enum { BYTES_PER_PIXEL = Virtio_gpu_device::BYTES_PER_PIXEL };

		if (x     > res.area.w()              ||
		    y     > res.area.h()              ||
		    w     > res.area.w()              ||
		    h     > res.area.h()              ||
		    x + w > res.area.w()              ||
		    y + h > res.area.h()) {
			response.write<Control_header::Type>(Control_header::Type::ERR_INVALID_PARAMETER);
			return;
		}

		response.write<Control_header::Type>(Control_header::Type::OK_NO_DATA);

		void * src =
			(void*)((addr_t)res.dst_ds.local_addr<void>() +
			        (res.area.w() * y + x) * BYTES_PER_PIXEL);
		void * dst =
			(void*)((addr_t)_device._fb_ds->local_addr<void>() +
			        (_device._fb_mode.area.w() * y + x) * BYTES_PER_PIXEL);
		uint32_t line_src = res.area.w() * BYTES_PER_PIXEL;
		uint32_t line_dst = _device._fb_mode.area.w() * BYTES_PER_PIXEL;

		blit(src, line_src, dst, line_dst, w*BYTES_PER_PIXEL, h);
		_device._gui.framebuffer()->refresh(x, y, w, h);
	});
}


void Vmm::Virtio_gpu_control_request::_transfer_to_host_2d()
{
	Transfer_to_host_2d tth      { _desc_range(0) };
	Control_header      response { _desc_range(1) };

	uint32_t id = tth.read<Transfer_to_host_2d::Resource_id>();
	response.write<Control_header::Type>(Control_header::Type::ERR_INVALID_RESOURCE_ID);

	enum { BYTES_PER_PIXEL = Virtio_gpu_device::BYTES_PER_PIXEL };

	using Resource = Virtio_gpu_device::Resource;
	_device._resources.for_each([&] (Resource & res)
	{
		if (res.id != id)
			return;

		uint32_t x = tth.read<Transfer_to_host_2d::X>();
		uint32_t y = tth.read<Transfer_to_host_2d::Y>();
		uint32_t w = tth.read<Transfer_to_host_2d::Width>();
		uint32_t h = tth.read<Transfer_to_host_2d::Height>();
		addr_t off = (addr_t)tth.read<Transfer_to_host_2d::Offset>();

		if (x + w > res.area.w() || y + h > res.area.h()) {
			response.write<Control_header::Type>(Control_header::Type::ERR_INVALID_PARAMETER);
			return;
		}

		void * src  = (void*)((addr_t)res.src_ds.local_addr<void>() + off);
		void * dst  = (void*)((addr_t)res.dst_ds.local_addr<void>() +
		              (y * res.area.w() + x) * BYTES_PER_PIXEL);
		uint32_t line = res.area.w() * BYTES_PER_PIXEL;

		blit(src, line, dst, line, w*BYTES_PER_PIXEL, h);

		response.write<Control_header::Type>(Control_header::Type::OK_NO_DATA);
	});
}


void Vmm::Virtio_gpu_control_request::_update_cursor() { }


void Vmm::Virtio_gpu_control_request::_move_cursor() { }
