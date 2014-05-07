/*
 * \brief  Service that provides files of a file system as ROM sessions
 * \author Norman Feske
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rom_session/rom_session.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <file_system_session/connection.h>
#include <util/arg_string.h>
#include <base/rpc_server.h>
#include <base/env.h>
#include <base/printf.h>
#include <os/path.h>


/*********************************************
 ** Utilities for accessing the file system **
 *********************************************/

/*
 * XXX The following generic utilities should be moved to a public place.
 *     They are based on those found in the 'libc_fs' plugin. We should
 *     unify them.
 */

namespace File_system {

	/**
	 * Collect pending packet acknowledgements, freeing the space occupied
	 * by the packet in the bulk buffer
	 *
	 * This function should be called prior enqueing new packets into the
	 * packet stream to free up space in the bulk buffer.
	 */
	static void collect_acknowledgements(Session::Tx::Source &source)
	{
		while (source.ack_avail())
			source.release_packet(source.get_acked_packet());
	}


	/**
	 * Read file content
	 */
	static inline size_t read(Session &fs, File_handle const &file_handle,
	                          void *dst, size_t count, off_t seek_offset = 0)
	{
		Session::Tx::Source &source = *fs.tx();

		size_t const max_packet_size = source.bulk_buffer_size() / 2;

		size_t remaining_count = count;

		while (remaining_count) {

			collect_acknowledgements(source);

			size_t const curr_packet_size = min(remaining_count, max_packet_size);

			Packet_descriptor
				packet(source.alloc_packet(curr_packet_size),
				       0,
				       file_handle,
				       File_system::Packet_descriptor::READ,
				       curr_packet_size,
				       seek_offset);

			/* pass packet to server side */
			source.submit_packet(packet);

			packet = source.get_acked_packet();

			size_t const read_num_bytes = min(packet.length(), curr_packet_size);

			/* copy-out payload into destination buffer */
			memcpy(dst, source.packet_content(packet), read_num_bytes);

			source.release_packet(packet);

			/* prepare next iteration */
			seek_offset += read_num_bytes;
			dst = (void *)((Genode::addr_t)dst + read_num_bytes);
			remaining_count -= read_num_bytes;

			/*
			 * If we received less bytes than requested, we reached the end
			 * of the file.
			 */
			if (read_num_bytes < curr_packet_size)
				break;
		}

		return count - remaining_count;
	}


	struct Handle_guard
	{
		private:

			Session     &_session;
			Node_handle  _handle;

		public:

			Handle_guard(Session &session, Node_handle handle)
			: _session(session), _handle(handle) { }

			~Handle_guard() { _session.close(_handle); }
	};
}


/*****************
 ** ROM service **
 *****************/

/**
 * A 'Rom_session_component' exports a single file of the file system
 */
class Rom_session_component : public Genode::Rpc_object<Genode::Rom_session>
{
	private:

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
		 * Handler for ROM file changes
		 */
		Genode::Lock                      _sigh_lock;
		Genode::Signal_context_capability _sigh;

		/**
		 * Dispatcher that is called each time when the requested file is not
		 * yet available and the compound directory changes
		 *
		 * The change of the compound directory bears the chance that the
		 * requested file re-appears. So we inform the client about a ROM
		 * module change and thereby give it a chance to call 'dataspace()' in
		 * response.
		 */
		Genode::Signal_dispatcher<Rom_session_component> _dir_change_dispatcher;

		/**
		 * Signal-handling function called by the main thread the compound
		 * directory changed.
		 *
		 * Note that this function is not executed in the context of the RPC
		 * entrypoint. Therefore, the access to '_sigh' is synchronized with
		 * the 'sigh()' function using '_sigh_lock'.
		 */
		void _dir_changed(unsigned)
		{
			Genode::Lock::Guard guard(_sigh_lock);

			PINF("detected directory change");
			if (_sigh.valid())
				Genode::Signal_transmitter(_sigh).submit();
		}

