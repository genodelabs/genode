/*
 * \brief  Report server that writes reports to file-systems
 * \author Emery Hemingway
 * \date   2017-05-19
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <vfs/simple_env.h>
#include <os/path.h>
#include <report_session/report_session.h>
#include <root/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/session_label.h>
#include <base/heap.h>
#include <base/component.h>
#include <util/arg_string.h>

namespace Fs_report {

	using namespace Genode;
	using namespace Report;
	using namespace Vfs;

	class  Session_component;
	class  Root;
	struct Main;

	typedef Genode::Path<Session_label::capacity()> Path;

	static bool create_parent_dir(Vfs::Directory_service &vfs, Path const &child,
	                              Genode::Allocator &alloc)
	{
		typedef Vfs::Directory_service::Opendir_result Opendir_result;

		Path parent = child;
		parent.strip_last_element();
		if (parent == "/")
			return true;

		Vfs_handle *dir_handle;
		Opendir_result res = vfs.opendir(parent.base(), true, &dir_handle, alloc);
		if (res == Opendir_result::OPENDIR_ERR_LOOKUP_FAILED) {
			if (!create_parent_dir(vfs, parent, alloc))
				return false;
			res = vfs.opendir(parent.base(), true, &dir_handle, alloc);
		}

		switch (res) {
		case Opendir_result::OPENDIR_OK:
			vfs.close(dir_handle);
			return true;
		case Opendir_result::OPENDIR_ERR_NODE_ALREADY_EXISTS:
			 return true;
		default:
			return false;
		}
	}

}


class Fs_report::Session_component : public Genode::Rpc_object<Report::Session>
{
	private:

		Genode::Entrypoint     &_ep;
		Genode::Allocator      &_alloc;
		Vfs::File_system       &_vfs;

		Attached_ram_dataspace _ds;
		Path                   _path { };

		file_size _file_size = 0;
		bool      _success   = true;

		struct Open_failed { };

		template <typename FN> void _file_op(FN const &fn)
		{
			typedef Vfs::Directory_service::Open_result Open_result;

			Vfs_handle *handle;
			Open_result res = _vfs.open(_path.base(),
			                            Directory_service::OPEN_MODE_WRONLY,
			                            &handle, _alloc);

			/* try to create file if not accessible */
			if (res == Open_result::OPEN_ERR_UNACCESSIBLE) {
				res = _vfs.open(_path.base(),
				                Directory_service::OPEN_MODE_WRONLY |
				                Directory_service::OPEN_MODE_CREATE,
				                &handle, _alloc);
			}

			if (res != Open_result::OPEN_OK) {
				error("failed to open '", _path, "' res=", (int)res);
				throw Open_failed();
			}

			fn(handle);

			/* sync file operations before close */
			while (!handle->fs().queue_sync(handle))
				_ep.wait_and_dispatch_one_io_signal();

			while (handle->fs().complete_sync(handle) == Vfs::File_io_service::SYNC_QUEUED)
				_ep.wait_and_dispatch_one_io_signal();

			handle->close();
		}

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

	public:

		Session_component(Genode::Env                 &env,
		                  Genode::Allocator           &alloc,
		                  Vfs::File_system            &vfs,
		                  Genode::Session_label const &label,
		                  size_t                       buffer_size)
		:
			_ep(env.ep()), _alloc(alloc), _vfs(vfs),
			_ds(env.ram(), env.rm(), buffer_size),
			_path(path_from_label<Path>(label.string()))
		{
			create_parent_dir(_vfs, _path, _alloc);
		}

		~Session_component() { }

		Dataspace_capability dataspace() override { return _ds.cap(); }

		void submit(size_t const length) override
		{
			auto fn = [&] (Vfs_handle *handle) {

				typedef Vfs::File_io_service::Write_result Write_result;

				if (_file_size != length)
					handle->fs().ftruncate(handle, length);

				size_t offset = 0;
				while (offset < length) {
					file_size n = 0;

					handle->seek(offset);
					Write_result res = handle->fs().write(
						handle, _ds.local_addr<char const>() + offset,
						length - offset, n);

					if (res != Write_result::WRITE_OK) {
						/* do not spam the log */
						if (_success)
							error("failed to write report to '", _path, "'");
						_file_size = 0;
						_success = false;
						return;
					}

					offset += n;
				}

				_file_size = length;
				_success = true;
			};

			try { _file_op(fn); } catch (...) { }
		}

		void response_sigh(Genode::Signal_context_capability) override { }

		size_t obtain_response() override { return 0; }
};


class Fs_report::Root : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env  &_env;
		Genode::Heap  _heap { &_env.ram(), &_env.rm() };

		Genode::Attached_rom_dataspace _config_rom { _env, "config" };

		Genode::Xml_node vfs_config()
		{
			try { return _config_rom.xml().sub_node("vfs"); }
			catch (...) {
				Genode::error("VFS not configured");
				_env.parent().exit(~0);
				throw;
			}
		}

		Vfs::Simple_env _vfs_env { _env, _heap, vfs_config() };

		Genode::Signal_handler<Root> _config_dispatcher {
			_env.ep(), *this, &Root::_config_update };

		void _config_update()
		{
			_config_rom.update();
			_vfs_env.root_dir().apply_config(vfs_config());
		}

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			/* read label from session arguments */
			Session_label const label = label_from_args(args);

			/* read RAM donation from session arguments */
			size_t const ram_quota =
				Arg_string::find_arg(args, "ram_quota").aligned_size();
			/* read report buffer size from session arguments */
			size_t const buffer_size =
				Arg_string::find_arg(args, "buffer_size").aligned_size();

			size_t session_size =
				max((size_t)4096, sizeof(Session_component)) +
				buffer_size;

			if (session_size > ram_quota) {
				error("insufficient 'ram_quota' from '", label, "' "
				      "got ", ram_quota, ", need ", session_size);
				throw Insufficient_ram_quota();
			}

			return new (md_alloc())
				Session_component(_env, _heap, _vfs_env.root_dir(), label, buffer_size);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(env.ep(), md_alloc),
			_env(env)
		{ }
};


struct Fs_report::Main
{
	Genode::Env &env;

	Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root root { env, sliced_heap };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
};

void Component::construct(Genode::Env &env)
{
	static Fs_report::Main main(env);
}
