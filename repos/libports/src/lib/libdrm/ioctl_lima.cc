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
#include <base/debug.h>
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

#include <drm.h>
#include <drm-uapi/lima_drm.h>
#include <libdrm_macros.h>
}


enum { verbose_ioctl = false };


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
} /* namespace Gpu */


struct Gpu::Buffer
{
	Gpu::Connection &_gpu;

	Genode::Id_space<Gpu::Buffer>::Element const _elem;

	Genode::Dataspace_capability const cap;
	size_t                       const size;

	Genode::Constructible<Genode::Attached_dataspace> _attached_buffer { };

	Buffer(Gpu::Connection               &gpu,
	       size_t                         size,
	       Genode::Id_space<Gpu::Buffer> &space)
	:
		_gpu  { gpu },
		_elem { *this, space },
		 cap  { _gpu.alloc_buffer(_elem.id(), size) },
		 size { size }
	{ }

	virtual ~Buffer()
	{
		_gpu.free_buffer(_elem.id());
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
		return reinterpret_cast<addr_t>(_attached_buffer->local_addr<addr_t>());
	}

	Gpu::Buffer_id id() const
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

		/*****************
		 ** Gpu session **
		 *****************/

		struct Gpu_context
		{
			int const fd;
			unsigned long const _gpu_id;

			Gpu::Connection &_gpu { *vfs_gpu_connection(_gpu_id) };

			using Id_space = Genode::Id_space<Gpu_context>;
			Id_space::Element const _elem;

			Gpu_context(int fd, unsigned long gpu,
			            Genode::Id_space<Gpu_context> &space)
			: fd { fd }, _gpu_id { gpu }, _elem { *this, space } { }

			virtual ~Gpu_context()
			{
				::close(fd);
			}

			unsigned long id() const
			{
				return _elem.id().value;
			}

			Gpu::Connection& gpu()
			{
				return _gpu;
			}
		};

		using Gpu_context_space = Genode::Id_space<Gpu_context>;
		Gpu_context_space _gpu_context_space { };

		Gpu_context &_create_ctx()
		{
			int const fd = ::open("/dev/gpu", 0);
			if (fd < 0) {
				Genode::error("Failed to open '/dev/gpu': ",
				              "try configure '<gpu>' in 'dev' directory of VFS'");
				throw Gpu::Session::Invalid_state();
			}

			struct ::stat buf;
			if (::fstat(fd, &buf) < 0) {
				Genode::error("Could not stat '/dev/gpu'");
				::close(fd);
				throw Gpu::Session::Invalid_state();
			}
			Gpu_context * context =
				new (_heap) Gpu_context(fd, buf.st_ino, _gpu_context_space);

			return *context;
		}

		struct Syncobj
		{
			/*
			 * Noncopyable
			 */
			Syncobj(Syncobj const &) = delete;
			Syncobj &operator=(Syncobj const &) = delete;


			Gpu_context          *_gc    { nullptr };
			Gpu::Sequence_number  _seqno { 0 };

			using Id_space = Genode::Id_space<Syncobj>;
			Id_space::Element const _elem;

			Syncobj(Id_space &space)
			: _elem { *this, space } { }

			unsigned long id() const
			{
				return _elem.id().value;
			}

			void adopt(Gpu_context &gc, Gpu::Sequence_number seqno)
			{
				_gc = &gc;
				_seqno = seqno;
			}

			Gpu_context &gpu_context()
			{
				if (!_gc) {
					struct Invalid_gpu_context { };
					throw Invalid_gpu_context();
				}

				return *_gc;
			}

			Gpu::Sequence_number seqno() const
			{
				return _seqno;
			}
		};
		Genode::Id_space<Syncobj> _syncobj_space { };

		struct Gpu_session
		{
			int const fd;
			unsigned long const id;

			Gpu_session(int fd, unsigned long id)
			: fd { fd }, id { id } { }

