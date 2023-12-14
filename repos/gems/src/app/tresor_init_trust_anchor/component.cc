/*
 * \brief  Integration of the Tresor block encryption
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <block_session/connection.h>
#include <os/path.h>
#include <vfs/dir_file_system.h>
#include <vfs/file_system_factory.h>
#include <vfs/simple_env.h>

/* tresor includes */
#include <tresor/types.h>

/* vfs tresor trust anchor includes */
#include <io_job.h>

using namespace Genode;

class Main : Vfs::Env::User
{
	public:

		struct Missing_config_attribute : Genode::Exception { };

	private:

		Env   &_env;
		Heap  _heap { _env.ram(), _env.rm() };

		Attached_rom_dataspace _config_rom { _env, "config" };

		Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };

		Vfs::File_system &_vfs { _vfs_env.root_dir() };

		using String_path = Genode::String<256>;

		static String_path _config_ta_dir(Xml_node const &node)
		{
			String_path const path =
				node.attribute_value("trust_anchor_dir", String_path());

			if (!path.valid()) {
				error("missing mandatory 'trust_anchor_dir' config attribute");
				throw Missing_config_attribute();
			}
			return path;
		}

		enum class State { WRITE, READ, };
		State _state { State::WRITE };

		struct File
		{
			struct Could_not_open_file : Genode::Exception { };

			struct Completed
			{
				bool complete;
				bool success;
			};

			File(File const &) = delete;
			File &operator=(File const&) = delete;

			Vfs::File_system &_vfs;
			Vfs::Vfs_handle  *_vfs_handle;

			Genode::Constructible<Util::Io_job> _io_job { };
			Util::Io_job::Buffer                _io_buffer { };

			Tresor::Passphrase _initialize_file_buf { };

			File(char          const *base_path,
				 char          const *name,
				 Vfs::File_system    &vfs,
				 Genode::Allocator   &alloc)
			:
				_vfs(vfs), _vfs_handle(nullptr)
			{
				using Result = Vfs::Directory_service::Open_result;

				Genode::Path<256> file_path = base_path;
				file_path.append_element(name);

				Result const res =
					_vfs.open(file_path.string(),
					          Vfs::Directory_service::OPEN_MODE_RDWR,
					          (Vfs::Vfs_handle **)&_vfs_handle, alloc);
				if (res != Result::OPEN_OK) {
					error("could not open '", file_path.string(), "'");
					throw Could_not_open_file();
				}
			}

			~File()
			{
				_vfs.close(_vfs_handle);
			}

			void write_passphrase(Tresor::Passphrase const &passphrase)
			{
				/* copy */
				_initialize_file_buf = passphrase;

				_io_buffer = {
					.base = const_cast<char *>(_initialize_file_buf.string()),
					.size = _initialize_file_buf.length()
				};

				_io_job.construct(*_vfs_handle, Util::Io_job::Operation::WRITE,
				                  _io_buffer, 0);
			}

			void queue_read()
			{
				_io_buffer = {
					.base = (char *)(_initialize_file_buf.string()),
					.size = _initialize_file_buf.length()
				};

				_io_job.construct(*_vfs_handle, Util::Io_job::Operation::READ,
				                  _io_buffer, 0);
			}

			void execute()
			{
				if (!_io_job.constructed()) {
					return;
				}

				_io_job->execute();
			}

			Completed write_complete()
			{
				return { _io_job->completed(), _io_job->succeeded() };
			}

			Completed read_complete()
			{
				return {
					_io_job->completed(),
					_io_job->succeeded() && _initialize_file_buf == "ok"};
			}

			void drop_io_job()
			{
				_io_job.destruct();
			}
		};

		Genode::Constructible<File> _init_file { };

		Genode::Signal_handler<Main> _io_handler {
			_env.ep(), *this, &Main::_handle_io };

		void _handle_io()
		{
			_init_file->execute();

			File::Completed result { false, false };

			switch (_state) {
			case State::WRITE:
				result = _init_file->write_complete();
				if (result.complete) {
					_init_file->drop_io_job();

					_state = State::READ;
					_init_file->queue_read();
				}
				break;
			case State::READ:
				result = _init_file->read_complete();
				if (result.complete) {
					_init_file->drop_io_job();
					_init_file.destruct();
					_env.parent().exit(result.success ? 0 : 1);
					return;
				}
				break;
			}

			_vfs_env.io().commit();
		}

		/**
		 * Vfs::Env::User interface
		 */
		void wakeup_vfs_user() override { _io_handler.local_submit(); }

	public:

		Main(Env &env) : _env(env)
		{
			Xml_node const &config { _config_rom.xml() };

			Tresor::Passphrase const passphrase =
				config.attribute_value("passphrase", Tresor::Passphrase());

			if (!passphrase.valid()) {
				error("mandatory 'passphrase' attribute missing");
				throw Missing_config_attribute();
			}

			String_path ta_dir = _config_ta_dir(_config_rom.xml());

			_init_file.construct(ta_dir.string(), "initialize",
			                     _vfs, _vfs_env.alloc());

			/* kick-off writing */
			_init_file->write_passphrase(passphrase.string());

			_handle_io();
		}
};


void Component::construct(Genode::Env &env)
{
	static Main main(env);
}
