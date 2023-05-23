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
#include <usb_session/connection.h>
#include <util/xml_node.h>
#include <vfs/file_system_factory.h>
#include <vfs/single_file_system.h>


/*
 * This function is implemented in the Genode backend
 * of libusb.
 */
extern void libusb_genode_usb_connection(Usb::Connection *);

class Libusb_file_system : public Vfs::Single_file_system
{
	private:

		Vfs::Env &_env;

		class Libusb_vfs_handle : public Single_vfs_handle
		{
			private:

				Genode::Env           &_env;
				Vfs::Env::User        &_vfs_user;
				Genode::Allocator_avl  _alloc_avl;

				Genode::Io_signal_handler<Libusb_vfs_handle> _state_changed_handler {
					_env.ep(), *this, &Libusb_vfs_handle::_handle_state_changed };

				void _handle_state_changed()
				{
					/*
					 * The handler is installed only to receive state-change
					 * signals from the USB connection using the 'Usb_device'
					 * constructor.
					 */
				}

				Usb::Connection _usb_connection {
					_env, &_alloc_avl, "usb_device", 1024*1024, _state_changed_handler };

				Genode::Io_signal_handler<Libusb_vfs_handle> _ack_avail_handler {
					_env.ep(), *this, &Libusb_vfs_handle::_handle_ack_avail };

				void _handle_ack_avail()
				{
					_vfs_user.wakeup_vfs_user();
				}

			public:

				Libusb_vfs_handle(Directory_service &ds,
				                  File_io_service   &fs,
				                  Genode::Allocator &alloc,
				                  Genode::Env       &env,
				                  Vfs::Env::User    &vfs_user)
				:
					Single_vfs_handle(ds, fs, alloc, 0),
					_env(env), _vfs_user(vfs_user), _alloc_avl(&alloc)
				{
					_usb_connection.tx_channel()->sigh_ack_avail(_ack_avail_handler);

					Genode::log("libusb: waiting until device is plugged...");
					while (!_usb_connection.plugged())
						_env.ep().wait_and_dispatch_one_io_signal();
					Genode::log("libusb: device is plugged");

					libusb_genode_usb_connection(&_usb_connection);
				}

				bool read_ready() const override
				{
					auto &nonconst_this = const_cast<Libusb_vfs_handle &>(*this);
					return nonconst_this._usb_connection.source()->ack_avail();
				}

				Read_result read(Genode::Byte_range_ptr const &, Genode::size_t &) override
				{
					return READ_ERR_IO;
				}

				bool write_ready() const override
				{
					return true;
				}

				Write_result write(Genode::Const_byte_range_ptr const &, Genode::size_t &) override
				{
					return WRITE_ERR_IO;
				}
		};

	public:

		Libusb_file_system(Vfs::Env &env,
		                   Genode::Xml_node config)
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

		Open_result open(char const         *path, unsigned,
		                 Vfs::Vfs_handle   **out_handle,
		                 Genode::Allocator  &alloc) override
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
	Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node node) override
	{
		return new (env.alloc()) Libusb_file_system(env, node);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Libusb_factory factory;
	return &factory;
}
