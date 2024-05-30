/*
 * \brief  DRM ioctl backend
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Alexander Boettcher
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/registry.h>
#include <base/sleep.h>

#include <gpu_session/connection.h>
#include <gpu/info_intel.h>
#include <util/dictionary.h>
#include <util/retry.h>

#include <vfs_gpu.h>

extern "C" {
#include <errno.h>
#include <drm.h>
#include <i915_drm.h>
#include <xf86drm.h>
#define DRM_NUMBER(req) ((req) & 0xff)
}

namespace Libc {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
}

/*
 * This is currently not in upstream libdrm (2.4.120) but in internal Mesa
 * 'i195_drm.h'
 */
#ifndef I915_PARAM_PXP_STATUS
/*
 * Query the status of PXP support in i915.
 *
 * The query can fail in the following scenarios with the listed error codes:
 *     -ENODEV = PXP support is not available on the GPU device or in the
 *               kernel due to missing component drivers or kernel configs.
 *
 * If the IOCTL is successful, the returned parameter will be set to one of
 * the following values:
 *     1 = PXP feature is supported and is ready for use.
 *     2 = PXP feature is supported but should be ready soon (pending
 *         initialization of non-i915 system dependencies).
 *
 * NOTE: When param is supported (positive return values), user space should
 *       still refer to the GEM PXP context-creation UAPI header specs to be
 *       aware of possible failure due to system state machine at the time.
 */
#define I915_PARAM_PXP_STATUS 58
#endif

using Genode::addr_t;
using Genode::Attached_dataspace;
using Genode::Constructible;

namespace Drm
{
	using namespace Gpu;
	struct Call;
	struct Context;
	struct Buffer;

	using Buffer_id = Genode::Id_space<Buffer>::Id;
	using Buffer_space = Genode::Id_space<Buffer>;
}

enum { verbose_ioctl = false };

namespace Utils
{
	Gpu::Virtual_address limit_to_48bit(Gpu::Virtual_address addr) {
		return Gpu::Virtual_address { addr.value & ((1ULL << 48) - 1) }; }
}

namespace Gpu
{
	struct Vram_allocator;
}


/**
 * Get DRM command number
 */
static unsigned long command_number(unsigned long request)
{
	return request & 0xffu;
}


/**
 * Get device specific command number
 */
static unsigned long device_number(unsigned long request)
{
	return command_number(request) - DRM_COMMAND_BASE;
}


/**
 * Check if request is device command
 */
static bool device_ioctl(unsigned long request)
{
	unsigned long const cmd = command_number(request);
	return cmd >= DRM_COMMAND_BASE && cmd < DRM_COMMAND_END;
}


static const char *command_name(unsigned long request)
{
	if (IOCGROUP(request) != DRM_IOCTL_BASE)
		return "<non-DRM>";


	if (!device_ioctl(request)) {
		long const cmd = command_number(request);
		switch (cmd) {
		case DRM_NUMBER(DRM_IOCTL_GEM_CLOSE): return "DRM_IOCTL_GEM_CLOSE";
		case DRM_NUMBER(DRM_IOCTL_GEM_FLINK): return "DRM_IOCTL_GEM_FLINK";
		case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_CREATE): return "DRM_IOCTL_SYNCOBJ_CREATE";
		case DRM_NUMBER(DRM_IOCTL_PRIME_HANDLE_TO_FD): return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
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
	case DRM_I915_GEM_CONTEXT_DESTROY:   return "DRM_I915_GEM_CONTEXT_DESTROY";
	case DRM_I915_GEM_SET_CACHING:       return "DRM_I915_GEM_SET_CACHING";
	default:
		return "<unknown driver>";
	}
}

static void dump_ioctl(unsigned long request)
{
	using namespace Genode;

	log("ioctl(request=", Hex(request),
	    (request & 0xe0000000) == IOC_OUT   ? " out"   :
	    (request & 0xe0000000) == IOC_IN    ? " in"    :
	    (request & 0xe0000000) == IOC_INOUT ? " inout" : " void",
	    " len=", IOCPARM_LEN(request),
	    " cmd=",command_name(request), " (", Hex(command_number(request)), ")");
}


namespace {
	using Offset = unsigned long;
} /* anonymous namespace */


/*
 * GPU graphics memory
 */
struct Gpu::Vram
{
	Gpu::Vram_id_space::Element  const _elem;
	Genode::Dataspace_capability const _cap;
	Genode::Allocator_avl              _alloc;

	Vram(Gpu::Connection    &gpu,
	     Genode::Allocator  &md_alloc,
	     Gpu::Vram_id_space &space,
	     Genode::size_t      size)
	: _elem(*this, space),
	  _cap(gpu.alloc_vram(_elem.id(), size)),
	  _alloc(&md_alloc)
	{
		_alloc.add_range(0, Genode::Dataspace_client(_cap).size());
	}

	struct Allocation
	{
		Gpu::Vram_id                 id;
		Genode::Dataspace_capability cap;
		Genode::off_t                offset;
		Genode::size_t               size { 0 };

		bool valid() const { return size > 0; }
	};

	Allocation alloc(Genode::size_t size)
	{
		return _alloc.alloc_aligned(size, 12).convert<Allocation>(
			[&] (void *offset) {
				return Allocation { _elem.id(), _cap, Genode::off_t(offset), size };
			},
			[&] (Genode::Allocator::Alloc_error err) -> Allocation {
				return Allocation();
			});
	}

	void free(Allocation &allocation) { _alloc.free((void *)allocation.offset); };
};


struct Gpu::Vram_allocator
{
	enum { VRAM_BLOCK_SIZE = 16*1024*1024 };
	Gpu::Connection       &_gpu;
	Genode::Allocator     &_md_alloc;
	Gpu::Vram_id_space     _vram_space { };

