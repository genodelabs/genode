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
#include <base/debug.h>
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

		/*****************
		 ** Gpu session **
		 *****************/

		Gpu::Connection _gpu_session { _env };
		Gpu::Info_etnaviv const &_gpu_info {
			*_gpu_session.attached_info<Gpu::Info_etnaviv>() };

		Id_space<Buffer> _buffer_space { };

		/*
		 * Play it safe, glmark2 apparently submits araound 110 KiB at
		 * some point.
		 */
		enum { EXEC_BUFFER_SIZE = 256u << 10 };
		Constructible<Buffer> _exec_buffer { };

		void _wait_for_completion(uint32_t fence)
		{
			Sequence_number const seqno { .value = fence };
			do {
				if (_gpu_session.complete(seqno))
					break;

				_env.ep().wait_and_dispatch_one_io_signal();
			} while (true);
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

		int _drm_etnaviv_gem_cpu_fini(drm_etnaviv_gem_cpu_fini &arg)
		{
			return _apply_handle(arg.handle, [&] (Buffer const &b) {
				_gpu_session.unmap_buffer(b.id());
			}) ? 0 : -1;
		}

		int _drm_etnaviv_gem_cpu_prep(drm_etnaviv_gem_cpu_prep &arg)
		{
			int res = -1;
			return _apply_handle(arg.handle, [&] (Buffer const &b) {

				Gpu::Mapping_attributes attrs { false, false };

				if (arg.op == ETNA_PREP_READ)
					attrs.readable = true;
				else

				if (arg.op == ETNA_PREP_WRITE)
					attrs.writeable = true;

				/*
				 * For now we ignore NOSYNC
				 */

				bool const to = arg.timeout.tv_sec != 0;
				if (to) {
					for (int i = 0; i < 100; i++) {
						Dataspace_capability const map_cap =
						_gpu_session.map_buffer(b.id(), false, attrs);
						if (map_cap.valid()) {
							res = 0;
							break;
						}
					}
				}
				else {
					Dataspace_capability const map_cap =
						_gpu_session.map_buffer(b.id(), false, attrs);
					if (map_cap.valid())
						res = 0;
				}
			}) ? res : -1;
		}

		int _drm_etnaviv_gem_info(drm_etnaviv_gem_info &arg)
		{
			return _apply_handle(arg.handle,
				[&] (Buffer &b) {
					if (!b.mmap(_env))
						return;
					arg.offset = reinterpret_cast<::uint64_t>(b.mmap_addr());
				}) ? 0 : -1;
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
						new (&_heap) Buffer(_gpu_session, size,
						                    _buffer_space);
				},
				[&] () {
					_gpu_session.upgrade_caps(2);
				});
			},
			[&] () {
				_gpu_session.upgrade_ram(donate);
			});

			if (buffer)
				fn(*buffer);
		}

		int _drm_etnaviv_gem_new(drm_etnaviv_gem_new &arg)
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

		int _drm_etnaviv_gem_submit(drm_etnaviv_gem_submit &arg)
		{
			size_t const payload_size = Etnaviv::get_payload_size(arg);
			if (payload_size > EXEC_BUFFER_SIZE) {
				Genode::error(__func__, ": exec buffer too small (",
				              (unsigned)EXEC_BUFFER_SIZE, ") needed ", payload_size);
				return -1;
			}

			/*
			 * Copy each array flat to the exec buffer and adjust the
			 * addresses in the submit object.
			 */
			char *local_exec_buffer = (char*)_exec_buffer->mmap_addr();
			Genode::memset(local_exec_buffer, 0, EXEC_BUFFER_SIZE);
			Etnaviv::serialize(&arg, local_exec_buffer);

			try {
				Genode::uint64_t const pending_exec_buffer =
					_gpu_session.exec_buffer(_exec_buffer->id(),
					                         EXEC_BUFFER_SIZE).value;
				arg.fence = pending_exec_buffer & 0xffffffffu;
				return 0;
			} catch (Gpu::Session::Invalid_state) { }

			return -1;
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
			return _apply_handle(gem_close.handle,
				[&] (Gpu::Buffer &b) {
					destroy(_heap, &b);
				}) ? 0 : -1;
		}

		int _drm_version(drm_version &version)
		{
			static char buffer[1] = { '\0' };

			version.version_major = 1;
			version.version_minor = 3;
			version.version_patchlevel = 0;
			version.name_len = 0;
			version.name = buffer;
			version.date_len = 0;
			version.date = buffer;
			version.desc_len = 0;
			version.desc = buffer;

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

		Call()
		{
			try {
				_exec_buffer.construct(_gpu_session,
				                       (size_t)EXEC_BUFFER_SIZE,
				                       _buffer_space);
			} catch (...) {
				throw Gpu::Session::Invalid_state();
			}
			if (!_exec_buffer->mmap(_env))
				throw Gpu::Session::Invalid_state();
		}

		~Call() { }

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
