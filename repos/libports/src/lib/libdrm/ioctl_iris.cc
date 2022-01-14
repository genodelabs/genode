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
#include <util/retry.h>

#include <vfs_gpu.h>

extern "C" {
#include <errno.h>
#include <drm.h>
#include <i915_drm.h>

#define DRM_NUMBER(req) ((req) & 0xff)
}

namespace Libc {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}

using Genode::addr_t;
using Genode::Attached_dataspace;
using Genode::Constructible;

namespace Drm
{
	struct Context;
}

enum { verbose_ioctl = false };

namespace Utils
{
	uint64_t limit_to_48bit(uint64_t addr) {
		return addr & ((1ULL << 48) - 1); }
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

	struct Gpu_virtual_address {
		uint64_t addr;
	};

} /* anonymous namespace */


struct Gpu::Buffer
{
	Gpu::Connection &_gpu;

	Genode::Id_space<Gpu::Buffer>::Element const _elem;

	Genode::Dataspace_capability           const cap;
	Genode::size_t                         const size;

	Constructible<Attached_dataspace> buffer_attached { };

	Genode::Dataspace_capability map_cap    { };
	Offset                       map_offset { 0 };

	struct Tiling
	{
		bool     _valid;
		uint32_t mode;
		uint32_t stride;
		uint32_t swizzle;

		Tiling(uint32_t mode, uint32_t stride, uint32_t swizzle)
		:
			_valid  { true },
			mode    { mode },
			stride  { stride },
			swizzle { swizzle }
		{ }

		Tiling()
		:
			_valid { false }, mode { 0 }, stride { 0 }, swizzle { 0 }
		{ }

		bool valid() const { return _valid; }
	};

	Tiling tiling { };

	Gpu_virtual_address  gpu_vaddr { };
	Gpu::Sequence_number seqno { };

	bool                         gpu_vaddr_valid { false };
	bool                         busy            { false };

	Buffer(Gpu::Connection &gpu,
	       Genode::size_t   size,
	       Genode::Id_space<Buffer> &space)
	:
		_gpu   { gpu },
		_elem  { *this, space },
		cap    { _gpu.alloc_buffer(_elem.id(), size) },
		size   { Genode::Dataspace_client(cap).size() }
	{ }

	virtual ~Buffer()
	{
		_gpu.free_buffer(_elem.id());
	}

	bool mmap(Genode::Env &env)
	{
		if (!buffer_attached.constructed())
			buffer_attached.construct(env.rm(), cap);

		return buffer_attached.constructed();
	}

	addr_t mmap_addr() {
		return reinterpret_cast<addr_t>(buffer_attached->local_addr<addr_t>());
	}

	Gpu::Buffer_id id() const
	{
		return _elem.id();
	}
};


struct Drm::Context
{
	struct Buffer
	{
		using Id_space = Genode::Id_space<Buffer>;
		Id_space::Element const elem;

		Gpu_virtual_address  gpu_vaddr { };
		Gpu::Sequence_number seqno { };
		bool                 gpu_vaddr_valid { false };
		bool                 busy            { false };

		Buffer(Id_space &space, Gpu::Buffer_id id)
		: elem(*this, space, Id_space::Id { .value = id.value })
		{ }

		Gpu::Buffer_id buffer_id() const
		{
			return Gpu::Buffer_id { .value = elem.id().value };
		}

		Id_space::Id id() const
		{
			return elem.id();
		}
	};

	Gpu::Connection   &_gpu;
	Gpu::Connection   &_gpu_master;
	Genode::Allocator &_alloc;

	Gpu::Info_intel  const &_gpu_info {
		*_gpu.attached_info<Gpu::Info_intel>() };

	int _fd;

	using Id_space = Genode::Id_space<Context>;
	Id_space::Element const _elem;

	Genode::Id_space<Buffer> _buffer_space { };

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