	Vram_allocator(Gpu::Connection &gpu, Genode::Allocator &md_alloc)
	: _gpu(gpu), _md_alloc(md_alloc)
	{ }

	Vram::Allocation alloc(Genode::size_t size)
	{
		Vram::Allocation allocation { };

		if (size <= VRAM_BLOCK_SIZE)
			_vram_space.for_each<Vram>([&] (Vram &vram) {
				if (allocation.valid()) return;
				allocation = vram.alloc(size);
			});

		if (allocation.valid()) return allocation;

		/* alloc more Vram from session */
		Vram *vram = new (_md_alloc) Vram(_gpu, _md_alloc, _vram_space,
		                                  size <= VRAM_BLOCK_SIZE ? VRAM_BLOCK_SIZE : size);
		return vram->alloc(size);
	}

	void free(Vram::Allocation &allocation)
	{
		if (allocation.valid() == false) return;

		try {
			_vram_space.apply<Vram>(allocation.id, [&](Vram &vram) {
				vram.free(allocation);
			});
		} catch (Gpu::Vram_id_space::Unknown_id) {
			Genode::error(__func__, ": id ", allocation.id, " invalid");
		}
	}
};


/*
 * Buffer object abstraction for Mesa/Iris
 */
struct Drm::Buffer
{
	Genode::Env &_env;
	Genode::Id_space<Drm::Buffer>::Element const _elem;

	Vram::Allocation _allocation;
	Gpu::addr_t _local_addr { 0 };

	/* handle id's have to start at 1 (0) is invalid */
	static Drm::Buffer_id _new_id()
	{
		static unsigned long _id = 0;
		return Drm::Buffer_id { ++_id };
	}

	Buffer(Genode::Env              &env,
	       Genode::Id_space<Buffer> &space,
	       Vram::Allocation          allocation)
	:
		_env { env }, _elem { *this, space, _new_id() }, _allocation { allocation }
	{ }

	~Buffer()
	{
		unmap();
	}

	bool mmap(Genode::Env &env)
	{
		if (_local_addr) return true;

		_local_addr = Gpu::addr_t(env.rm().attach(_allocation.cap, _allocation.size,
		                                          _allocation.offset));
		return true;
	}

	void unmap()
	{
		if (_local_addr)
			_env.rm().detach((void *)_local_addr);

		_local_addr = 0;
	}

	Gpu::addr_t mmap_addr() const { return _local_addr; }

	Vram::Allocation &vram() { return _allocation; }

	Drm::Buffer_id id() const
	{
		return Drm::Buffer_id { _elem.id().value };
	}
};


/*
 * Used to implement OpenGL contexts. Each context uses a dedictated GPU
 * session which provides  a separate GPU context (e.g., page tables, exec
 * lists, ...) within the intel_gpu driver.
 */
struct Drm::Context
{
	/*
	 * A context has to make sure a buffer is mapped in it's address space (i.e.,
	 * it's GPU page tables = PPGTT), a buffer is executed within this GPU
	 * context.
	 */
	struct Buffer
	{
		using Id_space = Genode::Id_space<Buffer>;
		Id_space::Element const elem;

		Vram::Allocation     vram;
		Gpu::Virtual_address  gpu_vaddr { };
		Gpu::Sequence_number seqno { };
		bool                 gpu_vaddr_valid { false };
		bool                 busy            { false };

		Buffer(Id_space &space, Id_space::Id id, Vram::Allocation vram)
		: elem(*this, space, id),
		  vram(vram)
		{ }

		Gpu::Vram_id vram_id() const
		{
			return Gpu::Vram_id { .value = elem.id().value };
		}

		Id_space::Id id() const
		{
			return elem.id();
		}
	};

	Gpu::Connection   &_gpu;
	Gpu::Connection   &_gpu_master;
	Genode::Allocator &_alloc;
	Drm::Buffer_space &_drm_buffer_space;

	Gpu::Info_intel  const &_gpu_info {
		*_gpu.attached_info<Gpu::Info_intel>() };

	int _fd;

	using Id_space = Genode::Id_space<Context>;
	Id_space::Element const _elem;

	Genode::Id_space<Buffer> _buffer_space { };

	struct Vram_map : Genode::Dictionary<Vram_map, unsigned long>::Element
	{
		Vram_map(Genode::Dictionary<Vram_map, unsigned long> &dict,
		         Gpu::Vram_id const &id)
		: Genode::Dictionary<Vram_map, unsigned long>::Element(dict, id.value) { }
	};

	Genode::Dictionary<Vram_map, unsigned long> _vram_map { };

	void _wait_for_completion(Gpu::Sequence_number seqno)
	{
		while (true) {
			if (_gpu.complete(seqno)) {
				break;
			}

			/* wait for completion signal in VFS plugin */
			char buf;
			Libc::read(_fd, &buf, 1);
		}

		/* mark done buffer objects */
		_buffer_space.for_each<Buffer>([&] (Buffer &b) {
			if (!b.busy) return;
			if (b.seqno.value > _gpu_info.last_completed.value) return;
			b.busy = false;
		});
	}

	void _wait(Buffer::Id_space::Id id)
	{
		bool busy = true;

		while (busy) {

			Gpu::Sequence_number seqno { };
			try {
				_buffer_space.apply<Buffer>(id, [&](Buffer &b) {
					busy = b.busy;
					seqno = b.seqno;
				});
			} catch (Buffer::Id_space::Unknown_id) {
				Genode::error(__func__, ": id ", id, " invalid");
				return;
			}

			if (!busy)
				break;

			_wait_for_completion(seqno);
		}
	}

