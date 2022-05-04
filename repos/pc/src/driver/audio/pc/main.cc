/*
 * \brief  Linux sound driver (Intel HDA)
 * \author Sebastian Sumpf
 * \date   2022-05-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <os/vfs.h>

/* lx emul/kit includes */
#include <lx_emul/init.h>
#include <lx_kit/env.h>
#include <lx_kit/init.h>
#include <lx_kit/initial_config.h>

/* local includes */
#include "audio.h"
#include "firmware.h"

using namespace Genode;


extern "C" void lx_emul_module_params(void);


extern "C" int lx_emul_acpi_table(const char * const name, void *ctx,
                                  void (*fn) (void *ctx,
                                              unsigned long addr,
                                              unsigned long size))
{
	using namespace Lx_kit;
	using namespace Genode;

	int found = 0;
	env().devices.for_each([&] (Device &d) {
		if (d.name() != name)
		return;

		found = 1;
		d.for_each_io_mem([&] (Device::Io_mem &io_mem) {
			log("Found NHLT: ", Hex(io_mem.addr), " size: ", io_mem.size);
			fn(ctx, io_mem.addr, io_mem.size); });
	});

	return found;
}


struct Vfs_request_handler : Lx_kit::Firmware_request_handler
{
	Env  &_env;
	Heap &_heap;

	Attached_rom_dataspace _config_rom { _env, "config" };

	Vfs::Simple_env _vfs_env = _config_rom.node().with_sub_node("vfs",
		[&] (Node const &config) -> Vfs::Simple_env {
			return { _env, _heap, config }; },
		[&] () -> Vfs::Simple_env {
			warning("VFS not configured, firmware loading non-functional");
			return { _env, _heap, Node() }; } );

	static size_t query_file_length(Vfs::Simple_env &vfs_env, char const *file_path)
	{
		using DS = Vfs::Directory_service;
		using SR = DS::Stat_result;

		size_t length = 0;

		DS::Stat stat { };
		if (vfs_env.root_dir().stat(file_path, stat) == SR::STAT_OK) {
			length = (size_t)stat.size;
		}

		return length;
	}

	static size_t read_file(Vfs::Simple_env       &vfs_env,
	                        char             const *file_path,
	                        Byte_range_ptr   const &dst)
	{
		try {
			Readonly_file ro_file(Directory(vfs_env), file_path);
			return ro_file.read(dst);
		} catch (...) { }

		return 0;
	}

	Signal_handler<Vfs_request_handler> _handler;

	void _handle_request()
	{
		using Fw_path = Genode::String<128>;
		using namespace Lx_kit;

		Firmware_request *request_ptr = firmware_get_request();
		if (!request_ptr)
			return;

		Firmware_request &request = *request_ptr;

		request.success = false;

		switch (request.state) {
		case Firmware_request::State::PROBING:
		{
			Fw_path const path { "/firmware/", request.name };

			size_t const length = query_file_length(_vfs_env, path.string());

			request.fw_len  = length;
			request.success = length != 0;

			request.submit_response();
			break;
		}
		case Firmware_request::State::REQUESTING:
		{
			Fw_path const path { "/firmware/", request.name };

			size_t const bytes = read_file(_vfs_env, path.string(),
			                               Byte_range_ptr { request.dst,
			                                                request.dst_len });

			request.success = bytes == request.dst_len;

			request.submit_response();
			break;
		}
		case Firmware_request::State::INVALID:             break;
		case Firmware_request::State::PROBING_COMPLETE:    break;
		case Firmware_request::State::REQUESTING_COMPLETE: break;
		}
	}

	Vfs_request_handler(Genode::Env &env, Genode::Heap &heap)
	:
		_env     { env },
		_heap    { heap },
		_handler { env.ep(), *this, &Vfs_request_handler::_handle_request } { }

	void submit_request() override {
		_handler.local_submit(); }
};


struct Main
{
	Env &env;
	Heap heap { env.ram(), env.rm() };

	Signal_handler<Main> scheduler_handler { env.ep(), *this, &Main::handle_scheduler };

	Vfs_request_handler _request_handler { env, heap };

	Main(Env & env) : env(env)
	{
		Lx_kit::initialize(env, scheduler_handler);

		Lx_kit::firmware_establish_handler(_request_handler);

		lx_emul_module_params();

		genode_audio_init(genode_env_ptr(env),
		                  genode_allocator_ptr(heap));

		lx_emul_start_kernel(nullptr);
	};

	void handle_scheduler()
	{
		Lx_kit::env().scheduler.execute();
	}
};


void Component::construct(Env & env)
{
	static Main main(env);
}
