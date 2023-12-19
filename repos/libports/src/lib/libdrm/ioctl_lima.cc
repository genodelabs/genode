/*
 * \brief  DRM ioctl backend
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <gpu_session/connection.h>
#include <gpu/info_lima.h>
#include <util/string.h>

#include <vfs_gpu.h>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>

#include <drm.h>
#include <drm-uapi/lima_drm.h>
#include <libdrm_macros.h>
}


static constexpr bool verbose_ioctl = false;


/**
 * Get DRM command number
 */
static unsigned long constexpr command_number(unsigned long request)
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
	long const cmd = command_number(request);
	return cmd >= DRM_COMMAND_BASE && cmd < DRM_COMMAND_END;
}


/**
 * Return name of DRM command
 */
const char *command_name(unsigned long request)
{
	if (IOCGROUP(request) != DRM_IOCTL_BASE)
		return "<non-DRM>";

	if (!device_ioctl(request)) {
		switch (command_number(request)) {
		case command_number(DRM_IOCTL_GEM_CLOSE):            return "DRM_IOCTL_GEM_CLOSE";
		case command_number(DRM_IOCTL_GEM_FLINK):            return "DRM_IOCTL_GEM_FLINK";
		case command_number(DRM_IOCTL_GEM_OPEN):             return "DRM_IOCTL_GEM_OPEN";
		case command_number(DRM_IOCTL_GET_CAP):              return "DRM_IOCTL_GET_CAP";
		case command_number(DRM_IOCTL_GET_UNIQUE):           return "DRM_IOCTL_GET_UNIQUE";
		case command_number(DRM_IOCTL_PRIME_FD_TO_HANDLE):   return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
		case command_number(DRM_IOCTL_PRIME_HANDLE_TO_FD):   return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
		case command_number(DRM_IOCTL_SYNCOBJ_CREATE):       return "DRM_IOCTL_SYNCOBJ_CREATE";
		case command_number(DRM_IOCTL_SYNCOBJ_DESTROY):      return "DRM_IOCTL_SYNCOBJ_DESTROY";
		case command_number(DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD): return "DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD";
		case command_number(DRM_IOCTL_VERSION):              return "DRM_IOCTL_VERSION";
		default:                                             return "<unknown drm>";
		}
	}

	switch (device_number(request)) {
	case DRM_LIMA_CTX_CREATE: return "DRM_LIMA_CTX_CREATE";
	case DRM_LIMA_CTX_FREE:   return "DRM_LIMA_CTX_FREE";
	case DRM_LIMA_GET_PARAM:  return "DRM_LIMA_GET_PARAM";
	case DRM_LIMA_GEM_CREATE: return "DRM_LIMA_GEM_CREATE";
	case DRM_LIMA_GEM_INFO:   return "DRM_LIMA_GEM_INFO";
	case DRM_LIMA_GEM_SUBMIT: return "DRM_LIMA_GEM_SUBMIT";
	case DRM_LIMA_GEM_WAIT:   return "DRM_LIMA_GEM_WAIT";
	default:                  return "<unknown driver>";
	}
}


namespace Lima {

	size_t get_payload_size(drm_lima_gem_submit const &submit);

	// XXX better implement as 'size_t for_each_object(T const *t, unsigned len, FN const &fn, char *dst)'
	template <typename FN, typename T> void for_each_object(T const *t, unsigned len, FN const &fn)
	{
		for (unsigned i = 0; i < len; i++) {
			T const *obj = &t[i];
			fn(obj);
		}
	}

	void serialize(drm_lima_gem_submit *submit, char *content);

	template <typename FN>
	void for_each_submit_bo(drm_lima_gem_submit const &submit, FN const &fn)
	{
		auto access_bo = [&] (drm_lima_gem_submit_bo const *bo) {
			fn(*bo);
		};

		for_each_object((drm_lima_gem_submit_bo*)submit.bos,
		                submit.nr_bos, access_bo);
	}

	size_t get_payload_size(drm_version const &version);

} /* anonymous namespace */


size_t Lima::get_payload_size(drm_lima_gem_submit const &submit)
{
	size_t size = 0;

	size += sizeof (drm_lima_gem_submit_bo) * submit.nr_bos;
	size += submit.frame_size;

	return size;
}