			virtual ~Gpu_session()
			{
				::close(fd);
			}

			Gpu::Connection &gpu()
			{
				return *vfs_gpu_connection(id);
			}
		};

		Gpu_session _open_gpu()
		{
			int const fd = ::open("/dev/gpu", 0);
			if (fd < 0) {
				Genode::error("Failed to open '/dev/gpu': ",
				              "try configure '<gpu>' in 'dev' directory of VFS'");
				throw Gpu::Session::Invalid_state();
			}

			struct ::stat buf;
			if (::fstat(fd, &buf) < 0) {
				Genode::error("Could not stat '/dev/gpu'");
				::close(fd);
				throw Gpu::Session::Invalid_state();
			}

			return Gpu_session { fd, buf.st_ino };
		}

		Gpu_session _gpu_session { _open_gpu() };

		Gpu::Connection &_gpu { _gpu_session.gpu() };
		Gpu::Info_lima const &_gpu_info {
			*_gpu.attached_info<Gpu::Info_lima>() };

		Id_space<Buffer> _buffer_space { };

		/*
		 * Play it safe, glmark2 apparently submits araound 110 KiB at
		 * some point.
		 */
		enum { EXEC_BUFFER_SIZE = 256u << 10 };
		Constructible<Buffer> _exec_buffer { };

		void _wait_for_mapping(uint32_t handle, unsigned op)
		{
			Buffer_id const id { .value = handle };
			do {
				if (_gpu.set_tiling(id, op))
					break;

				char buf;
				(void)::read(_gpu_session.fd, &buf, sizeof(buf));
			} while (true);
		}

		void _wait_for_syncobj(unsigned int handle)
		{
			Syncobj::Id_space::Id syncobj_id { .value = handle };

			try {
				auto wait = [&] (Syncobj &sync_obj) {

					Gpu_context &gc = sync_obj.gpu_context();
					do {
						if (gc.gpu().complete(sync_obj.seqno()))
							break;

						char buf;
						(void)::read(gc.fd, &buf, sizeof(buf));
					} while (true);
				};
				_syncobj_space.apply<Syncobj>(syncobj_id, wait);
			} catch (Genode::Id_space<Lima::Call::Syncobj>::Unknown_id) { }
		}

		template <typename FN>
		bool _apply_handle(uint32_t handle, FN const &fn)
		{
			Buffer_id const id { .value = handle };

			bool found = false;
			_buffer_space.apply<Buffer>(id, [&] (Buffer &b) {
				fn(b);
				found = true;
			});

			return found;
		}

