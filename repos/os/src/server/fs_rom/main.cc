/*
 * \brief  Service that provides files of a file system as ROM sessions
 * \author Norman Feske
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rom_session/rom_session.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <os/path.h>
#include <root/component.h>
#include <base/component.h>
#include <base/session_label.h>
#include <util/arg_string.h>
#include <base/heap.h>
#include <base/log.h>


using namespace Genode;

/*****************
 ** ROM service **
 *****************/

/**
 * A 'Rom_session_component' exports a single file of the file system
 */
class Rom_session_component : public Genode::Rpc_object<Genode::Rom_session>
{
	private:

		Genode::Env          &_env;

		File_system::Session &_fs;

		enum { PATH_MAX_LEN = 512 };
		typedef Genode::Path<PATH_MAX_LEN> Path;

		/**
		 * Name of requested file, interpreted at path into the file system
		 */
		Path const _file_path;

		/**
		 * Handle of associated file
		 */
		File_system::File_handle _file_handle;

		/**
		 * Size of current version of the file
		 */
		File_system::file_size_t _file_size = 0;

		/**
		 * Handle of currently watched compound directory
		 *
		 * The compund directory is watched only if the requested file could
		 * not be looked up.
		 */
		File_system::Node_handle _compound_dir_handle;

		/**
		 * Dataspace exposed as ROM module to the client
		 */
		Genode::Ram_dataspace_capability _file_ds;

		/**
		 * Signal destination for ROM file changes
		 */
		Genode::Signal_context_capability _sigh;

		/**
		 * Signal-handling function called by the main thread the compound
		 * directory changed.
		 */
		void _dir_changed()
		{
			Genode::log("detected directory change");
			if (_sigh.valid())
				Genode::Signal_transmitter(_sigh).submit();
		}

		/**
		 * Handler that is called each time when the requested file is not
		 * yet available and the compound directory changes
		 *
		 * The change of the compound directory bears the chance that the
		 * requested file re-appears. So we inform the client about a ROM
		 * module change and thereby give it a chance to call 'dataspace()' in
		 * response.
		 */
		Genode::Signal_handler<Rom_session_component> _dir_change_handler
			{ _env.ep(), *this, &Rom_session_component::_dir_changed };

		/**
		 * Open compound directory of specified file
		 *
		 * \param walk_up  If set to true, the function tries to walk up the
		 *                 hierarchy towards the root and returns the first
		 *                 existing directory on the way. If set to false, the
		 *                 function returns the immediate compound directory.
		 */
		File_system::Dir_handle _open_compound_dir(File_system::Session &fs,
		                                           Path const &path,
		                                           bool walk_up)
		{
			using namespace File_system;

			Genode::Path<PATH_MAX_LEN> dir_path(path.base());

			while (!path.equals("/")) {

				dir_path.strip_last_element();

				try { return fs.dir(dir_path.base(), false); }

				catch (Invalid_handle)    { Genode::error(_file_path, ": invalid_handle"); }
				catch (Invalid_name)      { Genode::error(_file_path, ": invalid_name"); }
				catch (Lookup_failed)     { Genode::error(_file_path, ": lookup_failed"); }
				catch (Permission_denied) { Genode::error(_file_path, ": permission_denied"); }
				catch (Name_too_long)     { Genode::error(_file_path, ": name_too_long"); }
				catch (No_space)          { Genode::error(_file_path, ": no_space"); }

				/*
				 * If the directory could not be opened, walk up the hierarchy
				 * towards the root and try again.
				 */
				if (!walk_up) break;
			}
			return Dir_handle(); /* invalid */
		}

		/**
		 * Open file with specified name at the file system
		 */
		File_system::File_handle _open_file(File_system::Session &fs,
		                                    Path const &path)
		{
			using namespace File_system;

			File_system::File_handle file_handle;

			try {

				Dir_handle dir = _open_compound_dir(fs, path, false);
				Handle_guard guard(fs, dir);

				/* open file */
				Genode::Path<PATH_MAX_LEN> file_name(path.base());
				file_name.keep_only_last_element();
				file_handle = fs.file(dir, file_name.base() + 1,
				                      File_system::READ_ONLY, false);
			}
			catch (Invalid_handle)    { Genode::error(_file_path, ": Invalid_handle"); }
			catch (Invalid_name)      { Genode::error(_file_path, ": invalid_name"); }
			catch (Lookup_failed)     { Genode::error(_file_path, ": lookup_failed"); }

			return file_handle;
		}

		void _register_for_compound_dir_changes()
		{
			/* forget about the previously watched compound directory */
			if (_compound_dir_handle.valid())
				_fs.close(_compound_dir_handle);

			_compound_dir_handle = _open_compound_dir(_fs, _file_path, true);

			/* register for changes in compound directory */
			if (_compound_dir_handle.valid())
				_fs.sigh(_compound_dir_handle, _dir_change_handler);
			else
				Genode::warning("could not track compound dir, giving up");
		}