void Lima::serialize(drm_lima_gem_submit *submit, char *content)
{
	size_t offset = 0;

	/* leave place for object itself first */
	offset += sizeof (*submit);

	/* next are the buffer-objects */
	if (submit->nr_bos) {
		size_t const new_start = offset;

		auto copy_bos = [&] (drm_lima_gem_submit_bo const *bo) {
			char * const dst = content + offset;
			Genode::memcpy(dst, bo, sizeof (*bo));
			offset += sizeof (*bo);
		};
		for_each_object((drm_lima_gem_submit_bo*)submit->bos,
		                submit->nr_bos, copy_bos);
		submit->bos = reinterpret_cast<__u64>(new_start);
	}

	/* next is the frame */
	{
		size_t const new_start = offset;

		char * const dst = content + offset;
		Genode::memcpy(dst, reinterpret_cast<void const*>(submit->frame), submit->frame_size);
		offset += submit->frame_size;
		submit->frame = reinterpret_cast<__u64>(new_start);
	}

	/* copy submit object last but into the front */
	Genode::memcpy(content, submit, sizeof (*submit));
}


size_t Lima::get_payload_size(drm_version const &version)
{
	size_t size = 0;
	size += version.name_len;
	size += version.date_len;
	size += version.desc_len;
	return size;
}


namespace Lima {
	using namespace Genode;
	using namespace Gpu;

	struct Call;
} /* namespace Lima */


/*
 * Gpu::Vram encapsulates a buffer object allocation
 */
struct Gpu::Vram
{
	struct Allocation_failed : Genode::Exception { };

	Gpu::Connection &_gpu;

	Vram_id_space::Element const _elem;

	size_t const size;
	Gpu::Virtual_address va;
	Genode::Dataspace_capability cap { };

	Genode::Constructible<Genode::Attached_dataspace> _attached_buffer { };

	Vram(Gpu::Connection      &gpu,
	     size_t                size,
	     Gpu::Virtual_address  va,
	     Vram_id_space        &space)
	:
		_gpu  { gpu },
		_elem { *this, space },
		 size { size },
		 va   { va }
	{
		/*
		 * As we cannot easily enforce an GPU virtual-address after the
		 * fact when using the DRM Lima API we instead use 'map_gpu' to
		 * create the buffer object.
		*
		 * A valid virtual-address needs to be enforced before attempting
		 * the allocation.
		 */
		if (!_gpu.map_gpu(_elem.id(), size, 0, va))
			throw Allocation_failed();

		cap = _gpu.map_cpu(_elem.id(), Gpu::Mapping_attributes());
	}

	~Vram()
	{
		_gpu.unmap_gpu(_elem.id(), 0, Virtual_address());
	}

	bool mmap(Genode::Env &env)
	{
		if (!_attached_buffer.constructed()) {
			_attached_buffer.construct(env.rm(), cap);
		}

		return _attached_buffer.constructed();
	}

	Genode::addr_t mmap_addr()
	{
		return reinterpret_cast<Gpu::addr_t>(_attached_buffer->local_addr<Gpu::addr_t>());
	}

	Gpu::Vram_id id() const
	{
		return _elem.id();
	}
};


class Lima::Call
{
	private:

		/*
		 * Noncopyable
		 */
		Call(Call const &) = delete;
		Call &operator=(Call const &) = delete;

		Genode::Env  &_env { *vfs_gpu_env() };
		Genode::Heap  _heap { _env.ram(), _env.rm() };

		/*
		 * The virtual-address allocator manages all GPU virtual
		 * addresses, which are shared by all contexts.
		 */
		struct Va_allocator
		{
			Genode::Allocator_avl _alloc;

			Va_allocator(Genode::Allocator &alloc)
			:
				_alloc { &alloc }
			{
				/*
				 * The end of range correspondes to LIMA_VA_RESERVE_START
				 * in Linux minus the page we omit at the start.
				 */
				_alloc.add_range(0x1000, 0xfff00000ul - 0x1000);
			}

			Gpu::Virtual_address alloc(uint32_t size)
			{
				return Gpu::Virtual_address { _alloc.alloc_aligned(size, 12).convert<::uint64_t>(
					[&] (void *ptr) { return (::uint64_t)ptr; },
					[&] (Range_allocator::Alloc_error) -> ::uint64_t {
						error("Could not allocate GPU virtual address for size: ", size);
						return 0;
					}) };
			}