	void _map_buffer_ppgtt(Buffer &buffer, Gpu_virtual_address vaddr)
	{
		Genode::retry<Gpu::Session::Out_of_ram>(
		[&]() {
			Genode::retry<Gpu::Session::Out_of_caps>(
			[&] () {
				_gpu.map_buffer_ppgtt(buffer.buffer_id(), Utils::limit_to_48bit(vaddr.addr));
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

	void _unmap_buffer_ppgtt(Buffer &buffer)
	{
		if (!buffer.gpu_vaddr_valid)
			return;

		_gpu.unmap_buffer_ppgtt(buffer.buffer_id(), buffer.gpu_vaddr.addr);
		buffer.gpu_vaddr_valid = false;
	}

	Context(Gpu::Connection &gpu, Gpu::Connection &gpu_master,
	        Genode::Allocator &alloc, int fd,
	        Genode::Id_space<Context> &space)
	:
	  _gpu(gpu), _gpu_master(gpu_master), _alloc(alloc), _fd(fd),
	  _elem (*this, space) { }

	unsigned long id() const
	{
		return _elem.id().value + 1;
	}

	static Id_space::Id id(unsigned long value)
	{
		return Id_space::Id { .value = value - 1 };
	}

	int fd() const { return _fd; }

	void import_buffer(Gpu::Buffer_capability buffer_cap,
	                   Gpu::Buffer_id id)
	{
		Genode::retry<Gpu::Session::Out_of_ram>(
		[&]() {
			Genode::retry<Gpu::Session::Out_of_caps>(
			[&] () {
				_gpu.import_buffer(buffer_cap, id);
				new (_alloc) Buffer(_buffer_space, id);
			},
			[&] () {
				_gpu.upgrade_caps(2);
			});
		},
		[&] () {
			_gpu.upgrade_ram(1024*1024);
		});
	}

	void free_buffer(Gpu::Buffer_id id)
	{
		try {
			_buffer_space.apply<Buffer>(Buffer::Id_space::Id { .value = id.value },
			                            [&] (Buffer &buffer) {
				destroy(_alloc, &buffer);
			});
		} catch (Buffer::Id_space::Unknown_id) { return; }

		_gpu.free_buffer(id);
	}

	void free_buffers()
	{
		while (_buffer_space.apply_any<Buffer>([&] (Buffer &buffer) {
			Gpu::Buffer_id id = buffer.buffer_id();
			destroy(_alloc, &buffer);
			_gpu.free_buffer(id);
		})) { }
	}

	void unmap_buffer_ppgtt(Gpu::Buffer_id id)
	{
		try {
			_buffer_space.apply<Buffer>(Buffer::Id_space::Id { .value = id.value },
			                            [&] (Buffer &buffer) {
				_unmap_buffer_ppgtt(buffer);
			});
		} catch (Buffer::Id_space::Unknown_id) { }
	}

	int exec_buffer(drm_i915_gem_exec_object2 *obj, uint64_t count,
	                uint64_t batch_id, size_t batch_length)
	{
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

					if (b.gpu_vaddr_valid && b.gpu_vaddr.addr != obj[i].offset) {
						_unmap_buffer_ppgtt(b);
					}

					if (!b.gpu_vaddr_valid)
						_map_buffer_ppgtt(b, Gpu_virtual_address { .addr = obj[i].offset });

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
					Gpu::Buffer_id buffer_id { .value = id.value };
					Gpu::Buffer_capability buffer_cap = _gpu_master.export_buffer(buffer_id);
					import_buffer(buffer_cap, buffer_id);
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

		command_buffer->seqno = _gpu.exec_buffer(command_buffer->buffer_id(),
		                                         batch_length);

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


class Drm_call
{
	private:

		using Cap_quota = Genode::Cap_quota;
		using Ram_quota = Genode::Ram_quota;

		Genode::Env      &_env { *vfs_gpu_env() };
		Genode::Heap      _heap { _env.ram(), _env.rm() };
		Gpu::Connection   _gpu_session { _env };

		Gpu::Info_intel  const &_gpu_info {
			*_gpu_session.attached_info<Gpu::Info_intel>() };
		size_t            _available_gtt_size { _gpu_info.aperture_size };

		using Buffer       = Gpu::Buffer;
		using Buffer_space = Genode::Id_space<Buffer>;
		Buffer_space _buffer_space { };

		using Context_id    = Genode::Id_space<Drm::Context>::Id;
		using Context_space = Genode::Id_space<Drm::Context>;
		Context_space _context_space { };

		struct Resource_guard
		{
			struct Upgrade_failed     : Genode::Exception { };
			struct Invalid_accounting : Genode::Exception { };

			Gpu::Connection &_gpu;

			Cap_quota cap_donated;
			Ram_quota ram_donated;

			Cap_quota cap_used;
			Ram_quota ram_used;

			Genode::int64_t cap_count { 0 };
			Genode::int64_t ram_count { 0 };
			Genode::int64_t map_count { 0 };
			Genode::int64_t map_ppgtt_count { 0 };
			Genode::int64_t alloc_count { 0 };

			bool _available(Cap_quota needed) const {
				return cap_donated.value - cap_used.value >= needed.value; }

			bool _available(Ram_quota needed) const {
				return ram_donated.value - ram_used.value >= needed.value; }

			void _upgrade(Cap_quota caps)
			{
				_gpu.upgrade_caps(caps.value);
				cap_donated.value += caps.value;
			}

			void _upgrade(Ram_quota ram)
			{
				_gpu.upgrade_ram(ram.value);
				ram_donated.value += ram.value;
			}

			void _withdraw(Cap_quota caps)
			{
				if (caps.value == 0)
					return;

				Cap_quota const avail { cap_donated.value - cap_used.value };
				if (caps.value > avail.value) {
					Genode::error(__func__, ": assert CAP amount failed, ",
					              "caps: ", caps.value, " avail: ", avail,
					              " count: ", cap_count);
					throw Invalid_accounting();
				}
				cap_used.value += caps.value;
				cap_count++;
			}

			void _withdraw(Ram_quota ram)
			{
				if (ram.value == 0)
					return;

				Ram_quota const avail { ram_donated.value - ram_used.value };
				if (ram.value > avail.value) {
					Genode::error(__func__, ": assert CAP amount failed, ",
					              "ram: ", ram.value, " avail: ", avail,
					              " count: ", ram_count);
					throw Invalid_accounting();
				}
				ram_used.value += ram.value;
				ram_count++;
			}

			void _replenish(Cap_quota caps)
			{
				if (caps.value == 0)
					return;

				if (cap_used.value < caps.value) {
					Genode::error(__func__, ": assert CAP amount failed, ",
					              "used: ", cap_used.value, " caps: ",
					              caps.value, " count: ", cap_count);
					throw Invalid_accounting();
				}
				cap_used.value -= caps.value;

				cap_count--;
			}

			void _replenish(Ram_quota ram)
			{
				if (ram.value == 0)
					return;

				if (ram_used.value < ram.value) {
					Genode::error(__func__, ": assert RAM amount failed, ",
					              "used: ", ram_used.value, " ram: ", ram.value,
					              " count: ", ram_count);
					throw Invalid_accounting();
				}
				ram_used.value -= ram.value;

				ram_count--;
			}

			enum {
				ALLOC_BUFFER_CAP_AMOUNT     = 4,
				MAP_BUFFER_CAP_AMOUNT       = 2,
				MAP_BUFFER_RAM_AMOUNT       = 1024,
			};

			Resource_guard(Gpu::Connection &gpu)
			:
				_gpu { gpu },

				cap_donated { 0 },
				ram_donated { 0 },
				cap_used    { 0 },
				ram_used    { 0 }
			{ }

			template <typename FN> void _perform_gpu_op(Cap_quota caps, Ram_quota ram,
			                                            FN const &fn)
			{
				Cap_quota donated_caps { caps.value };
				if (!_available(caps)) {
					_upgrade(caps);
					caps.value /= 2;
				}

				Ram_quota donated_ram { ram.value };
				if (!_available(ram)) {
					_upgrade(ram);
					ram.value /= 2;
				}

				Genode::retry<Gpu::Session::Out_of_ram>(
				[&] () {
					Genode::retry<Gpu::Session::Out_of_caps>(
					[&] () {
						fn();
					},
					[&] () {
						if (caps.value == 0)
							throw Upgrade_failed();

						_upgrade(caps);
						donated_caps.value += caps.value;
						caps.value /= 2;
					});
				},
				[&] () {
					if (ram.value == 0)
						throw Upgrade_failed();

					_upgrade(ram);
					donated_ram.value += ram.value;
					ram.value /= 2;
				});

				_withdraw(donated_caps);
				_withdraw(donated_ram);
			}

			Buffer *alloc_buffer(Genode::Allocator &alloc, Buffer_space &buffer_space,
			                     Genode::size_t size)
			{
				/* round to next page size */
				size = ((size + 0xffful) & ~0xffful);

				Cap_quota caps { ALLOC_BUFFER_CAP_AMOUNT };
				Ram_quota ram { size };

				Buffer *buffer = nullptr;
				try {
					_perform_gpu_op(caps, ram, [&] () {
						buffer = new (alloc) Buffer(_gpu, size, buffer_space);
					});
				} catch (Upgrade_failed) {
					return nullptr;
				}
				alloc_count++;

				return buffer;
			}

			void free_buffer(Genode::size_t size)
			{
				alloc_count--;
				Cap_quota const caps { ALLOC_BUFFER_CAP_AMOUNT };
				_replenish(caps);
				_replenish(Ram_quota { size });
			}


			bool map_buffer(Buffer &buffer)
			{
				Cap_quota caps { MAP_BUFFER_CAP_AMOUNT };
				Ram_quota ram { MAP_BUFFER_RAM_AMOUNT };

				try {
					_perform_gpu_op(caps, ram, [&] () {
						buffer.map_cap =
							_gpu.map_buffer(buffer.id(), true,
							                Gpu::Mapping_attributes::rw());
					});
				} catch (Upgrade_failed) {
					return false;
				}

				return true;
			}

			void unmap_buffer(Buffer &buffer)
			{
				map_count--;
				Cap_quota const caps { MAP_BUFFER_CAP_AMOUNT };
				_replenish(caps);

				Ram_quota const ram { MAP_BUFFER_RAM_AMOUNT };
				_replenish(ram);

				_gpu.unmap_buffer(buffer.id());
			}

			void dump()
			{
				Genode::error("Resource_guard: ",
				              "caps: ", cap_used.value, "/", cap_donated.value, " "
				              "ram: ", ram_used.value, "/", ram_donated.value, " "
				              "counter: ", alloc_count, "/", map_count, "/", map_ppgtt_count);
			}
		};

		/*
		 * The initial connection quota is needed for bringup,
		 * start from 0 for all other allocations.
		 */
		Resource_guard _resources { _gpu_session };

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

		Offset _map_buffer(Buffer &b)
		{
			Offset offset = 0;

			if (b.map_cap.valid()) {
				offset = b.map_offset;
				return offset;
			}

			if (_resources.map_buffer(b)) {

				// XXX attach might faile
				b.map_offset = static_cast<Offset>(_env.rm().attach(b.map_cap));
				offset       = b.map_offset;

				_available_gtt_size -= b.size;
			} else {

				if (b.map_cap.valid()) {
					_unmap_buffer(b);

					Genode::error("could not attach GEM buffer handle: ", b.id().value);
					Genode::sleep_forever();
				}
			}

			return offset;
		}

		Offset _map_buffer(Gpu::Buffer_id const id)
		{
			Offset offset = 0;
			try {
				_buffer_space.apply<Buffer>(id, [&] (Buffer &b) {
					offset = _map_buffer(b);
				});
			} catch (Genode::Id_space<Buffer>::Unknown_id) {
				Genode::error(__func__, ": invalid handle ", id.value);
				Genode::sleep_forever();
			}
			return offset;
		}

		void _unmap_buffer(Buffer &h)
		{
			if (!h.map_cap.valid())
				return;

			_env.rm().detach(h.map_offset);
			h.map_offset = 0;

			_resources.unmap_buffer(h);

			h.map_cap = Genode::Dataspace_capability();

			_available_gtt_size += h.size;
		}

		template <typename FUNC>
		void _alloc_buffer(uint64_t const size, FUNC const &fn)
		{
			Buffer *buffer = _resources.alloc_buffer(_heap, _buffer_space, size);

			if (buffer)
				fn(*buffer);
		}

		int _free_buffer(Gpu::Buffer_id const id)
		{
			try {
				_buffer_space.apply<Buffer>(id, [&] (Buffer &b) {

					/* callee checks for mappings */
					_unmap_buffer(b);

					_resources.free_buffer(b.size);

					_context_space.for_each<Drm::Context>([&] (Drm::Context &context) {
						context.free_buffer(b.id()); });
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
			auto      const p      = reinterpret_cast<drm_i915_gem_mmap *>(arg);
			Gpu::Buffer_id const id { .value = p->handle };

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
			auto      const p = reinterpret_cast<drm_i915_gem_mmap_gtt *>(arg);
			Gpu::Buffer_id const id { .value = p->handle };

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", id.value,
				              " offset: ", Genode::Hex(p->offset));
			}

			bool successful = true;
			try {
				_buffer_space.apply<Buffer>(id, [&] (Buffer &b) {

					p->offset = _map_buffer(b);
					if (p->offset == 0) {
						successful = false;
						return;
					}

					/*
					 * Judging by iris mode == 0 is I915_TILING_NONE for
					 * which no fencing seems to be necessary.
					 */
					if (b.tiling.valid() && b.tiling.mode != 0) {
						uint32_t const m = (b.tiling.stride << 16) | (b.tiling.mode == 1 ? 1 : 0);
						successful = _gpu_session.set_tiling(b.id(), m);
					} else {
						successful = true;
					}

					if (!successful) {
						_unmap_buffer(b);
						return;
					}

				});
			} catch (Genode::Id_space<Buffer>::Unknown_id) { }

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", id.value,
				              " offset: ", Genode::Hex(p->offset),
				              successful ? " (mapped)" : " (failed)");
			}

			return successful ? 0 : -1;
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
			Gpu::Buffer_id const id { .value = p->handle };
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
				if (verbose_ioctl)
					Genode::error("I915_PARAM_CS_TIMESTAMP_FREQUENCY not supported");
				return -1;
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
			                                                 fd, _context_space);

			p->ctx_id = context->id();

			return 0;
		}

		int _device_gem_context_destroy(void *arg)
		{
			unsigned long ctx_id = reinterpret_cast<drm_i915_gem_context_destroy *>(arg)->ctx_id;
			Context_id const id  = Drm::Context::id(ctx_id);

			try {
				_context_space.apply<Drm::Context>(id, [&] (Drm::Context &context) {
					context.free_buffers();
					Libc::close(context.fd());
					destroy(_heap, &context);
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
			default:
				Genode::error(__func__, " ctx=", p->ctx_id, " param=", p->param, " size=", p->size, " value=", Genode::Hex(p->value));
				return -1;
			}
		}

		int _device_gem_set_tiling(void *arg)
		{
			auto      const p  = reinterpret_cast<drm_i915_gem_set_tiling*>(arg);
			Gpu::Buffer_id const id { .value = p->handle };
			uint32_t  const mode    = p->tiling_mode;
			uint32_t  const stride  = p->stride;
			uint32_t  const swizzle = p->swizzle_mode;

			if (verbose_ioctl) {
				Genode::error(__func__, ": ",
				              "handle: ", id.value, " "
				              "mode: ", mode, " "
				              "stride: ", stride , " "
				              "swizzle: ", swizzle);
			}

			bool ok = false;
			try {
				_buffer_space.apply<Buffer>(id, [&] (Buffer &b) {

					b.tiling = Gpu::Buffer::Tiling(mode, stride, swizzle);
					ok = true;

				});
			} catch (Genode::Id_space<Buffer>::Unknown_id) {
				Genode::error(__func__, ": invalid handle: ", id.value);
			}

			return ok ? 0 : -1;
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

			if (p->flags & I915_EXEC_FENCE_ARRAY) {
				bool unsupported = false;

				for (unsigned i = 0; i < p->num_cliprects; i++) {
					auto &fence = reinterpret_cast<drm_i915_gem_exec_fence *>(p->cliprects_ptr)[i];

					Sync_obj::Id const id { .value = fence.handle };
					_sync_objects.apply<Sync_obj>(id, [&](Sync_obj &) {
						/**
						 * skipping signal fences should be save as long as
						 * no one tries to wait for ...
						 * - fence.flags & I915_EXEC_FENCE_SIGNAL
						 */
						if (fence.flags & I915_EXEC_FENCE_WAIT)
							unsupported = true;
					});
				}

				if (unsupported) {
					Genode::error("fence wait not supported");
					return -1;
				}
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
			Gpu::Buffer_id const id { .value = p->handle };

			try {
				_buffer_space.apply<Buffer>(id, [&](Buffer const &b) {
					p->busy = b.busy;
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

		int _device_query(void *arg)
		{
			auto const query = reinterpret_cast<drm_i915_query*>(arg);

			if (verbose_ioctl)
				Genode::error("device specific iocall DRM_I915_QUERY not supported"
				              " - num_items=", query->num_items);

			return -1;
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
			default:
				if (verbose_ioctl)
					Genode::error("Unhandled device specific ioctl:", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

		int _generic_gem_close(void *arg)
		{
			auto      const p  = reinterpret_cast<drm_gem_close*>(arg);
			Gpu::Buffer_id const id { .value = p->handle };
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
				errno = 62 /* ETIME */;
				return -1;
			} else
				Genode::error("unknown sync object handle ", handles[0]);

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
		Gpu::Buffer_id  prime_handle { };

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

			Gpu::Buffer_id const id { .value = p->handle };
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

		int _generic_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case DRM_NUMBER(DRM_IOCTL_GEM_CLOSE): return _generic_gem_close(arg);
			case DRM_NUMBER(DRM_IOCTL_GEM_FLINK): return _generic_gem_flink(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_CREATE): return _generic_syncobj_create(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_WAIT): return _generic_syncobj_wait(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_DESTROY): return _generic_syncobj_destroy(arg);
			case DRM_NUMBER(DRM_IOCTL_GEM_OPEN): return _generic_gem_open(arg);
			case DRM_NUMBER(DRM_IOCTL_GET_CAP): return _generic_get_cap(arg);
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

		Drm_call()
		{
			/* make handle id 0 unavailable, handled as invalid by iris */
			drm_syncobj_create reserve_id_0 { };
			if (_generic_syncobj_create(&reserve_id_0))
				Genode::warning("syncobject 0 not reserved");
		}

		int lseek(int fd, off_t offset, int whence)
		{
			if (fd != prime_fd || offset || whence != SEEK_END)
				return -1;

			int size = -1;
			_buffer_space.apply<Buffer>(prime_handle, [&](Buffer const &b) {
				size =(int)b.size;
			});

			return size;
		}

		bool map_buffer_ggtt(Offset offset, size_t length)
		{
			bool result = false;

			_buffer_space.for_each<Buffer>([&] (Buffer &h) {
				if (h.map_offset != offset) { return; }
				if (length > h.size) { Genode::error("map_buffer_ggtt: size mismatch"); return; }
				result = true;
			});

			if (!result)
				Genode::error("could not lookup buffer for offset: ", offset);

			return result;
		}

		void unmap_buffer(void *addr, size_t length)
		{
			bool found = false;

			_buffer_space.for_each<Buffer>([&] (Buffer &b) {
				if (found || !b.buffer_attached.constructed())
					return;

				if (reinterpret_cast<void *>(b.mmap_addr()) != addr)
					return;

				if (b.buffer_attached->size() != length) {
					Genode::warning(__func__, " size mismatch");
					Genode::sleep_forever();
					return;
				}

				if (b.tiling.valid())
					b.tiling = Gpu::Buffer::Tiling();

				if (b.map_cap.valid())
					_unmap_buffer(b);

				b.buffer_attached.destruct();
				found = true;
			});

			if (!found) {
				Genode::warning(__func__, " unknown region ",
				                addr, "+", Genode::Hex(length));
				Genode::sleep_forever();
			}
		}

		void unmap_buffer_ggtt(void *addr, size_t length)
		{
			Offset const offset = Offset(addr);

			bool handled = false;

			_buffer_space.for_each<Buffer>([&] (Buffer &h) {
				if (handled) return;
				if (h.map_offset != offset) return;
				if (length > h.size) { Genode::error("unmap_buffer_ggtt: size mismatch"); return; }

				if (!h.map_cap.valid()) {
					Genode::error("no valid capability found for offset: ", Genode::Hex(offset));
					return;
				}

				_unmap_buffer(h);
				handled = true;
			});

			if (!handled) {
				Genode::error(__func__, ": unknown addr ", addr, "+", Genode::Hex(length));
				Genode::sleep_forever();
			}
		}

		void unmap_buffer_ppgtt(__u32 handle)
		{
			Gpu::Buffer_id const id = { .value = handle };
			_context_space.for_each<Drm::Context>([&](Drm::Context &context) {
				context.unmap_buffer_ppgtt(id);
			});
		}

		int ioctl(unsigned long request, void *arg)
		{
			bool const device = device_ioctl(request);
			return device ? _device_ioctl(device_number(request), arg)
			              : _generic_ioctl(command_number(request), arg);
		}
};


static Genode::Constructible<Drm_call> _call;


void drm_init()
{
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
	if (_call.constructed() == false) { errno = EIO; return nullptr; }

	/* sanity check if we got a GTT mapped offset */
	bool const ok = _call->map_buffer_ggtt(offset, length);
	return ok ? (void *)offset : nullptr;
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
	if (verbose_ioctl) { Genode::log("returned ", ret); }
	return ret;
}