		/**
		 * Initialize '_file_ds' dataspace with file content
		 */
		void _update_dataspace()
		{
			using namespace File_system;

			/*
			 * On each repeated call of this function, the dataspace is
			 * replaced with a new one that contains the most current file
			 * content. The dataspace is re-allocated if the new version
			 * of the file has become bigger.
			 */
			{
				File_handle const file_handle = _open_file(_fs, _file_path);
				if (file_handle.valid()) {
					File_system::file_size_t const new_file_size =
						_fs.status(file_handle).size;

					if (_file_ds.valid() && (new_file_size > _file_size)) {
						_env.ram().free(_file_ds);

						/* mark as invalid */
						_file_ds = Ram_dataspace_capability();
						_file_size = 0;
					}
				}
				_fs.close(file_handle);
			}

			/* close and then re-open the file */
			if (_file_handle.valid())
				_fs.close(_file_handle);

			_file_handle = _open_file(_fs, _file_path);

			/*
			 * If we got the file, we can stop paying attention to the
			 * compound directory.
			 */
			if (_file_handle.valid() && _compound_dir_handle.valid())
				_fs.close(_compound_dir_handle);

			/* register for file changes */
			if (_sigh.valid() && _file_handle.valid())
				_fs.sigh(_file_handle, _sigh);

			size_t const file_size = _file_handle.valid()
			                       ? _fs.status(_file_handle).size : 0;

			/* allocate new RAM dataspace according to file size */
			if (file_size > 0) {
				try {
					if (!_file_ds.valid()) {
						_file_ds = _env.ram().alloc(file_size);
						_file_size = file_size;
					}
				} catch (...) {
					Genode::error("couldn't allocate memory for file, empty result");
					_file_ds = Ram_dataspace_capability();
					return;
				}
			}

			if (!_file_ds.valid()) {
				_register_for_compound_dir_changes();
				return;
			}

			/* map dataspace locally */
			void * const dst_addr = _env.rm().attach(_file_ds);

			/* read content from file */
			read(_fs, _file_handle, dst_addr, file_size);

			/* unmap dataspace */
			_env.rm().detach(dst_addr);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param fs        file-system session to read the file from
		 * \param filename  requested file name
		 * \param sig_rec   signal receiver used to get notified about changes
		 *                  within the compound directory (in the case when
		 *                  the requested file could not be found at session-
		 *                  creation time)
		 */
		Rom_session_component(Genode::Env &env,
		                      File_system::Session &fs, const char *file_path)
		:
			_env(env), _fs(fs), _file_path(file_path),
			_file_handle(_open_file(_fs, _file_path))
		{
			if (!_file_handle.valid())
				_register_for_compound_dir_changes();
		}

		/**
		 * Destructor
		 */
		~Rom_session_component()
		{
			/* close re-open the file */
			if (_file_handle.valid())
				_fs.close(_file_handle);

			if (_compound_dir_handle.valid())
				_fs.close(_compound_dir_handle);

			/* close file */
			_env.ram().free(_file_ds);
		}

		/**
		 * Return dataspace with up-to-date content of file
		 */
		Genode::Rom_dataspace_capability dataspace()
		{
			_update_dataspace();
			Genode::Dataspace_capability ds = _file_ds;
			return Genode::static_cap_cast<Genode::Rom_dataspace>(ds);
		}

		void sigh(Genode::Signal_context_capability sigh)
		{
			_sigh = sigh;
			if (_file_handle.valid())
				_fs.sigh(_file_handle, _sigh);
		}
};


class Rom_root : public Genode::Root_component<Rom_session_component>
{
	private:

		Genode::Env          &_env;
		Genode::Heap          _heap { _env.ram(), _env.rm() };
		Genode::Allocator_avl _fs_tx_block_alloc { &_heap };

		/* open file-system session */
		File_system::Connection _fs { _env, _fs_tx_block_alloc };

		Rom_session_component *_create_session(const char *args)
		{
			Genode::Session_label const label = label_from_args(args);
			Genode::Session_label const module_name = label.last_element();

			Genode::log("request for ", label);

			/* create new session for the requested file */
			return new (md_alloc())
				Rom_session_component(_env, _fs, module_name.string());
		}

	public:

		/**
		 * Constructor
		 *
		 * \param  env         Component environment
		 * \param  md_alloc    meta-data allocator used for ROM sessions
		 */
		Rom_root(Genode::Env       &env,
		         Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Rom_session_component>(env.ep(), md_alloc),
			_env(env)
		{
			env.parent().announce(env.ep().manage(*this));
		}
};

Genode::size_t Component::stack_size() { return 2*1024*sizeof(long); }

void Component::construct(Genode::Env &env)
{
	static Sliced_heap sliced_heap(env.ram(), env.rm());
	static Rom_root inst(env, sliced_heap);
}
