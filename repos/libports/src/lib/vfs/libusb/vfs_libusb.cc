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
#include <util/xml_node.h>
#include <vfs/file_system_factory.h>
#include <vfs/single_file_system.h>

using namespace Genode;


/*
 * These functions are implemented in the Genode backend of libusb.
 */
extern void libusb_genode_backend_init(Env&, Allocator&, Signal_context_capability);
extern bool libusb_genode_backend_signaling;

class Libusb_file_system : public Vfs::Single_file_system
{
	private:

		Vfs::Env &_env;

		class Libusb_vfs_handle : public Single_vfs_handle
		{
			private:

				Env            &_env;
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
				                  Env               &env,
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

		Libusb_file_system(Vfs::Env &env, Xml_node config)
		:
			Single_file_system(Vfs::Node_type::CONTINUOUS_FILE, name(),
			                   Vfs::Node_rwx::ro(), config),
			_env(env) { }

		~Libusb_file_system() { }

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


struct Libusb_factory : Vfs::File_system_factory
{
	Vfs::File_system *create(Vfs::Env &env, Xml_node node) override
	{
		return new (env.alloc()) Libusb_file_system(env, node);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Libusb_factory factory;
	return &factory;
}
