/*
 * \brief  DRM ioctl backend
 * \author Sebastian Sumpf
 * \author Josef Soentgen
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
#include <gpu_session/connection.h>
#include <gpu/info_etnaviv.h>
#include <util/string.h>

#include <vfs_gpu.h>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <drm.h>
#include <etnaviv_drm.h>
#include <libdrm_macros.h>
}


enum { verbose_ioctl = false };

namespace {

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
		case command_number(DRM_IOCTL_VERSION):            return "DRM_IOCTL_VERSION";
		case command_number(DRM_IOCTL_GEM_CLOSE):          return "DRM_IOCTL_GEM_CLOSE";
		case command_number(DRM_IOCTL_GEM_FLINK):          return "DRM_IOCTL_GEM_FLINK";
		case command_number(DRM_IOCTL_GEM_OPEN):           return "DRM_IOCTL_GEM_OPEN";
		case command_number(DRM_IOCTL_GET_CAP):            return "DRM_IOCTL_GET_CAP";
		case command_number(DRM_IOCTL_PRIME_HANDLE_TO_FD): return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
		case command_number(DRM_IOCTL_PRIME_FD_TO_HANDLE): return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
		default:                                  return "<unknown drm>";
		}
	}

	switch (device_number(request)) {
	case DRM_ETNAVIV_GET_PARAM:    return "DRM_ETNAVIV_GET_PARAM";
	case DRM_ETNAVIV_GEM_NEW:      return "DRM_ETNAVIV_GEM_NEW";
	case DRM_ETNAVIV_GEM_INFO:     return "DRM_ETNAVIV_GEM_INFO";
	case DRM_ETNAVIV_GEM_CPU_PREP: return "DRM_ETNAVIV_GEM_CPU_PREP";
	case DRM_ETNAVIV_GEM_CPU_FINI: return "DRM_ETNAVIV_GEM_CPU_FINI";
	case DRM_ETNAVIV_GEM_SUBMIT:   return "DRM_ETNAVIV_GEM_SUBMIT";
	case DRM_ETNAVIV_WAIT_FENCE:   return "DRM_ETNAVIV_WAIT_FENCE";
	case DRM_ETNAVIV_GEM_USERPTR:  return "DRM_ETNAVIV_GEM_USERPTR";
	case DRM_ETNAVIV_GEM_WAIT:     return "DRM_ETNAVIV_GEM_WAIT";
	case DRM_ETNAVIV_PM_QUERY_DOM: return "DRM_ETNAVIV_PM_QUERY_DOM";
	case DRM_ETNAVIV_PM_QUERY_SIG: return "DRM_ETNAVIV_PM_QUERY_SIG";
	case DRM_ETNAVIV_NUM_IOCTLS:   return "DRM_ETNAVIV_NUM_IOCTLS";
	default:
		return "<unknown driver>";
	}
}

} /* anonymous namespace */


namespace Etnaviv {

	size_t get_payload_size(drm_etnaviv_gem_submit const &submit);

	// XXX better implement as 'size_t for_each_object(T const *t, unsigned len, FN const &fn, char *dst)'
	template <typename FN, typename T> void for_each_object(T const *t, unsigned len, FN const &fn)
	{
		for (unsigned i = 0; i < len; i++) {
			T const *obj = &t[i];
			fn(obj);
		}
	}

	void serialize(drm_etnaviv_gem_submit *submit, char *content);

	size_t get_payload_size(drm_version const &version);
	void serialize(drm_version *version, char *content);
	void deserialize(drm_version *version, char *content);

} /* anonymous namespace */


size_t Etnaviv::get_payload_size(drm_etnaviv_gem_submit const &submit)
{
	size_t size = 0;

	size += sizeof (drm_etnaviv_gem_submit_reloc) * submit.nr_relocs;
	size += sizeof (drm_etnaviv_gem_submit_bo) * submit.nr_bos;
	size += sizeof (drm_etnaviv_gem_submit_pmr) * submit.nr_pmrs;
	size += submit.stream_size;

	return size;
}