	void _map_buffer_gpu(Buffer &buffer, Gpu::Virtual_address vaddr)
	{
		Genode::retry<Gpu::Session::Out_of_ram>(
		[&]() {
			Genode::retry<Gpu::Session::Out_of_caps>(
			[&] () {
				_gpu.map_gpu(buffer.vram.id,
				             buffer.vram.size,
				             buffer.vram.offset,
				             Utils::limit_to_48bit(vaddr));
				buffer.gpu_vaddr       = vaddr;
				buffer.gpu_vaddr_valid = true;
			},
			[&] () {
				_gpu.upgrade_caps(2);
			});
		},
		[&] () {
			_gpu.upgrade_ram(1024*1024);
		});
	}

	void _unmap_buffer_gpu(Buffer &buffer)
	{
		if (!buffer.gpu_vaddr_valid)
			return;

		_gpu.unmap_gpu(buffer.vram.id,
		               buffer.vram.offset,
		               Utils::limit_to_48bit(buffer.gpu_vaddr));
		buffer.gpu_vaddr_valid = false;
	}

	void _import_vram(Gpu::Vram_id id)
	{
		if (_vram_map.exists(id.value)) return;

		Gpu::Vram_capability cap = _gpu_master.export_vram(id);

		Genode::retry<Gpu::Session::Out_of_ram>(
		[&]() {
			Genode::retry<Gpu::Session::Out_of_caps>(
			[&] () {
				_gpu.import_vram(cap, id);
				new (_alloc) Vram_map(_vram_map, id);
			},
			[&] () {
				_gpu.upgrade_caps(2);
			});
		},
		[&] () {
			_gpu.upgrade_ram(1024*1024);
		});
	}

	void _import_buffer(Buffer::Id_space::Id id, Drm::Buffer &buffer)
	{
		/* import Vram if not present in this GPU connection */
		_import_vram(buffer.vram().id);

		new (_alloc) Buffer(_buffer_space, id, buffer.vram());
	}

	Context(Gpu::Connection &gpu, Gpu::Connection &gpu_master,
	        Genode::Allocator &alloc, int fd,
	        Genode::Id_space<Context> &space,
	        Drm::Buffer_space &drm_buffer_space)
	:
	  _gpu(gpu), _gpu_master(gpu_master), _alloc(alloc),
	  _drm_buffer_space(drm_buffer_space), _fd(fd), _elem (*this, space)
	  { }

	~Context()
	{
		while (_vram_map.with_any_element([&] (Vram_map &map) {
			_gpu.free_vram(Gpu::Vram_id { .value = map.name });;
			destroy(_alloc, &map);
		})) { }
	}

	unsigned long id() const
	{
		return _elem.id().value + 1;
	}

	static Id_space::Id id(unsigned long value)
	{
		return Id_space::Id { .value = value - 1 };
	}

	int fd() const { return _fd; }

	void free_buffer(Drm::Buffer_id id)
	{
		try {
			_buffer_space.apply<Buffer>(Buffer::Id_space::Id { .value = id.value },
			                            [&] (Buffer &buffer) {
			  _unmap_buffer_gpu(buffer);
				destroy(_alloc, &buffer);
			});
		} catch (Buffer::Id_space::Unknown_id) { return; }
	}

	void free_buffers()
	{
		while (_buffer_space.apply_any<Buffer>([&] (Buffer &buffer) {
			_unmap_buffer_gpu(buffer);
			destroy(_alloc, &buffer);
		})) { }
	}

	void unmap_buffer_gpu(Drm::Buffer_id id)
	{
		try {
			_buffer_space.apply<Buffer>(Buffer::Id_space::Id { .value = id.value },
			                            [&] (Buffer &buffer) {
				_unmap_buffer_gpu(buffer);
			});
		} catch (Buffer::Id_space::Unknown_id) { }
	}

	Genode::Mutex _exec_mutex { };
	unsigned      _exec_counter { 0 };

	struct Exec_mutex_check
	{
		unsigned &_counter;

		Exec_mutex_check(unsigned &counter)
		: _counter(counter)
		{
			_counter++;
		}

		~Exec_mutex_check() { _counter--; }
	};

	int exec_buffer(drm_i915_gem_exec_object2 *obj, uint64_t count,
	                uint64_t batch_id, size_t batch_length)
	{
		if (_exec_counter > 0)
			Genode::warning("Parallel calls to '", __func__, "' are ",
			                "unsupported. This call may block forever");

		Genode::Mutex::Guard guard      { _exec_mutex   };
		Exec_mutex_check     exec_guard { _exec_counter };

		Buffer *command_buffer = nullptr;

		for (uint64_t i = 0; i < count; i++) {
			if (verbose_ioctl) {
				Genode::log("  obj[", i, "] ",
				            "handle: ", obj[i].handle, " "
				            "relocation_count: ", obj[i].relocation_count, " "
				            "relocs_ptr: ", Genode::Hex(obj[i].relocs_ptr), " "
				            "alignment: ", Genode::Hex(obj[i].alignment), " "
				            "offset: ", Genode::Hex(obj[i].offset), " "
				            "flags: ", Genode::Hex(obj[i].flags));
			}

			if (obj[i].relocation_count > 0) {
				Genode::error("no relocation supported");
				return -1;
			}

			int ret = -1;
			Buffer::Id_space::Id const id  { .value = obj[i].handle };

			try {
				_buffer_space.apply<Buffer>(id, [&](Buffer &b) {

					if (b.busy)
						Genode::warning("handle: ", obj[i].handle, " reused but is busy");

					if (b.gpu_vaddr_valid && b.gpu_vaddr.value != obj[i].offset) {
						_unmap_buffer_gpu(b);
					}

					if (!b.gpu_vaddr_valid)
						_map_buffer_gpu(b, Gpu::Virtual_address { .value = obj[i].offset });

					if (!b.gpu_vaddr_valid) {
						Genode::error("handle: ", obj[i].handle,
						              " gpu_vaddr invalid for context ", id);
						return;
					}

					b.busy = true;

					if (i == batch_id)
						command_buffer = &b;

					ret = 0;
				});
			} catch (Buffer::Id_space::Unknown_id) {
					Drm::Buffer_id drm_id { .value = id.value };
					_drm_buffer_space.apply<Drm::Buffer>(drm_id, [&](Drm::Buffer &buffer) {
						_import_buffer(id, buffer);
					});
					i--;
					continue;
			}

			if (ret) {
				Genode::error("handle: ", obj[i].handle, " invalid, ret=", ret);
				return ret;
			}
		}

		if (!command_buffer)
			return -1;

		command_buffer->seqno = _gpu.execute(command_buffer->vram.id,
		                                     command_buffer->vram.offset);

		for (uint64_t i = 0; i < count; i++) {
			Buffer::Id_space::Id const id { .value = obj[i].handle };
			_buffer_space.apply<Buffer>(id, [&](Buffer &b) {
				b.seqno = command_buffer->seqno;
			});
		}

		/*
		 * Always wait for buffer to complete to avoid race between map and unmap
		 * of signal ep, the original drm_i915_gem_wait simply 0 now
		 */
		_wait(command_buffer->id());

		return 0;
	}
};