			void free(Gpu::Virtual_address va)
			{
				_alloc.free((void*)va.value);
			}
		};

		Va_allocator _va_alloc { _heap };

		/*****************
		 ** Gpu session **
		 *****************/

		/*
		 * A Buffer wraps the actual Vram object and is used
		 * locally in each Gpu_context.
		 */
		struct Buffer
		{
			Genode::Id_space<Buffer>::Element const _elem;

			Gpu::Vram const &_vram;

			Buffer(Genode::Id_space<Buffer>       &space,
			       Gpu::Vram                const &vram)
			:
				_elem { *this, space,
				        Genode::Id_space<Buffer>::Id { .value = vram.id().value } },
				_vram { vram },

				busy { false }
			{ }

			~Buffer() { }

			bool busy;
		};

		using Buffer_space = Genode::Id_space<Buffer>;

		struct Gpu_context
		{
			private:

				/*
				 * Noncopyable
				 */
				Gpu_context(Gpu_context const &) = delete;
				Gpu_context &operator=(Gpu_context const &) = delete;

				Genode::Allocator &_alloc;

				static int _open_gpu()
				{
					int const fd = ::open("/dev/gpu", 0);
					if (fd < 0) {
						error("Failed to open '/dev/gpu': ",
						      "try configure '<gpu>' in 'dev' directory of VFS'");
						throw Gpu::Session::Invalid_state();
					}
					return fd;
				}

				static unsigned long _stat_gpu(int fd)
				{
					struct ::stat buf;
					if (::fstat(fd, &buf) < 0) {
						error("Could not stat '/dev/gpu'");
						::close(fd);
						throw Gpu::Session::Invalid_state();
					}
					return buf.st_ino;
				}

				int           const _fd;
				unsigned long const _id;

				Gpu::Connection &_gpu { *vfs_gpu_connection(_id) };

				Genode::Id_space<Gpu_context>::Element const _elem;

				/*
				 * The virtual-address allocator is solely used for
				 * the construction of the context-local exec buffer
				 * as the Gpu session requires a valid Vram object
				 * for it.
				 */
				Va_allocator &_va_alloc;

				/*
				 * The vram space is used for actual memory allocations.
				 * Each and every Gpu_context is able to allocate memory
				 * at the Gpu session although normally the main context
				 * is going to alloc memory for buffer objects.
				 */
				Gpu::Vram_id_space _vram_space { };

				template <typename FN>
				void _try_apply(Gpu::Vram_id id, FN const &fn)
				{
					try {
						_vram_space.apply<Vram>(id, fn);
					} catch (Vram_id_space::Unknown_id) { }
				}


				/*
				 * The buffer space is used to attach a given buffer
				 * object backed by an vram allocation - normally from the
				 * main context - to the given context, i.e., it is mapped
				 * when SUBMIT is executed.
				 */
				Buffer_space _buffer_space { };

				template <typename FN>
				void _try_apply(Buffer_space::Id id, FN const &fn)
				{
					try {
						_buffer_space.apply<Buffer>(id, fn);
					} catch (Buffer_space::Unknown_id) { }
				}

				/*
				 * Each context contains its on exec buffer object that is
				 * required by the Gpu session to pass on driver specific
				 * command buffer.
				 */
				Gpu::Vram *_exec_buffer;


			public:

				bool defer_destruction { false };

				static constexpr size_t _exec_buffer_size = { 256u << 10 };

				Gpu_context(Genode::Allocator &alloc,
				            Genode::Id_space<Gpu_context> &space,
				            Va_allocator &va_alloc)
				:
					_alloc { alloc },
					_fd { _open_gpu() },
					_id { _stat_gpu(_fd) },
					_elem { *this, space },
					_va_alloc { va_alloc },
					_exec_buffer { new (alloc) Gpu::Vram(_gpu, _exec_buffer_size,
						                                 _va_alloc.alloc(_exec_buffer_size),
						                                 _vram_space) }
				{ }