void Etnaviv::serialize(drm_etnaviv_gem_submit *submit, char *content)
{
	size_t offset = 0;

	/* leave place for object itself first */
	offset += sizeof (*submit);

	/* next are the buffer-objects */
	if (submit->nr_bos) {
		size_t const new_start = offset;

		auto copy_bos = [&] (drm_etnaviv_gem_submit_bo const *bo) {
			char * const dst = content + offset;
			Genode::memcpy(dst, bo, sizeof (*bo));
			offset += sizeof (*bo);
		};
		for_each_object((drm_etnaviv_gem_submit_bo*)submit->bos,
		                submit->nr_bos, copy_bos);
		submit->bos = reinterpret_cast<__u64>(new_start);
	}

	/* next are the relocs */
	if (submit->nr_relocs) {
		size_t const new_start = offset;

		auto copy_relocs = [&] (drm_etnaviv_gem_submit_reloc const *reloc) {
			char * const dst = content + offset;
			Genode::memcpy(dst, reloc, sizeof (*reloc));
			offset += sizeof (*reloc);
		};
		for_each_object((drm_etnaviv_gem_submit_reloc*)submit->relocs,
		                submit->nr_relocs, copy_relocs);
		submit->relocs = reinterpret_cast<__u64>(new_start);
	}

	/* next are the pmrs */
	if (submit->nr_pmrs) {
		size_t const new_start = offset;
		auto copy_pmrs = [&] (drm_etnaviv_gem_submit_pmr const *pmr) {
			char * const dst = content + offset;
			Genode::memcpy(dst, pmr, sizeof (*pmr));
			offset += sizeof (*pmr);
		};
		for_each_object((drm_etnaviv_gem_submit_pmr*)submit->pmrs,
		                submit->nr_pmrs, copy_pmrs);
		submit->pmrs = reinterpret_cast<__u64>(new_start);
	}

	/* next is the cmd stream */
	{
		size_t const new_start = offset;

		char * const dst = content + offset;
		Genode::memcpy(dst, reinterpret_cast<void const*>(submit->stream), submit->stream_size);
		offset += submit->stream_size;
		submit->stream = reinterpret_cast<__u64>(new_start);
	}

	/* copy submit object last but into the front */
	Genode::memcpy(content, submit, sizeof (*submit));
}


size_t Etnaviv::get_payload_size(drm_version const &version)
{
	size_t size = 0;
	size += version.name_len;
	size += version.date_len;
	size += version.desc_len;
	return size;
}


void Etnaviv::serialize(drm_version *version, char *content)
{
	size_t offset = 0;
	char *start = 0;
	offset += sizeof (*version);

	start = (char*)offset;
	version->name = start;
	offset += version->name_len;

	start = (char*)offset;
	version->date = start;
	offset += version->date_len;

	start = (char*)offset;
	version->desc = start;
	offset += version->desc_len;

	Genode::memcpy(content, version, sizeof (*version));
}


void Etnaviv::deserialize(drm_version *version, char *content)
{
	drm_version *cversion = reinterpret_cast<drm_version*>(content);

	version->version_major      = cversion->version_major;
	version->version_minor      = cversion->version_minor;
	version->version_patchlevel = cversion->version_patchlevel;

	version->name += (unsigned long)version;
	version->date += (unsigned long)version;
	version->desc += (unsigned long)version;

	cversion->name += (unsigned long)cversion;
	cversion->date += (unsigned long)cversion;
	cversion->desc += (unsigned long)cversion;

	Genode::copy_cstring(version->name, cversion->name, cversion->name_len);
	Genode::copy_cstring(version->date, cversion->date, cversion->date_len);
	Genode::copy_cstring(version->desc, cversion->desc, cversion->desc_len);
}


namespace Etnaviv {
	using namespace Genode;
	using namespace Gpu;

	struct Call;
} /* namespace Etnaviv */


struct Gpu::Vram
{
	struct Allocation_failed : Genode::Exception { };

	Gpu::Connection &_gpu;

	Genode::Id_space<Gpu::Vram>::Element const _elem;

