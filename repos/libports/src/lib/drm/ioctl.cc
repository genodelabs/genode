/*
 * \brief  DRM ioctl backend
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/registry.h>
// #include <drm/serialize.h>
#include <gpu_session/connection.h>
#include <os/backtrace.h>
#include <util/retry.h>

extern "C" {
#include <drm.h>
#include <i915_drm.h>

#define DRM_NUMBER(req) ((req) & 0xff)
}


enum { verbose_ioctl = false };

namespace Utils
{
	uint64_t canonical_addr(uint64_t addr) {
		return (Genode::int64_t)(addr << 16) >> 16; }

	uint64_t noncanonical_addr(uint64_t addr) {
		return addr & 0xffffffffffffu; }
}


/**
 * Get DRM command number
 */
static long command_number(long request)
{
	return request & 0xff;
}


/**
 * Get device specific command number
 */
static long device_number(long request)
{
	return command_number(request) - DRM_COMMAND_BASE;
}


/**
 * Check if request is device command
 */
static bool device_ioctl(long request)
{
	long const cmd = command_number(request);
	return cmd >= DRM_COMMAND_BASE && cmd < DRM_COMMAND_END;
}


static const char *command_name(long request)
{
	if (IOCGROUP(request) != DRM_IOCTL_BASE)
		return "<non-DRM>";


	if (!device_ioctl(request)) {
		long const cmd = command_number(request);
		switch (cmd) {
		case DRM_NUMBER(DRM_IOCTL_GEM_CLOSE): return "DRM_IOCTL_GEM_CLOSE";
		case DRM_NUMBER(DRM_IOCTL_GEM_FLINK): return "DRM_IOCTL_GEM_FLINK";
		default:                  return "<unknown command>";
		}
	}

	switch (device_number(request)) {
	case DRM_I915_INIT:                  return "DRM_I915_INIT";
	case DRM_I915_FLUSH:                 return "DRM_I915_FLUSH";
	case DRM_I915_FLIP:                  return "DRM_I915_FLIP";
	case DRM_I915_BATCHBUFFER:           return "DRM_I915_BATCHBUFFER";
	case DRM_I915_IRQ_EMIT:              return "DRM_I915_IRQ_EMIT";
	case DRM_I915_IRQ_WAIT:              return "DRM_I915_IRQ_WAIT";
	case DRM_I915_GETPARAM:              return "DRM_I915_GETPARAM";
	case DRM_I915_SETPARAM:              return "DRM_I915_SETPARAM";
	case DRM_I915_ALLOC:                 return "DRM_I915_ALLOC";
	case DRM_I915_FREE:                  return "DRM_I915_FREE";
	case DRM_I915_INIT_HEAP:             return "DRM_I915_INIT_HEAP";
	case DRM_I915_CMDBUFFER:             return "DRM_I915_CMDBUFFER";
	case DRM_I915_DESTROY_HEAP:          return "DRM_I915_DESTROY_HEAP";
	case DRM_I915_SET_VBLANK_PIPE:       return "DRM_I915_SET_VBLANK_PIPE";
	case DRM_I915_GET_VBLANK_PIPE:       return "DRM_I915_GET_VBLANK_PIPE";
	case DRM_I915_VBLANK_SWAP:           return "DRM_I915_VBLANK_SWAP";
	case DRM_I915_HWS_ADDR:              return "DRM_I915_HWS_ADDR";
	case DRM_I915_GEM_INIT:              return "DRM_I915_GEM_INIT";
	case DRM_I915_GEM_EXECBUFFER:        return "DRM_I915_GEM_EXECBUFFER";
	case DRM_I915_GEM_PIN:               return "DRM_I915_GEM_PIN";
	case DRM_I915_GEM_UNPIN:             return "DRM_I915_GEM_UNPIN";
	case DRM_I915_GEM_BUSY:              return "DRM_I915_GEM_BUSY";
	case DRM_I915_GEM_THROTTLE:          return "DRM_I915_GEM_THROTTLE";
	case DRM_I915_GEM_ENTERVT:           return "DRM_I915_GEM_ENTERVT";
	case DRM_I915_GEM_LEAVEVT:           return "DRM_I915_GEM_LEAVEVT";
	case DRM_I915_GEM_CREATE:            return "DRM_I915_GEM_CREATE";
	case DRM_I915_GEM_PREAD:             return "DRM_I915_GEM_PREAD";
	case DRM_I915_GEM_PWRITE:            return "DRM_I915_GEM_PWRITE";
	case DRM_I915_GEM_MMAP:              return "DRM_I915_GEM_MMAP";
	case DRM_I915_GEM_SET_DOMAIN:        return "DRM_I915_GEM_SET_DOMAIN";
	case DRM_I915_GEM_SW_FINISH:         return "DRM_I915_GEM_SW_FINISH";
	case DRM_I915_GEM_SET_TILING:        return "DRM_I915_GEM_SET_TILING";
	case DRM_I915_GEM_GET_TILING:        return "DRM_I915_GEM_GET_TILING";
	case DRM_I915_GEM_GET_APERTURE:      return "DRM_I915_GEM_GET_APERTURE";
	case DRM_I915_GEM_MMAP_GTT:          return "DRM_I915_GEM_MMAP_GTT";
	case DRM_I915_GET_PIPE_FROM_CRTC_ID: return "DRM_I915_GET_PIPE_FROM_CRTC_ID";
	case DRM_I915_GEM_MADVISE:           return "DRM_I915_GEM_MADVISE";
	case DRM_I915_OVERLAY_PUT_IMAGE:     return "DRM_I915_OVERLAY_PUT_IMAGE";
	case DRM_I915_OVERLAY_ATTRS:         return "DRM_I915_OVERLAY_ATTRS";
	case DRM_I915_GEM_EXECBUFFER2:       return "DRM_I915_GEM_EXECBUFFER2";
	case DRM_I915_REG_READ:              return "DRM_I915_REG_READ";
	case DRM_I915_GET_RESET_STATS:       return "DRM_I915_GET_RESET_STATS";
	case DRM_I915_GEM_CONTEXT_CREATE:    return "DRM_I915_GEM_CONTEXT_CREATE";
	default:
		Genode::backtrace();
		return "<unknown driver>";
	}
}