				~Gpu_context()
				{
					while (_buffer_space.apply_any<Buffer>([&] (Buffer &b) {
						Genode::destroy(_alloc, &b);
					})) { ; }

					/*
					 * 'close' below will destroy the Gpu session belonging
					 * to this context so free the exec buffer beforehand.
					 */
					while (_vram_space.apply_any<Vram>([&] (Vram &v) {
						Genode::destroy(_alloc, &v);
					})) { ; }

					::close(_fd);
				}

				Gpu::Connection& gpu()
				{
					return _gpu;
				}

				Gpu::Vram_capability export_vram(Gpu::Vram_id id)
				{
					Gpu::Vram_capability cap { };
					_try_apply(id, [&] (Gpu::Vram const &b) {
						cap = _gpu.export_vram(b.id());
					});
					return cap;
				}

				Buffer *import_vram(Gpu::Vram_capability cap, Gpu::Vram const &v)
				{
					Buffer *b = nullptr;

					try { _gpu.import_vram(cap, v.id()); }
					catch (... /* should only throw Invalid_state*/) {
						return nullptr; }

					try { b = new (_alloc) Buffer(_buffer_space, v); }
					catch (... /* either conflicting id or alloc failure */) {
						return nullptr; }

					return b;
				}

				void free_buffer(Buffer_space::Id id)
				{
					_try_apply(id, [&] (Buffer &b) {

						/*
						 * We have to invalidate any imported buffer as otherwise
						 * the object stays ref-counted in the driver and the
						 * VA occupied by the object is not freed.
						 *
						 * For that we repurpose 'unmap_cpu' that otherwise is
						 * not used.
						 */
						_gpu.unmap_cpu(Gpu::Vram_id { .value = id.value });
						Genode::destroy(_alloc, &b);
					});
				}

				bool buffer_space_contains(Buffer_space::Id id)
				{
					bool found = false;
					_try_apply(id, [&] (Buffer const &) { found = true; });
					return found;
				}

				Gpu::Vram_id_space &vram_space()
				{
					return _vram_space;
				}

				template <typename FN>
				void with_vram_space(Gpu::Vram_id id, FN const &fn)
				{
					_try_apply(id, fn);
				}

				template <typename FN>
				void access_exec_buffer(Genode::Env &env, FN const &fn)
				{
					/*
					 * 'env' is solely needed for mapping the exec buffer
					 * and is therefor used here locally.
					 */
					if (_exec_buffer->mmap(env)) {
						char *ptr = (char*)_exec_buffer->mmap_addr();
						if (ptr)
							fn(ptr, _exec_buffer_size);
					}
				}

				Gpu::Vram_id exec_buffer_id() const
				{
					return _exec_buffer->id();
				}

				unsigned long id() const
				{
					return _elem.id().value;
				}

				int fd() const
				{
					return _fd;
				}
		};

		using Gpu_context_space = Genode::Id_space<Gpu_context>;
		Gpu_context_space _gpu_context_space { };

		struct Syncobj
		{
			/*
			 * Noncopyable
			 */
			Syncobj(Syncobj const &) = delete;
			Syncobj &operator=(Syncobj const &) = delete;


			Gpu_context_space::Id _gc_id { .value = ~0u };

			Gpu::Sequence_number  _seqno { 0 };

			Genode::Id_space<Syncobj>::Element const _elem;


			bool sync_fd          { false };
			bool defer_destruction { false };

			Syncobj(Genode::Id_space<Syncobj> &space)
			: _elem { *this, space } { }

			void adopt(Gpu_context_space::Id id, Gpu::Sequence_number seqno)
			{
				_gc_id = id;
				_seqno = seqno;
			}

			Genode::Id_space<Syncobj>::Id id() const
			{
				return _elem.id();
			}

			Gpu_context_space::Id ctx_id() const
			{
				return _gc_id;
			}

			Gpu::Sequence_number seqno() const
			{
				return _seqno;
			}
		};
		using Syncobj_space = Genode::Id_space<Syncobj>;
		Syncobj_space _syncobj_space { };

		Gpu_context *_main_ctx {
			new (_heap) Gpu_context(_heap, _gpu_context_space, _va_alloc) };

		Gpu::Info_lima const &_gpu_info {
			*_main_ctx->gpu().attached_info<Gpu::Info_lima>() };