	Genode::Dataspace_capability const cap;
	size_t                       const size;

	Genode::Constructible<Genode::Attached_dataspace> _attached_buffer { };

	Vram(Gpu::Connection  &gpu,
	     size_t            size,
	     Vram_id_space    &space)
	:
		_gpu  { gpu },
		_elem { *this, space },
		 cap  { _gpu.alloc_vram(_elem.id(), size) },
		 size { size }
	{
		if (!cap.valid())
			throw Allocation_failed();
	}

	~Vram()
	{
		_gpu.free_vram(_elem.id());
	}

	bool mmap(Genode::Env &env)
	{
		if (!_attached_buffer.constructed()) {
			_attached_buffer.construct(env.rm(), cap);
		}

		return _attached_buffer.constructed();
	}

	Gpu::addr_t mmap_addr()
	{
		return reinterpret_cast<Gpu::addr_t>(_attached_buffer->local_addr<Gpu::addr_t>());
	}

	Gpu::Vram_id id() const
	{
		return _elem.id();
	}
};


class Etnaviv::Call
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
				 * Each context contains its own exec buffer object that is
				 * required by the Gpu session to pass on the driver specific
				 * command buffer.
				 */
				Gpu::Vram *_exec_buffer;


			public:

				bool defer_destruction { false };
				bool fenced            { false };

				static constexpr size_t _exec_buffer_size = { 256u << 10 };

				Gpu_context(Genode::Allocator &alloc,
				            Genode::Id_space<Gpu_context> &space)
				:
					_alloc { alloc },
					_fd { _open_gpu() },
					_id { _stat_gpu(_fd) },
					_elem { *this, space },
					_exec_buffer { new (alloc) Gpu::Vram(_gpu, _exec_buffer_size,
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
						 * For that we repurpose 'unmap_gpu' that otherwise is
						 * not used.
						 */
						_gpu.unmap_gpu(Gpu::Vram_id { .value = id.value }, 0,
						               Gpu::Virtual_address());
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

		struct Fenceobj
		{
			Genode::Id_space<Fenceobj>::Element const _elem;

			uint32_t const _fence;

			Fenceobj(Genode::Id_space<Fenceobj> &space,
			         Gpu_context_space::Id       ctx_id,
			         uint32_t                    fence)
			:
				_elem  { *this, space, { .value = ctx_id.value } },
				_fence { fence }
			{ }

			Gpu_context_space::Id ctx_id() const
			{
				return Gpu_context_space::Id { .value = _elem.id().value };
			}

			uint32_t fence() const { return _fence; }
		};

		using Fenceobj_space = Genode::Id_space<Fenceobj>;
		Fenceobj_space _fenceobj_space { };

		Gpu::Vram_id_space _buffer_space { };

		Gpu_context *_main_ctx {
			new (_heap) Gpu_context(_heap, _gpu_context_space) };

		Gpu::Info_etnaviv const &_gpu_info {
			*_main_ctx->gpu().attached_info<Gpu::Info_etnaviv>() };

		template <typename FN>
		bool _apply_handle(uint32_t handle, FN const &fn)
		{
			Vram_id const id { .value = handle };

			bool found = false;
			_main_ctx->with_vram_space(id, [&] (Vram &vram) {
				fn(vram);
				found = true;
			});

			return found;
		}

		void _wait_for_completion(uint32_t fence)
		{
			try {
				auto lookup_fenceobj = [&] (Fenceobj &fo) {

					Sequence_number const seqno { .value = fo.fence() };

					auto poll_completion = [&] (Gpu_context &gc) {
						do {
							if (gc.gpu().complete(seqno))
								break;

							char buf;
							(void)::read(gc.fd(), &buf, sizeof(buf));
						} while (true);

						if (gc.defer_destruction)
							Genode::destroy(_heap, &gc);
					};
					_gpu_context_space.apply<Gpu_context>(fo.ctx_id(),
					                                      poll_completion);
				};
				_fenceobj_space.apply<Fenceobj>(Fenceobj_space::Id { .value = fence },
				                                lookup_fenceobj);
			} catch (Fenceobj_space::Unknown_id) {
				/* XXX */
			} catch (Gpu_context_space::Unknown_id) {
				/* XXX */
			}
		}

		template <typename FUNC>
		void _alloc_buffer(::uint64_t const size, FUNC const &fn)
		{
			size_t donate = size;
			Vram *vram = nullptr;
			try {
				retry<Gpu::Session::Out_of_ram>(
				[&] () {
					retry<Gpu::Session::Out_of_caps>(
					[&] () {
						vram =
							new (&_heap) Vram(_main_ctx->gpu(), size,
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
				return;
			}

			if (vram)
				fn(*vram);
		}

		/******************************
		 ** Device DRM I/O controls **
		 ******************************/

		int _drm_etnaviv_gem_info(drm_etnaviv_gem_info &arg)
		{
			return _apply_handle(arg.handle, [&] (Vram &vram) {
				if (!vram.mmap(_env))
					return;
				arg.offset = reinterpret_cast<Genode::uint64_t>(vram.mmap_addr());
			}) ? 0 : -1;
		}

		int _drm_etnaviv_gem_cpu_fini(drm_etnaviv_gem_cpu_fini &arg)
		{
			return _apply_handle(arg.handle, [&] (Vram const &vram) {
				_main_ctx->gpu().unmap_cpu(vram.id());
			}) ? 0 : -1;
		}

		int _drm_etnaviv_gem_cpu_prep(drm_etnaviv_gem_cpu_prep &arg)
		{
			int res = -1;
			return _apply_handle(arg.handle, [&] (Vram const &vram) {

				Gpu::Mapping_attributes attrs {
					arg.op == ETNA_PREP_READ,
					arg.op == ETNA_PREP_WRITE
				};

				/*
				 * For now we ignore NOSYNC as well as timeouts.
				 */

				do {
					Dataspace_capability const map_cap =
						_main_ctx->gpu().map_cpu(vram.id(), attrs);
					if (map_cap.valid())
						break;

					char buf;
					(void)::read(_main_ctx->fd(), &buf, sizeof(buf));
				} while (true);
				res = 0;

			}) ? res : -1;
		}

		int _drm_etnaviv_gem_new(drm_etnaviv_gem_new &arg)
		{
			::uint64_t const size = arg.size;

			bool result = false;
			_alloc_buffer(size, [&](Vram const &vram) {
				arg.handle = vram.id().value;
				result = true;
			});
			return result ? 0 : -1;
		}

		int _drm_etnaviv_gem_submit(drm_etnaviv_gem_submit &arg)
		{
			Gpu_context_space::Id ctx_id { .value = _main_ctx->id() };

			bool result = false;
			_gpu_context_space.apply<Gpu_context>(ctx_id, [&] (Gpu_context &gc) {

				bool serialized = false;
				gc.access_exec_buffer(_env, [&] (char *ptr, size_t size) {

					size_t const payload_size = Etnaviv::get_payload_size(arg);
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
					Etnaviv::serialize(&arg, ptr);

					serialized = true;
				});

				if (!serialized)
					return;

				try {
					Genode::uint64_t const pending_exec_buffer =
						gc.gpu().execute(gc.exec_buffer_id(), 0).value;

					arg.fence = pending_exec_buffer & 0xffffffffu;

					/* XXX make part of context ? */
					if (gc.fenced == false) {
						new (&_heap) Fenceobj(_fenceobj_space,
						                      Gpu_context_space::Id { .value = gc.id() },
						                      arg.fence);
						gc.fenced = true;
					}

					result = true;
				} catch (Gpu::Session::Invalid_state) {
					warning(": could not execute: ", gc.exec_buffer_id().value);
				}
			});

			return result ? 0 : -1;
		}

		int _drm_etnaviv_gem_wait(drm_etnaviv_gem_wait &)
		{
			warning(__func__, ": not implemented");
			return -1;
		}

		int _drm_etnaviv_gem_userptr(drm_etnaviv_gem_userptr &)
		{
			warning(__func__, ": not implemented");
			return -1;
		}

		int _drm_etnaviv_get_param(drm_etnaviv_param &arg)
		{
			if (arg.param > Gpu::Info_etnaviv::MAX_ETNAVIV_PARAMS) {
				errno = EINVAL;
				return -1;
			}

			arg.value = _gpu_info.param[arg.param];
			return 0;
		}

		int _drm_etnaviv_pm_query_dom(drm_etnaviv_pm_domain &)
		{
			warning(__func__, ": not implemented");
			return -1;
		}

		int _drm_etnaviv_pm_query_sig(drm_etnaviv_pm_signal &)
		{
			warning(__func__, ": not implemented");
			return -1;
		}

		int _drm_etnaviv_wait_fence(drm_etnaviv_wait_fence &arg)
		{
			_wait_for_completion(arg.fence);
			return 0;
		}

		int _device_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case DRM_ETNAVIV_GEM_CPU_FINI:
				return _drm_etnaviv_gem_cpu_fini(*reinterpret_cast<drm_etnaviv_gem_cpu_fini*>(arg));
			case DRM_ETNAVIV_GEM_CPU_PREP:
				return _drm_etnaviv_gem_cpu_prep(*reinterpret_cast<drm_etnaviv_gem_cpu_prep*>(arg));
			case DRM_ETNAVIV_GEM_INFO:
				return _drm_etnaviv_gem_info(*reinterpret_cast<drm_etnaviv_gem_info*>(arg));
			case DRM_ETNAVIV_GEM_NEW:
				return _drm_etnaviv_gem_new(*reinterpret_cast<drm_etnaviv_gem_new*>(arg));
			case DRM_ETNAVIV_GEM_SUBMIT:
				return _drm_etnaviv_gem_submit(*reinterpret_cast<drm_etnaviv_gem_submit*>(arg));
			case DRM_ETNAVIV_GEM_USERPTR:
				return _drm_etnaviv_gem_userptr(*reinterpret_cast<drm_etnaviv_gem_userptr*>(arg));
			case DRM_ETNAVIV_GEM_WAIT:
				return _drm_etnaviv_gem_wait(*reinterpret_cast<drm_etnaviv_gem_wait*>(arg));
			case DRM_ETNAVIV_GET_PARAM:
				return _drm_etnaviv_get_param(*reinterpret_cast<drm_etnaviv_param*>(arg));
			case DRM_ETNAVIV_PM_QUERY_DOM:
				return _drm_etnaviv_pm_query_dom(*reinterpret_cast<drm_etnaviv_pm_domain*>(arg));
			case DRM_ETNAVIV_PM_QUERY_SIG:
				return _drm_etnaviv_pm_query_sig(*reinterpret_cast<drm_etnaviv_pm_signal*>(arg));
			case DRM_ETNAVIV_WAIT_FENCE:
				return _drm_etnaviv_wait_fence(*reinterpret_cast<drm_etnaviv_wait_fence*>(arg));
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
					destroy(_heap, &b);
				}) ? 0 : -1;
		}

		int _drm_version(drm_version &version)
		{
			version.version_major = 1;
			version.version_minor = 3;
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
			default:
				error("unhandled generic DRM ioctl: ", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

	public:

		Call() { }

		~Call()
		{
			while (_fenceobj_space.apply_any<Fenceobj>([&] (Fenceobj &obj) {
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
};


static Genode::Constructible<Etnaviv::Call> _drm;


void etnaviv_drm_init()
{
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
	    " cmd=", command_name(request), " (", Hex(command_number(request)), ")");
}


int etnaviv_drm_ioctl(unsigned long request, void *arg)
{
	if (verbose_ioctl)
		dump_ioctl(request);

	try {
		int ret = _drm->ioctl(request, arg);

		if (verbose_ioctl)
			Genode::log("returned ", ret);

		return ret;
	} catch (...) { }

	return -1;
}


void *etnaviv_drm_mmap(off_t offset, size_t length)
{
	return _drm->mmap(offset, length);
}


int etnaviv_drm_munmap(void *addr)
{
	_drm->munmap(addr);
	return 0;
}