		/**
		 * Open compound directory of specified file
		 *
		 * \param walk_up  If set to true, the function tries to walk up the
		 *                 hierarchy towards the root and returns the first
		 *                 existing directory on the way. If set to false, the
		 *                 function returns the immediate compound directory.
		 */
		static File_system::Dir_handle _open_compound_dir(File_system::Session &fs,
		                                                  Path const &path,
		                                                  bool walk_up)
		{
			using namespace File_system;

			Genode::Path<PATH_MAX_LEN> dir_path(path.base());

			while (!path.equals("/")) {

				dir_path.strip_last_element();

				try { return fs.dir(dir_path.base(), false); }

				catch (Invalid_handle)    { PERR("Invalid_handle"); }
				catch (Invalid_name)      { PERR("Invalid_name"); }
				catch (Lookup_failed)     { PERR("Lookup_failed"); }
				catch (Permission_denied) { PERR("Permission_denied"); }
				catch (Name_too_long)     { PERR("Name_too_long"); }
				catch (No_space)          { PERR("No_space"); }

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
		static File_system::File_handle _open_file(File_system::Session &fs,
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
			catch (Invalid_handle)    { PERR("Invalid_handle"); }
			catch (Invalid_name)      { PERR("Invalid_name"); }
			catch (Lookup_failed)     { PERR("Lookup_failed"); }

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
				_fs.sigh(_compound_dir_handle, _dir_change_dispatcher);
			else
				PWRN("could not track compound dir, giving up");
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
			 * content.
			 */
			if (_file_ds.valid()) {
				env()->ram_session()->free(_file_ds);
				_file_ds = Ram_dataspace_capability();
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
					_file_ds = env()->ram_session()->alloc(file_size); }
				catch (...) {
					PERR("couldn't allocate memory for file, empty result\n");
					_file_ds = Ram_dataspace_capability();
					return;
				}
			}

			if (!_file_ds.valid()) {
				_register_for_compound_dir_changes();
				return;
			}

			/* map dataspace locally */
			void * const dst_addr = env()->rm_session()->attach(_file_ds);

			/* read content from file */
			read(_fs, _file_handle, dst_addr, file_size);

			/* unmap dataspace */
			env()->rm_session()->detach(dst_addr);
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
		Rom_session_component(File_system::Session &fs, const char *file_path,
		                      Genode::Signal_receiver &sig_reg)
		:
			_fs(fs), _file_path(file_path), _file_handle(_open_file(_fs, _file_path)),
			_dir_change_dispatcher(sig_reg, *this, &Rom_session_component::_dir_changed)
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
			Genode::env()->ram_session()->free(_file_ds);
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
			Genode::Lock::Guard guard(_sigh_lock);
			_sigh = sigh;
			if (_file_handle.valid())
				_fs.sigh(_file_handle, _sigh);
		}
};


class Rom_root : public Genode::Root_component<Rom_session_component>
{
	private:

		File_system::Session    &_fs;
		Genode::Signal_receiver &_sig_rec;

		Rom_session_component *_create_session(const char *args)
		{
			enum { FILENAME_MAX_LEN = 128 };
			char filename[FILENAME_MAX_LEN];
			Genode::Arg_string::find_arg(args, "filename")
				.string(filename, sizeof(filename), "");

			PINF("connection for file '%s' requested\n", filename);

			/* create new session for the requested file */
			return new (md_alloc()) Rom_session_component(_fs, filename, _sig_rec);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param  entrypoint  entrypoint to be used for ROM sessions
		 * \param  md_alloc    meta-data allocator used for ROM sessions
		 * \param  fs          file-system session
		 */
		Rom_root(Genode::Rpc_entrypoint  &entrypoint,
		         Genode::Allocator       &md_alloc,
		         File_system::Session    &fs,
		         Genode::Signal_receiver &sig_rec)
		:
			Genode::Root_component<Rom_session_component>(&entrypoint, &md_alloc),
			_fs(fs), _sig_rec(sig_rec)
		{ }
};


int main(void)
{
	using namespace Genode;

	/* open file-system session */
	static Genode::Allocator_avl fs_tx_block_alloc(env()->heap());
	static File_system::Connection fs(fs_tx_block_alloc);

	/* connection to capability service needed to create capabilities */
	static Cap_connection cap;

	/* creation of the entrypoint and the root interface */
	static Sliced_heap sliced_heap(env()->ram_session(),
	                               env()->rm_session());

	/* receiver of directory-change signals */
	static Signal_receiver sig_rec;

	enum { STACK_SIZE = 8*1024 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fs_rom_ep");
	static Rom_root rom_root(ep, sliced_heap, fs, sig_rec);

	/* announce server*/
	env()->parent()->announce(ep.manage(&rom_root));

	/* process incoming signals */
	for (;;) {
		Signal s = sig_rec.wait_for_signal();
		static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
	}
	return 0;
}