		template <typename FN>
		bool _apply_handle(uint32_t handle, FN const &fn)
		{
			Vram_id const id { .value = handle };

			bool found = false;
			_main_ctx->with_vram_space(id, [&] (Vram &b) {
				fn(b);
				found = true;
			});

			return found;
		}

		void _wait_for_mapping(uint32_t handle, unsigned op)
		{
			(void)_apply_handle(handle, [&] (Vram &b) {
				do {
					if (_main_ctx->gpu().set_tiling_gpu(b.id(), 0, op))
						break;

					char buf;
					(void)::read(_main_ctx->fd(), &buf, sizeof(buf));
				} while (true);
			});
		}

		int _wait_for_syncobj(int fd)
		{
			if (fd < SYNC_FD) {
				Genode::warning("ignore invalid sync fd: ", fd);
				return -1;
			}

			unsigned const handle = fd - SYNC_FD;
			Syncobj_space::Id syncobj_id { .value = handle };

			try {
				auto wait = [&] (Syncobj &sync_obj) {

					auto poll_completion = [&] (Gpu_context &gc) {
						do {
							if (gc.gpu().complete(sync_obj.seqno()))
								break;

							char buf;
							(void)::read(gc.fd(), &buf, sizeof(buf));
						} while (true);

						if (gc.defer_destruction)
							Genode::destroy(_heap, &gc);
					};
					_gpu_context_space.apply<Gpu_context>(sync_obj.ctx_id(),
					                                      poll_completion);

					if (sync_obj.defer_destruction)
						Genode::destroy(_heap, &sync_obj);
				};
				_syncobj_space.apply<Syncobj>(syncobj_id, wait);

			} catch (Syncobj_space::Unknown_id) {
				/*
				 * We could end up here on the last wait when Mesa already
				 * destroyed the syncobj. For that reason we deferred the
				 * destruction of the syncobj as well as the referenced ctx.
				 */
				return -1;
			} catch (Gpu_context_space::Unknown_id) {
				/*
				 * We will end up here in case the GP or PP job could not
				 * be submitted as Mesa does not check for successful
				 * submission.
				 */
				return -1;
			}

			return 0;
		}

		Dataspace_capability _lookup_cap_from_handle(uint32_t handle)
		{
			Dataspace_capability cap { };
			auto lookup_cap = [&] (Vram const &b) {
				cap = b.cap;
			};
			(void)_apply_handle(handle, lookup_cap);
			return cap;
		}

		/******************************
		 ** Device DRM I/O controls **
		 ******************************/

		int _drm_lima_gem_info(drm_lima_gem_info &arg)
		{
			int result = -1;
			(void)_apply_handle(arg.handle, [&] (Vram &b) {
				if (!b.mmap(_env))
					return;
				arg.offset = reinterpret_cast<::uint64_t>(b.mmap_addr());

				Gpu::Virtual_address const va = b.va;
				if (va.value == (Gpu::addr_t)-1)
					return;
				arg.va = (uint32_t)va.value;

				result = 0;
			});

			return result;
		}

		template <typename FUNC>
		void _alloc_buffer(::uint64_t const size, FUNC const &fn)
		{
			size_t donate = size;
			Vram *buffer = nullptr;

			Gpu::Virtual_address const va = _va_alloc.alloc(size);
			if (!va.value)
				return;

			try {
				retry<Gpu::Session::Out_of_ram>(
				[&] () {
					retry<Gpu::Session::Out_of_caps>(
					[&] () {
						buffer =
							new (&_heap) Vram(_main_ctx->gpu(), size, va,
							                  _main_ctx->vram_space());
					},
					[&] () {
						_main_ctx->gpu().upgrade_caps(2);
					});
				},
				[&] () {
					_main_ctx->gpu().upgrade_ram(donate);
				});
			} catch (Gpu::Vram::Allocation_failed) {
				_va_alloc.free(va);
				return;
			}

			if (buffer)
				fn(*buffer);
		}

		int _drm_lima_gem_create(drm_lima_gem_create &arg)
		{
			::uint64_t const size = arg.size;

			bool result = false;
			_alloc_buffer(size, [&](Vram const &b) {
				arg.handle = b.id().value;
				result = true;
			});
			return result ? 0 : -1;
		}