class Drm::Call
{
	private:

		using Cap_quota = Genode::Cap_quota;
		using Ram_quota = Genode::Ram_quota;

		Genode::Env      &_env { *vfs_gpu_env() };
		Genode::Heap      _heap { _env.ram(), _env.rm() };
		Gpu::Connection   _gpu_session { _env };
		Genode::Mutex     _drm_mutex { };

		Gpu::Info_intel  const &_gpu_info {
			*_gpu_session.attached_info<Gpu::Info_intel>() };

		size_t _available_gtt_size { _gpu_info.aperture_size };

		Vram_allocator _vram_allocator { _gpu_session, _heap };

		Buffer_space _buffer_space { };

		using Context_id    = Genode::Id_space<Drm::Context>::Id;
		using Context_space = Genode::Id_space<Drm::Context>;
		Context_space _context_space { };

		template <typename FN> void _gpu_op( FN const &fn)
		{
			Genode::retry<Gpu::Session::Out_of_ram>(
			[&] () {
				Genode::retry<Gpu::Session::Out_of_caps>(
				[&] () {
					fn();
				},
				[&] () {
					_gpu_session.upgrade_caps(2);
				});
			},
			[&] () {
				/* heap allocation granularity */
				_gpu_session.upgrade_ram(2*1024*1024);
			});
		}

		struct Sync_obj
		{
			using Sync = Genode::Id_space<Sync_obj>;
			using Id = Sync::Id;

			Sync::Element id;

			Sync_obj(Sync &space)
			: id(*this, space)
			{ }
		};

		Genode::Id_space<Sync_obj> _sync_objects { };

		template <typename FUNC>
		void _alloc_buffer(uint64_t const size, FUNC const &fn)
		{
			Buffer *buffer  { nullptr };

			_gpu_op([&] () {
				Vram::Allocation vram = _vram_allocator.alloc(Genode::align_addr(size, 12));
				if (vram.valid() == false) {
					Genode::error("VRAM allocation of size ", size/1024, "KB failed");
					return;
				}
				buffer = new (_heap) Buffer(_env, _buffer_space, vram);
			});

			if (buffer)
				fn(*buffer);
		}

		int _free_buffer(Drm::Buffer_id const id)
		{
			try {
				_buffer_space.apply<Buffer>(id, [&] (Buffer &b) {

					_context_space.for_each<Drm::Context>([&] (Drm::Context &context) {
						context.free_buffer(b.id()); });

					_vram_allocator.free(b.vram());
					Genode::destroy(&_heap, &b);
				});

				return 0;
			} catch (Genode::Id_space<Buffer>::Unknown_id) {
				Genode::error(__func__, ": invalid handle ", id.value);
				return -1;
			}
		}

		/************
		 ** ioctls **
		 ************/

		int _device_gem_get_aperture_size(void *arg)
		{
			drm_i915_gem_get_aperture * const p = reinterpret_cast<drm_i915_gem_get_aperture*>(arg);
			p->aper_size           = _gpu_info.aperture_size;
			p->aper_available_size = _available_gtt_size;
			Genode::warning(__func__, ": available_gtt_size (", p->aper_size/1024, " KB) is not properly accounted");
			return 0;
		}

		int _device_gem_create(void *arg)
		{
			Genode::Mutex::Guard guard { _drm_mutex };

			auto const p = reinterpret_cast<drm_i915_gem_create*>(arg);

			uint64_t const size = (p->size + 0xfff) & ~0xfff;

			bool successful = false;
			_alloc_buffer(size, [&](Buffer const &b) {
				p->size   = size;
				p->handle = b.id().value;
				successful = true;

				if (verbose_ioctl) {
					Genode::error(__func__, ": ", "handle: ", b.id().value,
					              " size: ", size);
				}
			});
			return successful ? 0 : -1;
		}