		Dataspace_capability _lookup_cap_from_handle(uint32_t handle)
		{
			Dataspace_capability cap { };
			auto lookup_cap = [&] (Buffer const &b) {
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
			(void)_apply_handle(arg.handle, [&] (Buffer &b) {
				if (!b.mmap(_env))
					return;
				arg.offset = reinterpret_cast<::uint64_t>(b.mmap_addr());

				Gpu::addr_t const va = _gpu.query_buffer_ppgtt(b.id());
				if (va == (Gpu::addr_t)-1)
					return;
				arg.va = (uint32_t)va;

				result = 0;
			});

			return result;
		}

		template <typename FUNC>
		void _alloc_buffer(::uint64_t const size, FUNC const &fn)
		{
			size_t donate = size;
			Buffer *buffer = nullptr;

			retry<Gpu::Session::Out_of_ram>(
			[&] () {
				retry<Gpu::Session::Out_of_caps>(
				[&] () {
					buffer =
						new (&_heap) Buffer(_gpu, size,
						                    _buffer_space);
				},
				[&] () {
					_gpu.upgrade_caps(2);
				});
			},
			[&] () {
				_gpu.upgrade_ram(donate);
			});

			if (buffer)
				fn(*buffer);
		}

		int _drm_lima_gem_create(drm_lima_gem_create &arg)
		{
			::uint64_t const size = arg.size;

			try {
				_alloc_buffer(size, [&](Buffer const &b) {
					arg.handle = b.id().value;
				});
				return 0;
			} catch (...) {
				return -1;
			}
		}

		int _drm_lima_gem_submit(drm_lima_gem_submit &arg)
		{
			Gpu_context::Id_space::Id ctx_id { .value = arg.ctx };

			Syncobj::Id_space::Id syncobj_id { .value = arg.out_sync };

			bool result = false;
			_syncobj_space.apply<Syncobj>(syncobj_id, [&] (Syncobj &sync_obj) {

				_gpu_context_space.apply<Gpu_context>(ctx_id, [&] (Gpu_context &gc) {

					size_t const payload_size = Lima::get_payload_size(arg);
					if (payload_size > EXEC_BUFFER_SIZE) {
						Genode::error(__func__, ": exec buffer too small (",
									  (unsigned)EXEC_BUFFER_SIZE, ") needed ", payload_size);
						return;
					}

					/*
					 * Copy each array flat to the exec buffer and adjust the
					 * addresses in the submit object.
					 */
					char *local_exec_buffer = (char*)_exec_buffer->mmap_addr();
					Genode::memset(local_exec_buffer, 0, EXEC_BUFFER_SIZE);
					Lima::serialize(&arg, local_exec_buffer);

					try {
						Gpu::Connection &gpu = gc.gpu();

						Gpu::Sequence_number const seqno =
							gpu.exec_buffer(_exec_buffer->id(), EXEC_BUFFER_SIZE);

						sync_obj.adopt(gc, seqno);

						result = true;
					} catch (Gpu::Session::Invalid_state) { }
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
				Gpu_context &ctx = _create_ctx();

				arg.id = ctx.id();
				return 0;
			} catch (... /* intentional catch-all ... */) {
				/* ... as the lima GPU driver will not throw */
			}
			return -1;
		}

		int _drm_lima_ctx_free(drm_lima_ctx_free &arg)
		{
			Gpu_context::Id_space::Id id { .value = arg.id };

			bool result = false;
			auto free_ctx = [&] (Gpu_context &ctx) {
				::close(ctx.fd);
				Genode::destroy(_heap, &ctx);
				result = true;
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
			return _apply_handle(gem_close.handle,
				[&] (Gpu::Buffer &b) {
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
				arg.handle = (uint32_t)obj->id();
				return 0;
			} catch (... /* XXX which exceptions can occur? */) { }
			return -1;
		}

		int _drm_syncobj_destroy(drm_syncobj_destroy &arg)
		{
			Syncobj::Id_space::Id id { .value = arg.handle };

			bool result = false;
			_syncobj_space.apply<Syncobj>(id, [&] (Syncobj &obj) {
				Genode::destroy(_heap, &obj);
				result = true;
			});
			return result ? 0 : -1;
		}

		int _drm_syncobj_handle_to_fd(drm_syncobj_handle &arg)
		{
			arg.fd = arg.handle + SYNC_FD;
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

		static constexpr int const SYNC_FD { 384 };

		Call()
		{
			try {
				_exec_buffer.construct(_gpu,
				                       (size_t)EXEC_BUFFER_SIZE,
				                       _buffer_space);
			} catch (...) {
				throw Gpu::Session::Invalid_state();
			}
			if (!_exec_buffer->mmap(_env))
				throw Gpu::Session::Invalid_state();
		}

		~Call()
		{
			while (_gpu_context_space.apply_any<Gpu_context>([&] (Gpu_context &ctx) {
				Genode::destroy(_heap, &ctx); })) { ; }

			while (_syncobj_space.apply_any<Syncobj>([&] (Syncobj &obj) {
				Genode::destroy(_heap, &obj); })) { ; }
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
	int const handle = fd - Lima::Call::SYNC_FD;
	_drm->wait_for_syncobj((unsigned)handle);
	return 0;
}