		int _drm_lima_gem_submit(drm_lima_gem_submit &arg)
		{
			Gpu_context_space::Id ctx_id { .value = arg.ctx };

			Syncobj_space::Id syncobj_id { .value = arg.out_sync };

			bool result = false;
			_syncobj_space.apply<Syncobj>(syncobj_id, [&] (Syncobj &sync_obj) {

				_gpu_context_space.apply<Gpu_context>(ctx_id, [&] (Gpu_context &gc) {

					/* XXX always map buffer id 1 to prevent GP MMU faults */
					{
						uint32_t const handle = 1;
						Buffer_space::Id const id = { .value = handle };
						if (!gc.buffer_space_contains(id)) {

							(void)_apply_handle(handle, [&] (Gpu::Vram const &v) {
								Gpu::Vram_capability cap = _main_ctx->export_vram(v.id());
								if (gc.import_vram(cap, v) == nullptr) {
									Genode::error("could force mapping of buffer ", handle);
									return;
								}
							});
						}
					}

					/*
					 * Check if we have access to all needed buffer objects and
					 * if not import them from the main context that normaly performs
					 * all allocations.
					 */

					bool all_buffer_mapped = true;
					Lima::for_each_submit_bo(arg, [&] (drm_lima_gem_submit_bo const &bo) {
						Buffer_space::Id const id = { .value = bo.handle };
						if (!gc.buffer_space_contains(id)) {

							bool imported = false;
							(void)_apply_handle(bo.handle, [&] (Gpu::Vram const &v) {
								Gpu::Vram_capability cap = _main_ctx->export_vram(v.id());
								if (gc.import_vram(cap, v) == nullptr)
									return;

								imported = true;
							});

							if (!imported)
								all_buffer_mapped = false;
						}
					});

					if (!all_buffer_mapped)
						return;

					bool serialized = false;
					gc.access_exec_buffer(_env, [&] (char *ptr, size_t size) {

						size_t const payload_size = Lima::get_payload_size(arg);
						if (payload_size > size) {
							error("exec buffer for context ", ctx_id.value,
							      " too small, got ", size, " but needed ",
							      payload_size);
							return;
						}

						/*
						 * Copy each array flat to the exec buffer and adjust the
						 * addresses in the submit object.
						 */
						Genode::memset(ptr, 0, size);
						Lima::serialize(&arg, ptr);

						serialized = true;
					});

					if (!serialized)
						return;

					try {
						Gpu::Connection &gpu = gc.gpu();

						Gpu::Sequence_number const seqno =
							gpu.execute(gc.exec_buffer_id(), 0);

						sync_obj.adopt(Gpu_context_space::Id { .value = gc.id() },
						               seqno);

						result = true;
					} catch (Gpu::Session::Invalid_state) {
						warning(": could not execute: ", gc.exec_buffer_id().value);
					}
				});
			});

			return result ? 0 : -1;
		}

		int _drm_lima_gem_wait(drm_lima_gem_wait &arg)
		{
			/*
 			 * For the moment we do not handle timeouts
			 */
			(void)arg.timeout_ns;
			_wait_for_mapping(arg.handle, arg.op);
			return 0;
		}

		int _drm_lima_get_param(drm_lima_get_param &arg)
		{
			if (arg.param > Gpu::Info_lima::MAX_LIMA_PARAMS) {
				errno = EINVAL;
				return -1;
			}

			arg.value = _gpu_info.param[arg.param];
			return 0;
		}

		int _drm_lima_ctx_create(drm_lima_ctx_create &arg)
		{
			try {
				Gpu_context * ctx =
					new (_heap) Gpu_context(_heap, _gpu_context_space, _va_alloc);

				arg.id = ctx->id();
				return 0;
			} catch (... /* intentional catch-all ... */) {
				/* ... as the lima GPU driver will not throw */
			}
			return -1;
		}

		int _drm_lima_ctx_free(drm_lima_ctx_free &arg)
		{
			Gpu_context_space::Id id { .value = arg.id };

			bool result = false;
			auto free_ctx = [&] (Gpu_context &ctx) {

				result = true;

				/*
				 * If the ctx is currently referenced by a syncobj its
				 * destruction gets deferred until the final wait-for-syncobj
				 * call.
				 */
				_syncobj_space.for_each<Syncobj>([&] (Syncobj &obj) {
					if (obj.ctx_id().value == ctx.id())
						ctx.defer_destruction = true;
				});

				if (!ctx.defer_destruction)
					Genode::destroy(_heap, &ctx);
			};
			_gpu_context_space.apply<Gpu_context>(id, free_ctx);

			return result ? 0 : -1;
		}