static void dump_ioctl(long request)
{
	using namespace Genode;

	log("ioctl(request=", Hex(request),
	    (request & 0xe0000000) == IOC_OUT   ? " out"   :
	    (request & 0xe0000000) == IOC_IN    ? " in"    :
	    (request & 0xe0000000) == IOC_INOUT ? " inout" : " void",
	    " len=", IOCPARM_LEN(request),
	    " cmd=",command_name(request), " (", Hex(command_number(request)), ")");
}


class Drm_call
{
	private:

		Genode::Env  &_env;
		Genode::Heap  _heap { _env.ram(), _env.rm() };

		Gpu::Connection _gpu_session { _env };
		Gpu::Info       _gpu_info { _gpu_session.info() };

		Genode::Lock _completion_lock { Genode::Lock::LOCKED };

		size_t available_gtt_size { _gpu_info.aperture_size };

		using Handle = uint32_t;
		using Offset = unsigned long;

		enum { INVALID_HANDLE = 0, };
		struct Buffer_handle
		{
			Genode::Dataspace_capability cap;
			Genode::size_t               size;
			Handle                       handle;
			Genode::addr_t               addr;

			Genode::Dataspace_capability map_cap;
			Offset                       map_offset;

			Buffer_handle(Genode::Dataspace_capability cap,
			              Genode::size_t              size,
			              Handle                      handle)
			: cap(cap), size(size), handle(handle) { }

			virtual ~Buffer_handle() { }