		int _device_gem_mmap(void *arg)
		{
			Genode::Mutex::Guard guard { _drm_mutex };

			auto      const p      = reinterpret_cast<drm_i915_gem_mmap *>(arg);
			Drm::Buffer_id const id { .value = p->handle };

			bool map_failed { true };

			try {
				_buffer_space.apply<Buffer>(id, [&] (Buffer &b) {
					if (b.mmap(_env)) {
						p->addr_ptr = b.mmap_addr();
						map_failed  = false;
					}
				});
			} catch (Genode::Id_space<Buffer>::Unknown_id) { }

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", id,
				              map_failed ? " buffer inaccessible" : "",
				              " flags=", p->flags,
				              " addr=", Genode::Hex(p->addr_ptr));
			}

			if (map_failed)
				return -1;

			return 0;
		}

		int _device_gem_mmap_gtt(void *arg)
		{
			Genode::error(__func__, " not implemented");
			while (1) ;
			return -1;
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
			auto      const p  = reinterpret_cast<drm_i915_gem_set_domain*>(arg);
			Gpu::Vram_id const id { .value = p->handle };
			uint32_t  const rd = p->read_domains;
			uint32_t  const wd = p->write_domain;

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", id.value,
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
			case I915_PARAM_HAS_CONTEXT_ISOLATION:
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
			case I915_PARAM_HAS_RESOURCE_STREAMER:
			case 54 /* I915_PARAM_PERF_REVISION */:
				*value = 0;
				break;
			case I915_PARAM_REVISION:
				*value = _gpu_info.revision.value;
				return 0;
			case I915_PARAM_CS_TIMESTAMP_FREQUENCY:
				*value = _gpu_info.clock_frequency.value;
				if (verbose_ioctl && *value == 0)
					Genode::error("I915_PARAM_CS_TIMESTAMP_FREQUENCY not supported");
				return *value ? 0 : -1;
			case I915_PARAM_SLICE_MASK:
				*value = _gpu_info.slice_mask.value;
				return 0;
			case I915_PARAM_EU_TOTAL:
				*value = _gpu_info.eus.value;
				return 0;
			case I915_PARAM_SUBSLICE_TOTAL:
				*value = _gpu_info.subslices.value;
				return 0;
			case I915_PARAM_SUBSLICE_MASK:
				*value = _gpu_info.subslice_mask.value;
				return 0;
			case I915_PARAM_MMAP_GTT_VERSION:
				*value = 0; /* XXX */
				if (verbose_ioctl)
					Genode::warning("I915_PARAM_MMAP_GTT_VERSION ", *value);
				return 0;
			/* validates user pointer and size */
			case I915_PARAM_HAS_USERPTR_PROBE:
				*value = 0;
				return 0;
			/*
			 * Protected Xe Path (PXP) hardware/ME feature (encrypted video memory, TEE,
			 * ...)
			 */
			case I915_PARAM_PXP_STATUS:
				*value = 0;
				errno = ENODEV;
				return -1;
			default:
				Genode::error("Unhandled device param:", Genode::Hex(param));
				return -1;
				break;
			}
			return 0;
		}

		int _device_gem_context_create(void *arg)
		{
			Genode::Mutex::Guard guard { _drm_mutex };

			drm_i915_gem_context_create * const p = reinterpret_cast<drm_i915_gem_context_create*>(arg);

			int fd = Libc::open("/dev/gpu", 0);
			if (fd < 0) {
				Genode::error("Failed to open '/dev/gpu': ",
				              "try configure '<gpu>' in 'dev' directory of VFS'");
				return -1;
			}

			struct Libc::stat buf;
			if (fstat(fd, &buf) < 0) {
				Genode::error("Could not stat '/dev/gpu'");
				return -1;
			}

			/* use inode to retrieve GPU connection */
			Gpu::Connection *gpu = vfs_gpu_connection(buf.st_ino);
			if (!gpu) {
				Genode::error("Could not find GPU session for id: ", buf.st_ino);
				Libc::close(fd);
				return -1;
			}

			Drm::Context *context = new (_heap) Drm::Context(*gpu, _gpu_session, _heap,
			                                                 fd, _context_space, _buffer_space);

			p->ctx_id = context->id();

			return 0;
		}

		int _device_gem_context_destroy(void *arg)
		{
			Genode::Mutex::Guard guard { _drm_mutex };

			unsigned long ctx_id = reinterpret_cast<drm_i915_gem_context_destroy *>(arg)->ctx_id;
			Context_id const id  = Drm::Context::id(ctx_id);

			try {
				_context_space.apply<Drm::Context>(id, [&] (Drm::Context &context) {
					context.free_buffers();
					/* GPU session fd */
					int fd = context.fd();
					destroy(_heap, &context);
					Libc::close(fd);
				});
			} catch (Drm::Context::Id_space::Unknown_id) { }

			return 0;
		}


		int _device_gem_context_set_param(void *arg)
		{
			auto * const p = reinterpret_cast<drm_i915_gem_context_param*>(arg);

			switch (p->param) {
			case I915_CONTEXT_PARAM_PRIORITY:
				return 0;
			case I915_CONTEXT_PARAM_RECOVERABLE:
				return 0;
			/*
			 * The id of the associated virtual memory address space (ppGTT) of
			 * this context. Can be retrieved and passed to another context
			 * (on the same fd) for both to use the same ppGTT and so share
			 * address layouts, and avoid reloading the page tables on context
			 * switches between themselves.
			 *
			 * This is currently not supported.
			 */
			case I915_CONTEXT_PARAM_VM:
				return 0;

			default:
				Genode::error(__func__, " unknown param=", p->param);
				return -1;
			};
		}

		int _device_gem_context_get_param(void *arg)
		{
			auto * const p = reinterpret_cast<drm_i915_gem_context_param*>(arg);

			switch (p->param) {
			case I915_CONTEXT_PARAM_SSEU:
				return 0;

			/* addressable VM area (PPGTT 48Bit - one page) for GEN8+ */
			case I915_CONTEXT_PARAM_GTT_SIZE:
				p->value = (1ull << 48) - 0x1000;
				return 0;

			/* global VM used for sharing BOs between contexts -> not supported so far */
			case I915_CONTEXT_PARAM_VM:
				return 0;

			default:
				Genode::error(__func__, " ctx=", p->ctx_id, " param=", p->param, " size=", p->size, " value=", Genode::Hex(p->value));
				return -1;
			}
		}

		int _device_gem_set_tiling(void *arg)
		{
			/*
			 * Tiling is only relevant in case something is mapped through the
			 * aperture. Iris sets tiling but never seems to establish mappings
			 * through the GTT, i.e., _device_gem_mmap_gtt which displays a "not
			 * implemented error"). In case this function is called again, tiling
			 * becomes also relevant.
			 */
			auto const p = reinterpret_cast<drm_i915_gem_set_tiling*>(arg);
			uint32_t const mode = p->tiling_mode;

			if (mode != I915_TILING_NONE) {
				if (verbose_ioctl) Genode::warning(__func__, " mode != I915_TILING_NONE (", mode, ") unsupported");
				return 0;
			}

			return 0;
		}

		int _device_gem_sw_finish(void *)
		{
			Genode::error(__func__, " called - unsupported");
			// drm_i915_gem_sw_finish * const p = reinterpret_cast<drm_i915_gem_sw_finish*>(arg);
			// Handle const handle              = p->handle;
			return 0;
		}

		int _device_gem_execbuffer2(void *arg)
		{
			auto const * const p = reinterpret_cast<drm_i915_gem_execbuffer2*>(arg);

			/* batch-buffer index and cap */
			unsigned const bb_id = (p->flags & I915_EXEC_BATCH_FIRST) ? 0 : p->buffer_count - 1;

			uint64_t const ctx_id = p->rsvd1;
			if (verbose_ioctl) {
				Genode::log(__func__,
				            " buffers_ptr: ",        Genode::Hex(p->buffers_ptr),
				            " buffer_count: ",       p->buffer_count,
				            " batch_start_offset: ", Genode::Hex(p->batch_start_offset),
				            " batch_len: ",          p->batch_len,
				            " dr1: ",                Genode::Hex(p->DR1),
				            " dr4: ",                Genode::Hex(p->DR4),
				            " num_cliprects: ",      p->num_cliprects,
				            " cliprects_ptr: ",      Genode::Hex(p->cliprects_ptr),
				            " flags: ",              Genode::Hex(p->flags),
				            " ctx_id: ",             Genode::Hex(ctx_id));
			}

			if (!(p->flags & I915_EXEC_NO_RELOC)) {
				Genode::error("no relocation supported");
				return -1;
			}

			if (verbose_ioctl && p->flags & I915_EXEC_FENCE_ARRAY) {
				Genode::warning("unsupported: Fence array with Sync-objects with "
				                "FENCE_WAIT/SIGNAL");
			}

			auto const obj =
				reinterpret_cast<drm_i915_gem_exec_object2*>(p->buffers_ptr);

			return _context_space.apply<Drm::Context>(Drm::Context::id(ctx_id),
			                                          [&] (Drm::Context &context) {
				return context.exec_buffer(obj, p->buffer_count, bb_id, p->batch_len); });
		}

		int _device_gem_busy(void *arg)
		{
			auto      const p  = reinterpret_cast<drm_i915_gem_busy*>(arg);
			Drm::Buffer_id const id { .value = p->handle };

			try {
				_buffer_space.apply<Buffer>(id, [&](Buffer const &b) {
					p->busy = false;
				});
				return 0;
			} catch (Genode::Id_space<Buffer>::Unknown_id) {
				return -1;
			}
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


		int _device_create_topology(void *arg)
		{
			auto topo = reinterpret_cast<drm_i915_query_topology_info *>(arg);

			Gpu::Info_intel::Topology const &info = _gpu_info.topology;

			size_t slice_length = sizeof(info.slice_mask);
			size_t subslice_length = info.max_slices * info.ss_stride;
			size_t eu_length = info.max_slices * info.max_subslices * info.eu_stride;

			Genode::memset(topo, 0, sizeof(*topo));
			topo->max_slices = info.max_slices;
			topo->max_subslices = info.max_subslices;
			topo->max_eus_per_subslice = info.max_eus_per_subslice;
			topo->subslice_offset = slice_length;
			topo->subslice_stride = info.ss_stride;
			topo->eu_offset = slice_length + subslice_length;
			topo->eu_stride = info.eu_stride;

			Genode::memcpy(topo->data, &info.slice_mask, slice_length);
			Genode::memcpy(topo->data + slice_length, info.subslice_mask,
			               subslice_length);
			Genode::memcpy(topo->data + slice_length + subslice_length,
			               info.eu_mask, eu_length);

			return 0;
		}

		int _device_query(void *arg)
		{
			auto const query = reinterpret_cast<drm_i915_query*>(arg);

			if (query->num_items != 1) {
				if (verbose_ioctl)
					Genode::error("device specific iocall DRM_I915_QUERY for num_items != 1 not supported"
					              " - num_items=", query->num_items);
				return -1;
			}

			auto const item = reinterpret_cast<drm_i915_query_item *>(query->items_ptr);

			if (item->query_id != DRM_I915_QUERY_TOPOLOGY_INFO || _gpu_info.topology.valid == false) {
				if (verbose_ioctl)
					Genode::error("device specific iocall DRM_I915_QUERY not supported for "
					              " - query_id: ", Genode::Hex(item->query_id));
				return -1;
			}

			if (!item->data_ptr) {
				item->length = 1;
				return 0;
			}

			return _device_create_topology((void *)item->data_ptr);
		}

		int _device_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case DRM_I915_GEM_GET_APERTURE:     return _device_gem_get_aperture_size(arg);
			case DRM_I915_GETPARAM:             return _device_getparam(arg);
			case DRM_I915_GEM_CREATE:           return _device_gem_create(arg);
			case DRM_I915_GEM_MMAP:             return _device_gem_mmap(arg);
			case DRM_I915_GEM_MMAP_GTT:         return _device_gem_mmap_gtt(arg);
			case DRM_I915_GEM_SET_DOMAIN:       return _device_gem_set_domain(arg);
			case DRM_I915_GEM_CONTEXT_CREATE:   return _device_gem_context_create(arg);
			case DRM_I915_GEM_CONTEXT_DESTROY:  return _device_gem_context_destroy(arg);
			case DRM_I915_GEM_SET_TILING:       return _device_gem_set_tiling(arg);
			case DRM_I915_GEM_SW_FINISH:        return _device_gem_sw_finish(arg);
			case DRM_I915_GEM_EXECBUFFER2:      return _device_gem_execbuffer2(arg);
			case DRM_I915_GEM_BUSY:             return _device_gem_busy(arg);
			case DRM_I915_GEM_MADVISE:          return _device_gem_madvise(arg);
			case DRM_I915_GEM_WAIT:             return 0;
			case DRM_I915_QUERY:                return _device_query(arg);
			case DRM_I915_GEM_CONTEXT_SETPARAM: return _device_gem_context_set_param(arg);
			case DRM_I915_GEM_CONTEXT_GETPARAM: return _device_gem_context_get_param(arg);
			case DRM_I915_GEM_SET_CACHING:      return 0;
			default:
				if (verbose_ioctl)
					Genode::error("Unhandled device specific ioctl:", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

		int _generic_gem_close(void *arg)
		{
			Genode::Mutex::Guard guard { _drm_mutex };

			auto      const p  = reinterpret_cast<drm_gem_close*>(arg);
			Drm::Buffer_id const id { .value = p->handle };
			return _free_buffer(id);
		}

		int _generic_gem_flink(void *arg)
		{
			auto const p = reinterpret_cast<drm_gem_flink*>(arg);
			p->name = prime_fd;
			return 0;
		}

		int _generic_syncobj_create(void *arg)
		{
			drm_syncobj_create * const p = reinterpret_cast<drm_syncobj_create *>(arg);
			if (p->flags) {
				Genode::error(__func__, " unsupported flags");
				errno = EINVAL;
				return -1;
			}
			auto * const obj = new (&_heap) Sync_obj(_sync_objects);
			p->handle = obj->id.id().value;
			return 0;
		}

		int _generic_syncobj_wait(void *arg)
		{
			auto &p = *reinterpret_cast<drm_syncobj_wait *>(arg);

			if (verbose_ioctl)
				Genode::error(__func__, " ", p.count_handles, " ",
				              Genode::Hex(p.handles),
				              " tiemout_nsec=", p.timeout_nsec,
				              " flags=", p.flags);

			if (p.count_handles > 1) {
				Genode::error(__func__, " count handles > 1 - not supported");
				return -1;
			}

			uint32_t * handles = reinterpret_cast<uint32_t *>(p.handles);
			bool ok = false;

			try {
				Sync_obj::Id const id { .value = handles[0] };
				_sync_objects.apply<Sync_obj>(id, [&](Sync_obj &) {
					ok = true;
				});
			} catch (Sync_obj::Sync::Unknown_id) {
				errno = EINVAL;
				return -1;
			}

			if (ok) {
				return 0;
			} else
				Genode::error("unknown sync object handle ", handles[0]);

			errno = EINVAL;
			return -1;
		}

		int _generic_syncobj_destroy(void *arg)
		{
			auto * const p = reinterpret_cast<drm_syncobj_destroy *>(arg);
			try {
				Sync_obj::Id const id { .value = p->handle };
				_sync_objects.apply<Sync_obj>(id, [&](Sync_obj &obj) {
					Genode::destroy(_heap, &obj);
				});
				return 0;
			} catch (Sync_obj::Sync::Unknown_id) {
				errno = EINVAL;
				return -1;
			}
		}

		int _generic_gem_open(void *arg)
		{
			auto const p = reinterpret_cast<drm_gem_open *>(arg);

			Genode::error("generic ioctl DRM_IOCTL_GEM_OPEN not supported ",
			              p->handle, " name=", Genode::Hex(p->name));

			return -1;
		}

		int _generic_get_cap(void *arg)
		{
			auto const p = reinterpret_cast<drm_get_cap *>(arg);

			if (p->capability == DRM_CAP_PRIME) {
				/* XXX fd == 43 check */
				p->value = DRM_PRIME_CAP_IMPORT;
				return 0;
			}

			Genode::error("generic ioctl DRM_IOCTL_GET_CAP not supported ",
			              p->capability);
			return -1;
		}

		int       const prime_fd     { 44 };
		Drm::Buffer_id  prime_handle { };

		int _generic_prime_fd_to_handle(void *arg)
		{
			auto const p = reinterpret_cast<drm_prime_handle *>(arg);
			if (p->fd != prime_fd) {
				Genode::error("generic ioctl DRM_IOCTL_PRIME_FD_TO_HANDLE not supported ", __builtin_return_address(0), " ", p->fd);
				return -1;
			}
			p->handle = prime_handle.value;
			return 0;
		}

		int _generic_prime_handle_to_fd(void *arg)
		{
			auto const p = reinterpret_cast<drm_prime_handle *>(arg);

			Drm::Buffer_id const id { .value = p->handle };
			try {
				_buffer_space.apply<Buffer>(id, [&](Buffer const &) {

					if (!prime_handle.value)
						prime_handle = id;

					if (prime_handle.value != id.value) {
						if (verbose_ioctl)
							Genode::warning("prime handle changed: ", id.value);
						prime_handle = id;
					}
				});
			 } catch (Genode::Id_space<Buffer>::Unknown_id) {
				return -1;
			}
			p->fd = prime_fd;
			return 0;
		}


		/*
		 * This is used to distinguish between the "i915" and the "xe" kernel
		 * drivers. Genode's driver is "i915" for now.
		 */
		int _generic_version(void *arg)
		{
			auto *version = reinterpret_cast<drm_version_t *>(arg);

			char const *driver = "i915";

			version->name_len = 5;
			if (version->name)
					Genode::copy_cstring(version->name, driver, 5);

			/*
			 * dummy alloc remaining member, since they are de-allocated using 'free'
			 * in xf86drm.c
			 */
			if (!version->date) version->date = (char *)Libc::malloc(1);
			if (!version->desc) version->desc = (char *)Libc::malloc(1);

			return 0;
		}

		int _generic_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case DRM_NUMBER(DRM_IOCTL_GEM_CLOSE):       return _generic_gem_close(arg);
			case DRM_NUMBER(DRM_IOCTL_GEM_FLINK):       return _generic_gem_flink(arg);
			case DRM_NUMBER(DRM_IOCTL_GEM_OPEN):        return _generic_gem_open(arg);
			case DRM_NUMBER(DRM_IOCTL_GET_CAP):         return _generic_get_cap(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_CREATE):  return _generic_syncobj_create(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_DESTROY): return _generic_syncobj_destroy(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_WAIT):    return _generic_syncobj_wait(arg);
			case DRM_NUMBER(DRM_IOCTL_VERSION):         return _generic_version(arg);
			case DRM_NUMBER(DRM_IOCTL_PRIME_FD_TO_HANDLE):
				return _generic_prime_fd_to_handle(arg);
			case DRM_NUMBER(DRM_IOCTL_PRIME_HANDLE_TO_FD):
				return _generic_prime_handle_to_fd(arg);
			default:
				Genode::error("Unhandled generic DRM ioctl:", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

	public:

		Call()
		{
			/* make handle id 0 unavailable, handled as invalid by iris */
			drm_syncobj_create reserve_id_0 { };
			if (_generic_syncobj_create(&reserve_id_0))
				Genode::warning("syncobject 0 not reserved");
		}

		~Call()
		{
			while(_buffer_space.apply_any<Buffer>([&] (Buffer &buffer) {
				_free_buffer(buffer.id());
			}));

			while (_context_space.apply_any<Drm::Context>([&] (Drm::Context &context) {
				Libc::close(context.fd());
				destroy(_heap, &context);
			}));

			while (_sync_objects.apply_any<Sync_obj>([&] (Sync_obj &obj) {
				destroy(_heap, &obj); }));
		}

		int lseek(int fd, off_t offset, int whence)
		{
			if (fd != prime_fd || offset || whence != SEEK_END)
				return -1;

			int size = -1;
			_buffer_space.apply<Buffer>(prime_handle, [&](Buffer &b) {
				size =(int)b.vram().size;
			});

			return size;
		}

		void unmap_buffer(void *addr, size_t length)
		{
			Genode::Mutex::Guard guard { _drm_mutex };

			bool found = false;

			_buffer_space.for_each<Buffer>([&] (Buffer &b) {
				if (found)
					return;

				if (reinterpret_cast<void *>(b.mmap_addr()) != addr)
					return;

				if (b.vram().size != length) {
					Genode::warning(__func__, " size mismatch");
					Genode::sleep_forever();
					return;
				}

				b.unmap();
				found = true;
			});

			if (!found) {
				Genode::warning(__func__, " unknown region ",
				                addr, "+", Genode::Hex(length));
				Genode::sleep_forever();
			}
		}

		void unmap_buffer_ppgtt(__u32 handle)
		{
			Genode::Mutex::Guard guard { _drm_mutex };

			Drm::Buffer_id const id = { .value = handle };
			_context_space.for_each<Drm::Context>([&](Drm::Context &context) {
				context.unmap_buffer_gpu(id);
			});
		}

		int ioctl(unsigned long request, void *arg)
		{
			bool const device = device_ioctl(request);
			return device ? _device_ioctl(device_number(request), arg)
			              : _generic_ioctl(command_number(request), arg);
		}

		/*
		 * Mesa 24+ way to retrieve device information (incomplete, expand as
		 * needed). Before it was done via '_device_getparam'
		 */
		int drm_pci_device(drmDevicePtr device)
		{
			device->deviceinfo.pci->device_id   = _gpu_info.chip_id;
			device->deviceinfo.pci->revision_id = _gpu_info.revision.value;

			return 0;
		}
};


static Genode::Constructible<Drm::Call> _call;


void drm_init()
{
	/* make sure VFS is initialized */
	struct Libc::stat buf;
	if (stat("/dev/gpu", &buf) < 0) {
		Genode::error("'/dev/gpu' not accessible: ",
				          "try configure '<gpu>' in 'dev' directory of VFS'");
		return;
	}

	_call.construct();
}


/**
 * Mmap buffer object
 *
 * On Genode the virtual address of MMAP_GTT is stored in the offset.
 */
extern "C" void *drm_mmap(void * /* vaddr */, size_t length,
                          int /* prot */, int /* flags */,
                          int /* fd */, off_t offset)
{
	Genode::error(__func__, " called not implemented");
	return nullptr;
}

/**
 * Unmap buffer object
 */
extern "C" int drm_munmap(void *addr, size_t length)
{
	if (_call.constructed() == false) { errno = EIO; return -1; }

	_call->unmap_buffer(addr, length);
	return 0;
}


extern "C" void drm_unmap_ppgtt(__u32 handle)
{
	_call->unmap_buffer_ppgtt(handle);
}


extern "C" int drm_lseek(int fd, off_t offset, int whence)
{
	if (_call.constructed() == false) { errno = EIO; return -1; }

	return _call->lseek(fd, offset, whence);
}


extern "C" int genode_ioctl(int /* fd */, unsigned long request, void *arg)
{
	if (_call.constructed() == false) { errno = EIO; return -1; }
	if (verbose_ioctl) { dump_ioctl(request); }

	int const ret = _call->ioctl(request, arg);
	if (verbose_ioctl) { Genode::log("returned ", ret, " from ", __builtin_return_address(0)); }
	return ret;
}


extern "C"  int genode_drmGetPciDevice(int fd, uint32_t flags, drmDevicePtr device)
{
	if (_call.constructed() == false) { errno = EIO; return -1; }

	/* TODO create constant */
	if (fd != 43) {
		Genode::error(__func__, " fd is not Genode Iris (43)");
		return -ENODEV;
	}

	return _call->drm_pci_device(device);
}