		int _device_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case DRM_LIMA_CTX_CREATE:
				return _drm_lima_ctx_create(*reinterpret_cast<drm_lima_ctx_create*>(arg));
			case DRM_LIMA_CTX_FREE:
				return _drm_lima_ctx_free(*reinterpret_cast<drm_lima_ctx_free*>(arg));
			case DRM_LIMA_GEM_INFO:
				return _drm_lima_gem_info(*reinterpret_cast<drm_lima_gem_info*>(arg));
			case DRM_LIMA_GEM_CREATE:
				return _drm_lima_gem_create(*reinterpret_cast<drm_lima_gem_create*>(arg));
			case DRM_LIMA_GEM_SUBMIT:
				return _drm_lima_gem_submit(*reinterpret_cast<drm_lima_gem_submit*>(arg));
			case DRM_LIMA_GEM_WAIT:
				return _drm_lima_gem_wait(*reinterpret_cast<drm_lima_gem_wait*>(arg));
			case DRM_LIMA_GET_PARAM:
				return _drm_lima_get_param(*reinterpret_cast<drm_lima_get_param*>(arg));
			default: break;
			}

			return 0;
		}

		/*******************************
		  ** Generic DRM I/O controls **
		 *******************************/

		int _drm_gem_close(drm_gem_close const &gem_close)
		{
			auto free_buffer = [&] (Gpu_context &ctx) {
				Buffer_space::Id const id { .value = gem_close.handle };
				ctx.free_buffer(id);
			};
			_gpu_context_space.for_each<Gpu_context>(free_buffer);

			return _apply_handle(gem_close.handle,
				[&] (Gpu::Vram &b) {
					_va_alloc.free(b.va);
					destroy(_heap, &b);
				}) ? 0 : -1;
		}

		int _drm_version(drm_version &version)
		{
			version.version_major = 1;
			version.version_minor = 1;
			version.version_patchlevel = 0;

			/**
			 * Libdrm probes the length by calling version twice
			 * and the second time strings are allocated.
			 */

			version.name_len = 1;
			if (version.name)
				version.name[0] = '\0';
			version.date_len = 1;
			if (version.date)
				version.date[0] = '\0';
			version.desc_len = 1;
			if (version.desc)
				version.desc[0] = '\0';

			return 0;
		}

		int _drm_syncobj_create(drm_syncobj_create &arg)
		{
			try {
				Syncobj *obj = new (_heap) Syncobj(_syncobj_space);
				arg.handle = (uint32_t)obj->id().value;
				return 0;
			} catch (... /* XXX which exceptions can occur? */) { }
			return -1;
		}

		int _drm_syncobj_destroy(drm_syncobj_destroy &arg)
		{
			Syncobj_space::Id id { .value = arg.handle };

			bool result = false;
			_syncobj_space.apply<Syncobj>(id, [&] (Syncobj &obj) {
				result = true;

				/*
				 * In case the syncobj was once exported destruction
				 * gets deferred until the final wait-for-syncobj call.
				 */
				if (obj.sync_fd) {
					obj.defer_destruction = true;
					return;
				}

				Genode::destroy(_heap, &obj);
			});
			return result ? 0 : -1;
		}

		int _drm_syncobj_handle_to_fd(drm_syncobj_handle &arg)
		{
			arg.fd = -1;

			Syncobj_space::Id id { .value = arg.handle };
			_syncobj_space.apply<Syncobj>(id, [&] (Syncobj &obj) {
				obj.sync_fd = true;

				arg.fd = arg.handle + SYNC_FD;
			});

			return arg.fd != -1 ? 0 : -1;
		}

		int _generic_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case command_number(DRM_IOCTL_GEM_CLOSE):
				return _drm_gem_close(*reinterpret_cast<drm_gem_close*>(arg));
			case command_number(DRM_IOCTL_VERSION):
				return _drm_version(*reinterpret_cast<drm_version*>(arg));
			case command_number(DRM_IOCTL_SYNCOBJ_CREATE):
				return _drm_syncobj_create(*reinterpret_cast<drm_syncobj_create*>(arg));
			case command_number(DRM_IOCTL_SYNCOBJ_DESTROY):
				return _drm_syncobj_destroy(*reinterpret_cast<drm_syncobj_destroy*>(arg));
			case command_number(DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD):
				return _drm_syncobj_handle_to_fd(*reinterpret_cast<drm_syncobj_handle*>(arg));
			default:
				error("unhandled generic DRM ioctl: ", Genode::Hex(cmd));
				break;
			}

			return -1;
		}


	public:

		/* arbitrary start value out of the libc's FD alloc range */
		static constexpr int const SYNC_FD { 10000 };

		Call() { }

		~Call()
		{
			while (_syncobj_space.apply_any<Syncobj>([&] (Syncobj &obj) {
				Genode::destroy(_heap, &obj); })) { ; }

			while (_gpu_context_space.apply_any<Gpu_context>([&] (Gpu_context &ctx) {
				Genode::destroy(_heap, &ctx); })) { ; }
		}

		int ioctl(unsigned long request, void *arg)
		{
			bool const device_request = device_ioctl(request);
			return device_request ? _device_ioctl(device_number(request), arg)
			                      : _generic_ioctl(command_number(request), arg);
		}

		int wait_for_syncobj(int fd)
		{
			return _wait_for_syncobj(fd);
		}

		void *mmap(unsigned long offset, unsigned long /* size */)
		{
			/*
			 * Buffer should have been mapped during GEM INFO call.
			 */
			return (void*)offset;
		}

		void munmap(void *addr)
		{
			/*
			 * We rely on GEM CLOSE to destroy the buffer and thereby
			 * to remove the local mapping. AFAICT the 'munmap' is indeed
			 * (always) followed by the CLOSE I/O control.
			 */
			(void)addr;
		}

		void wait_for_syncobj(unsigned int handle)
		{
			_wait_for_syncobj(handle);
		}
};