			bool valid() { return cap.valid() && size != 0; }
		};

		using Buffer = Genode::Registered<Buffer_handle>;

		Genode::Registry<Buffer> _buffer_registry;

		Handle _alloc_buffer(uint64_t const size)
		{
			static uint32_t handle = INVALID_HANDLE;

			Genode::size_t donate = size;
			Genode::Dataspace_capability cap = Genode::retry<Gpu::Session::Out_of_ram>(
			[&] () { return _gpu_session.alloc_buffer(size); },
			[&] () {
				_gpu_session.upgrade_ram(donate);
				donate /= 4;
			});

			/*
			 * Every buffer always is mapped into the PPGTT. To make things
			 * simplish[sic], we reuse the client local virtual adress for the
			 * PPGTT mapping.
			 */

			Genode::addr_t const addr = static_cast<Genode::addr_t>(_env.rm().attach(cap));

			bool const ppgtt = Genode::retry<Gpu::Session::Out_of_ram>(
				[&]() { return _gpu_session.map_buffer_ppgtt(cap, addr); },
				[&]() { _gpu_session.upgrade_ram(4096); }
			);

			if (!ppgtt) {
				Genode::error("could not insert buffer into PPGTT");
				_env.rm().detach(addr);
				_gpu_session.free_buffer(cap);
				return INVALID_HANDLE;
			}

			try {
				Buffer *buffer = new (&_heap) Buffer(_buffer_registry, cap, size, ++handle);
				buffer->addr = addr;
				return buffer->handle;
			} catch (...) {
				_env.rm().detach(addr);
				_gpu_session.free_buffer(cap);
				return INVALID_HANDLE;
			}
		}

		void _unmap_buffer(Buffer_handle &h)
		{
			_env.rm().detach(h.map_offset);
			h.map_offset = 0;

			_gpu_session.unmap_buffer(h.map_cap);
			h.map_cap = Genode::Dataspace_capability();

			available_gtt_size += h.size;
		}

		int _free_buffer(Handle handle)
		{
			int result = -1;

			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (h.handle != handle) { return; }

				if (h.map_cap.valid()) {
					// Genode::warning("free buffer, handle:", Genode::Hex(h.handle), " ggtt mapped");
					_unmap_buffer(h);
				}

				_env.rm().detach(h.addr);
				_gpu_session.unmap_buffer_ppgtt(h.cap, h.addr);
				_gpu_session.free_buffer(h.cap);
				result = 0;
			};
			_buffer_registry.for_each(lookup_buffer);

