/*
 * \brief  libusb file system
 * \author Christian Prochaska
 * \date   2022-01-31
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/allocator_avl.h>
#include <vfs/file_system_factory.h>
#include <vfs/single_file_system.h>

namespace Vfs_libusb {

	using namespace Genode;
	using namespace Genode::Vfs;

	class File_system;
}


/*
 * These functions are implemented in the Genode backend of libusb.
 */
extern void libusb_genode_backend_init(Genode::Env &, Genode::Allocator &,
                                       Genode::Signal_context_capability);

extern bool libusb_genode_backend_signaling;

class Vfs_libusb::File_system : public Vfs::Single_file_system
{
	private:

		Vfs::Env &_env;

		class Libusb_vfs_handle : public Single_vfs_handle
		{
			private:

				Genode::Env    &_env;
				Vfs::Env::User &_vfs_user;

				Io_signal_handler<Libusb_vfs_handle> _handler {
					_env.ep(), *this, &Libusb_vfs_handle::_handle };

				void _handle()
				{
					libusb_genode_backend_signaling = true;
					_vfs_user.wakeup_vfs_user();
				}

			public:

				Libusb_vfs_handle(Directory_service &ds,
				                  File_io_service   &fs,
				                  Allocator         &alloc,
				                  Genode::Env       &env,
				                  Vfs::Env::User    &vfs_user)
				:
					Single_vfs_handle(ds, fs, alloc, 0),
					_env(env), _vfs_user(vfs_user)
				{
					log("libusb: waiting until device is plugged...");
					libusb_genode_backend_init(env, alloc, _handler);
					log("libusb: device is plugged");
				}

				bool read_ready() const override {
					return libusb_genode_backend_signaling; }

				Read_result read(Byte_range_ptr const &, size_t &) override {
					return READ_ERR_IO; }

				bool write_ready() const override {
					return true; }

				Write_result write(Const_byte_range_ptr const &,
				                   size_t &) override {
					return WRITE_ERR_IO; }
		};

	public:

		File_system(Vfs::Env &env, Node const &config)
		:
			Single_file_system(Vfs::Node_type::CONTINUOUS_FILE, name(),
			                   Vfs::Node_rwx::ro(), config),
			_env(env) { }

		~File_system() { }

		static char const *name()   { return "libusb"; }
		char const *type() override { return "libusb"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle = new (alloc)
				Libusb_vfs_handle(*this, *this, alloc, _env.env(), _env.user());
			return OPEN_OK;
		}
};


extern "C" Genode::Vfs::File_system_factory *vfs_file_system_factory(void)
{
	using namespace Genode;

	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Node const &node) override
		{
			return new (env.alloc()) Vfs_libusb::File_system(env, node);
		}
	};

	static Factory factory;
	return &factory;
}