static Genode::Constructible<Lima::Call> _drm;


void lima_drm_init()
{
	/* make sure VFS is initialized */
	struct ::stat buf;
	if (stat("/dev/gpu", &buf) < 0) {
		Genode::error("'/dev/gpu' not accessible: ",
		              "try configure '<gpu>' in 'dev' directory of VFS'");
		return;
	}

	_drm.construct();
}


static void dump_ioctl(unsigned long request)
{
	using namespace Genode;

	log("ioctl(request=", Hex(request),
	    (request & 0xe0000000u) == IOC_OUT   ? " out"   :
	    (request & 0xe0000000u) == IOC_IN    ? " in"    :
	    (request & 0xe0000000u) == IOC_INOUT ? " inout" : " void",
	    " len=", IOCPARM_LEN(request),
	    " cmd=", command_name(request), " (", Hex(command_number(request)), "))");
}


int lima_drm_ioctl(unsigned long request, void *arg)
{
	static pthread_mutex_t ioctl_mutex = PTHREAD_MUTEX_INITIALIZER;
	int const err = pthread_mutex_lock(&ioctl_mutex);
	if (err) {
		Genode::error("could not lock ioctl mutex: ", err);
		return -1;
	}

	if (verbose_ioctl)
		dump_ioctl(request);

	int const ret = _drm->ioctl(request, arg);

	if (verbose_ioctl)
		Genode::log("returned ", ret);

	pthread_mutex_unlock(&ioctl_mutex);
	return ret;
}


void *lima_drm_mmap(off_t offset, size_t length)
{
	return _drm->mmap(offset, length);
}


int lima_drm_munmap(void *addr)
{
	_drm->munmap(addr);
	return 0;
}


int lima_drm_poll(int fd)
{
	static pthread_mutex_t poll_mutex = PTHREAD_MUTEX_INITIALIZER;
	int const err = pthread_mutex_lock(&poll_mutex);
	if (err) {
		Genode::error("could not lock poll mutex: ", err);
		return -1;
	}

	int const ret = _drm->wait_for_syncobj(fd);

	pthread_mutex_unlock(&poll_mutex);
	return ret;
}