			return result;
		}

		Offset _map_buffer(Handle handle)
		{
			Offset offset = 0;

			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (h.handle != handle) { return; }

				if (h.map_cap.valid()) {
					offset = h.map_offset;
					return;
				}

				try {
					_gpu_session.upgrade_ram(4096);
					h.map_cap    = _gpu_session.map_buffer(h.cap, true);
					h.map_offset = static_cast<Offset>(_env.rm().attach(h.map_cap));
					offset       = h.map_offset;

					available_gtt_size -= h.size;
				} catch (...) {
					if (h.map_cap.valid()) { _gpu_session.unmap_buffer(h.map_cap); }
					h.map_cap = Genode::Dataspace_capability();
					Genode::error("could not attach GEM buffer handle: ", h.handle);
				}
			};
			_buffer_registry.for_each(lookup_buffer);

			return offset;
		}

		/*******************
		 ** lookup buffer **
		 *******************/

		Buffer_handle *_lookup_buffer(Handle handle)
		{
			Buffer_handle *bh = nullptr;

			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (h.handle != handle) { return; }
				bh = &h;
			};
			_buffer_registry.for_each(lookup_buffer);

			return bh;
		}

		Genode::Dataspace_capability _lookup_buffer_cap(Handle handle)
		{
			Genode::Dataspace_capability cap;

			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (h.handle != handle) { return; }
				if (!h.cap.valid())     { return; }
				cap = h.cap;
			};
			_buffer_registry.for_each(lookup_buffer);

			return cap;
		}

		Genode::Dataspace_capability _lookup_buffer_map_cap(Handle handle)
		{
			Genode::Dataspace_capability cap;

			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (h.handle != handle) { return; }
				if (!h.map_cap.valid()) { return; }
				cap = h.map_cap;
			};
			_buffer_registry.for_each(lookup_buffer);

			return cap;
		}

		bool _is_mapped(Genode::Dataspace_capability cap)
		{
			bool mapped = false;

			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (!(h.cap == cap))    { return; }
				if (!h.map_cap.valid()) { return; }
				mapped = true;
			};
			_buffer_registry.for_each(lookup_buffer);

			return mapped;
		}

		/***************************
		 ** execbuffer completion **
		 ***************************/

		void _handle_completion() { _completion_lock.unlock(); }

		Genode::Io_signal_handler<Drm_call> _completion_sigh {
			_env.ep(), *this, &Drm_call::_handle_completion };


		/************
		 ** ioctls **
		 ************/

		int _device_gem_get_aperture_size(void *arg)
		{
			drm_i915_gem_get_aperture * const p = reinterpret_cast<drm_i915_gem_get_aperture*>(arg);
			p->aper_size           = _gpu_info.aperture_size;
			p->aper_available_size = available_gtt_size;
			Genode::warning(__func__, ": available_gtt_size is not properly accounted");
			return 0;
		}

		int _device_gem_create(void *arg)
		{
			drm_i915_gem_create * const p = reinterpret_cast<drm_i915_gem_create*>(arg);

			uint64_t const size = (p->size + 0xfff) & ~0xfff;
			Handle handle       = _alloc_buffer(size);
			if (handle == INVALID_HANDLE) { return -1; }

			p->size   = size;
			p->handle = handle;

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", handle, " size: ", size);
			}

			return 0;
		}

		int _device_gem_mmap(void *arg)
		{
			drm_i915_gem_mmap * const p = reinterpret_cast<drm_i915_gem_mmap *>(arg);
			Handle const         handle = p->handle;

			Genode::addr_t addr = 0;
			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (h.handle != handle) { return; }
				addr = h.addr;
			};
			_buffer_registry.for_each(lookup_buffer);

			if (addr == 0) { return -1; }

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", handle, " addr: ", Genode::Hex(addr));
			}

			p->addr_ptr = addr;
			return 0;
		}

		int _device_gem_mmap_gtt(void *arg)
		{
			drm_i915_gem_mmap * const p = reinterpret_cast<drm_i915_gem_mmap *>(arg);
			Handle const handle         = p->handle;

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", handle,
				              " offset: ", Genode::Hex(p->offset));
			}

			/*
			 * We always map a buffer when the tiling is set. Since Mesa
			 * sets the filing first and maps the buffer afterwards we might
			 * already have a mapping at this point.
			 */
			p->offset = _map_buffer(handle);

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", handle,
				              " offset: ", Genode::Hex(p->offset), " (mapped)");
			}

			return p->offset ? 0 : -1;
		}

		char const *_domain_name(uint32_t d)
		{
			if (d & I915_GEM_DOMAIN_CPU)         { return "CPU"; }
			if (d & I915_GEM_DOMAIN_GTT)         { return "CPU (GTT)"; }
			if (d & I915_GEM_DOMAIN_RENDER)      { return "GPU (RC)"; }
			if (d & I915_GEM_DOMAIN_VERTEX)      { return "GPU (VC)"; }
			if (d & I915_GEM_DOMAIN_INSTRUCTION) { return "GPU (IC)"; }
			if (d & I915_GEM_DOMAIN_SAMPLER)     { return "GPU (SC)"; }
			return "N/A";
		}

		int _device_gem_set_domain(void *arg)
		{
			/* XXX check read_domains/write_domain */
			drm_i915_gem_set_domain * const p = reinterpret_cast<drm_i915_gem_set_domain*>(arg);
			Handle const handle = p->handle;
			uint32_t const rd   = p->read_domains;
			uint32_t const wd   = p->write_domain;

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", handle,
				              " rd: ", _domain_name(rd),
				              " wd: ", _domain_name(wd));
			}

			return 0;
		}

		int _device_getparam(void *arg)
		{
			drm_i915_getparam_t *p = reinterpret_cast<drm_i915_getparam_t*>(arg);
			int const param = p->param;
			int *value      = p->value;

			switch (param) {
			case I915_PARAM_CHIPSET_ID:
				*value = _gpu_info.chip_id;
				break;
			case I915_PARAM_HAS_RELAXED_FENCING:
			case I915_PARAM_HAS_RELAXED_DELTA:
			case I915_PARAM_HAS_EXECBUF2:
			case I915_PARAM_HAS_LLC:
				*value = 1;
				break;
			case I915_PARAM_HAS_BSD:
			case I915_PARAM_HAS_BLT:
			case I915_PARAM_HAS_VEBOX:
			case I915_PARAM_HAS_WAIT_TIMEOUT:
			case 36 /* I915_PARAM_HAS_RESOURCE_STREAMER */:
				*value = 0;
				break;
			case I915_PARAM_CMD_PARSER_VERSION:
				*value = 23;
				break;
			default:
				Genode::error("Unhandled device param:", Genode::Hex(param));
				return -1;
				break;
			}
			return 0;
		}

		int _device_gem_context_create(void *arg)
		{
			drm_i915_gem_context_create * const p = reinterpret_cast<drm_i915_gem_context_create*>(arg);
			p->ctx_id = _gpu_info.ctx_id;
			return 0;
		}

		int _device_gem_set_tiling(void *arg)
		{
			drm_i915_gem_set_tiling * const p = reinterpret_cast<drm_i915_gem_set_tiling*>(arg);
			Handle const handle    = p->handle;
			uint32_t const mode    = p->tiling_mode;
			uint32_t const stride  = p->stride;
			uint32_t const swizzle = p->swizzle_mode;

			if (verbose_ioctl) {
				Genode::error(__func__, ": ",
				                "handle: ", handle, " "
				                "mode: ", mode, " "
				                "stride: ", stride , " "
				                "swizzle: ", swizzle);
			}

			/*
			 * XXX instead of returning the buffer cap and later-on the map_cap
			 *     returning the buffer itself in the first place is better
			 */
			Genode::Dataspace_capability const cap = _lookup_buffer_cap(handle);
			if (!cap.valid()) { return -1; }

			/* we need a valid GGTT mapping for fencing */
			if (!_is_mapped(cap) && !_map_buffer(handle)) { return -2; }

			Genode::Dataspace_capability map_cap = _lookup_buffer_map_cap(handle);

			uint32_t const m = (stride << 16) | (mode == 1 ? 1 : 0);
			bool    const ok = _gpu_session.set_tiling(map_cap, m);
			return ok ? 0 : -1;
		}

		int _device_gem_pwrite(void *arg)
		{
			drm_i915_gem_pwrite * const p = reinterpret_cast<drm_i915_gem_pwrite*>(arg);
			Handle const handle           = p->handle;

			Buffer_handle const *bh = _lookup_buffer(handle);
			if (bh == nullptr) {
				Genode::error(__func__, ": invalid handle: ", handle);
				return -1;
			}

			if (bh->size < p->size) {
				Genode::error(__func__, ": request size: ", p->size, " "
				              "does not fit buffer size: ", bh->size);
				return -1;
			}

			if (!bh->addr) { return -1; }

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", handle, " size: ", p->size, " addr: ", Genode::Hex(bh->addr));
			}

			Genode::memcpy((void*)bh->addr, reinterpret_cast<void*>(p->data_ptr), p->size);
			return 0;
		}

		int _device_gem_sw_finish(void *arg)
		{
			// drm_i915_gem_sw_finish * const p = reinterpret_cast<drm_i915_gem_sw_finish*>(arg);
			// Handle const handle              = p->handle;
			return 0;
		}

		bool _relocate_batch_buffer(drm_i915_gem_exec_object2 &obj, size_t batch_len)
		{
			Buffer_handle const * const bb_handle = _lookup_buffer(obj.handle);
			if (bb_handle == nullptr) {
				Genode::error("batch buffer handle: ", obj.handle, " invalid");
				return false;
			}

			uint8_t *bb_addr = (uint8_t*)bb_handle->addr;

			drm_i915_gem_relocation_entry *e =
				reinterpret_cast<drm_i915_gem_relocation_entry*>(obj.relocs_ptr);

			uint32_t const count = obj.relocation_count;
			for (uint64_t i = 0; i < count; i++) {
				if (verbose_ioctl) {
					using namespace Genode;
					log("target_handle: ", e[i].target_handle, " "
					    "delta: ", Hex(e[i].delta), " "
					    "offset: ", Hex(e[i].offset), " "
					    "presumed_offset: ", Hex(e[i].presumed_offset), " "
					    "read_domains: ", Hex(e[i].read_domains),
					    " (", _domain_name(e[i].read_domains), ") "
					    "write_domain: ", Hex(e[i].write_domain),
					    " (", _domain_name(e[i].write_domain), ") ");
				}

				Buffer_handle * buffer = _lookup_buffer(e[i].target_handle);
				if (buffer == nullptr) {
					Genode::error("target_handle: ", e[i].target_handle, " invalid");
					return false;
				}
				uint64_t target_offset = Utils::canonical_addr(buffer->addr + e[i].delta);

				uint32_t *addr = (uint32_t*)(bb_addr + e[i].offset);
				*addr       = (target_offset & 0xffffffff);
				*(addr + 1) = (target_offset >> 32) & 0xffffffff;

				e[i].presumed_offset = target_offset;
			}

			return true;
		}

		int _device_gem_execbuffer2(void *arg)
		{
			drm_i915_gem_execbuffer2 * const p = reinterpret_cast<drm_i915_gem_execbuffer2*>(arg);
			uint64_t const buffers_ptr         = p->buffers_ptr;
			uint64_t const buffer_count        = p->buffer_count;
			uint32_t const batch_start_offset  = p->batch_start_offset;
			uint32_t const batch_len           = p->batch_len;
			uint32_t const dr1                 = p->DR1;
			uint32_t const dr4                 = p->DR4;
			uint32_t const num_cliprects       = p->num_cliprects;
			uint64_t const cliprects_ptr       = p->cliprects_ptr;
			uint64_t const flags               = p->flags;
			uint64_t const ctx_id              = p->rsvd1;

			uint64_t const batch_buffer_id     = buffer_count - 1;

			if (verbose_ioctl) {
				Genode::log(__func__, " "
				            "buffers_ptr: ",        Genode::Hex(buffers_ptr),        " "
				            "buffer_count: ",       buffer_count,                    " "
				            "batch_start_offset: ", Genode::Hex(batch_start_offset), " "
				            "batch_len: ",          batch_len,                       " "
				            "dr1: ",                Genode::Hex(dr1),                " "
				            "dr4: ",                Genode::Hex(dr4),                " "
				            "num_cliprects: ",      num_cliprects,                   " "
				            "cliprects_ptr: ",      Genode::Hex(cliprects_ptr),      " "
				            "flags: ",              Genode::Hex(flags),              " "
				            "ctx_id: ",             Genode::Hex(ctx_id));
			}

			drm_i915_gem_exec_object2 *obj =
				reinterpret_cast<drm_i915_gem_exec_object2*>(buffers_ptr);

			for (uint64_t i = 0; i < buffer_count; i++) {
				if (verbose_ioctl) {
					Genode::log("  obj[", i, "] ",
					            "handle: ", obj[i].handle, " "
					            "relocation_count: ", obj[i].relocation_count, " "
					            "relocs_ptr: ", Genode::Hex(obj[i].relocs_ptr), " "
					            "alignment: ", Genode::Hex(obj[i].alignment), " "
					            "offset: ", Genode::Hex(obj[i].offset), " "
					            "flags: ", Genode::Hex(obj[i].flags));
				}

				Buffer_handle *bh = _lookup_buffer(obj[i].handle);
				if (!bh || !bh->cap.valid()) {
					Genode::error("handle: ", obj[i].handle, " invalid");
					return -1;
				}

				obj[i].offset = bh->addr;

				if (obj[i].relocation_count > 0) {
					drm_i915_gem_relocation_entry *e =
						reinterpret_cast<drm_i915_gem_relocation_entry*>(obj[i].relocs_ptr);
					uint32_t const count = obj[i].relocation_count;
					for (uint64_t i = 0; i < count; i++) {
						if (verbose_ioctl) {
							Genode::log("   ",
							           "target_handle: ", e[i].target_handle, " "
							           "delta: ", Genode::Hex(e[i].delta), " "
							           "offset: ", Genode::Hex(e[i].offset), " "
							           "presumed_offset: ", Genode::Hex(e[i].presumed_offset), " "
							           "read_domains: ", Genode::Hex(e[i].read_domains), " "
							           "write_domain: ", Genode::Hex(e[i].write_domain));
						}

						Genode::Dataspace_capability cap = _lookup_buffer_cap(e[i].target_handle);
						if (!cap.valid()) {
							Genode::error("target_handle: ", e[i].target_handle, " invalid");
							return -1;
						}
					}
				}
			}

			/* relocate in batch buffer and copy object to command buffer */
			drm_i915_gem_exec_object2 &bb = obj[batch_buffer_id];
			if (!_relocate_batch_buffer(bb, batch_len)) {
				Genode::error("could not relocate batch buffer objects");
				return -1;
			}

			Genode::Dataspace_capability bb_cap = _lookup_buffer_cap(bb.handle);
			if (!bb_cap.valid()) {
				Genode::error("batch buffer cap invalid");
				return -1;
			}

			_gpu_session.exec_buffer(bb_cap, batch_len);
			return 0;
		}

		int _device_gem_busy(void *arg)
		{
			drm_i915_gem_busy * const p = reinterpret_cast<drm_i915_gem_busy*>(arg);
			// Handle const handle = p->handle;
			// uint32_t busy = p->busy;
			/* TODO flag currently executed buffer */
			p->busy = 0;
			return 0;
		}

		int _device_gem_madvise(void *arg)
		{
			drm_i915_gem_madvise * const p = reinterpret_cast<drm_i915_gem_madvise*>(arg);
			// Handle const handle = p->handle;
			// uint32_t const madv = p->madv;
			/* all buffer are always available */
			p->retained = 1;
			return 0;
		}

		int _device_ioctl(unsigned cmd, void *arg)
		{
			switch (cmd) {
			case DRM_I915_GEM_GET_APERTURE:   return _device_gem_get_aperture_size(arg);
			case DRM_I915_GETPARAM:           return _device_getparam(arg);
			case DRM_I915_GEM_CREATE:         return _device_gem_create(arg);
			case DRM_I915_GEM_MMAP:           return _device_gem_mmap(arg);
			case DRM_I915_GEM_MMAP_GTT:       return _device_gem_mmap_gtt(arg);
			case DRM_I915_GEM_SET_DOMAIN:     return _device_gem_set_domain(arg);
			case DRM_I915_GEM_CONTEXT_CREATE: return _device_gem_context_create(arg);
			case DRM_I915_GEM_SET_TILING:     return _device_gem_set_tiling(arg);
			case DRM_I915_GEM_PWRITE:         return _device_gem_pwrite(arg);
			case DRM_I915_GEM_SW_FINISH:      return _device_gem_sw_finish(arg);
			case DRM_I915_GEM_EXECBUFFER2:    return _device_gem_execbuffer2(arg);
			case DRM_I915_GEM_BUSY:           return _device_gem_busy(arg);
			case DRM_I915_GEM_MADVISE:        return _device_gem_madvise(arg);
			default:
				Genode::error("Unhandled device specific ioctl:", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

		int _generic_gem_close(void *arg)
		{
			drm_gem_close * const p = reinterpret_cast<drm_gem_close*>(arg);
			return _free_buffer(p->handle);
		}

		int _generic_gem_flink(void *arg)
		{
			drm_gem_flink * const p = reinterpret_cast<drm_gem_flink*>(arg);
			p->name = p->handle;
			return 0;
		}

		int _generic_ioctl(unsigned cmd, void *arg)
		{
			switch (cmd) {
			case DRM_NUMBER(DRM_IOCTL_GEM_CLOSE): return _generic_gem_close(arg);
			case DRM_NUMBER(DRM_IOCTL_GEM_FLINK): return _generic_gem_flink(arg);
			default:
				Genode::error("Unhandled generic DRM ioctl:", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

	public:

		Drm_call(Genode::Env &env, Genode::Entrypoint &signal_ep)
		: _env(env),
			_completion_sigh(signal_ep, *this, &Drm_call::_handle_completion)
		{
			_gpu_session.completion_sigh(_completion_sigh);
		}

		bool map_buffer_ggtt(Offset offset, size_t length)
		{
			bool result = false;

			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (h.map_offset != offset) { return; }
				result = true;
			};
			_buffer_registry.for_each(lookup_buffer);

			if (!result) {
				Genode::error("could not lookup buffer for offset: ", offset);
			}

			return result;
		}

		void unmap_buffer_ggtt(void *addr, size_t length)
		{
			Offset const offset = (Offset const)addr;

			auto lookup_buffer = [&] (Buffer_handle &h) {
				if (h.map_offset != offset) { return; }

				if (!h.map_cap.valid()) {
					Genode::error("no valid capability found for offset: ", Genode::Hex(offset));
					return;
				}

				_unmap_buffer(h);
			};
			_buffer_registry.for_each(lookup_buffer);
		}

		int ioctl(unsigned long request, void *arg)
		{
			bool const device = device_ioctl(request);
			return device ? _device_ioctl(device_number(request), arg)
			              : _generic_ioctl(command_number(request), arg);
		}

		void wait_for_completion() { _completion_lock.lock(); }
};


static Genode::Constructible<Drm_call> _call;


void drm_init(Genode::Env &env, Genode::Entrypoint &signal_ep)
{
	_call.construct(env, signal_ep);
}


void drm_complete() { _call->wait_for_completion(); }


/**
 * Mmap buffer object
 *
 * On Genode the virtual address of MMAP_GTT is stored in the offset.
 */
extern "C" void *drm_mmap(void *addr, size_t length, int prot, int flags,
                             int fd, off_t offset)
{
	/* sanity check if we got a GTT mapped offset */
	bool const ok = _call->map_buffer_ggtt(offset, length);
	return ok ? (void *)offset : nullptr;
}

/**
 * Unmap buffer object
 */
extern "C" int drm_munmap(void *addr, size_t length)
{
	_call->unmap_buffer_ggtt(addr, length);
	return 0;
}


extern "C" int genode_ioctl(int fd, unsigned long request, void *arg)
{
	if (verbose_ioctl) { dump_ioctl(request); }
	int const ret = _call->ioctl(request, arg);
	if (verbose_ioctl) { Genode::log("returned ", ret); }
	return ret;
}
