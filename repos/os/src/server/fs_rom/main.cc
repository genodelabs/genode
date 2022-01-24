/*
 * \brief  Service that provides files of a file system as ROM sessions
 * \author Norman Feske
 * \author Emery Hemingway
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <rom_session/rom_session.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <os/path.h>
#include <base/attached_ram_dataspace.h>
#include <root/component.h>
#include <base/component.h>
#include <base/session_label.h>
#include <util/arg_string.h>
#include <base/heap.h>


/*****************
 ** ROM service **
 *****************/

namespace Fs_rom {
	using namespace Genode;

	class Rom_session_component;
	class Rom_root;

	typedef Id_space<Rom_session_component> Sessions;

	typedef File_system::Session_client::Tx::Source Tx_source;
}


/**
 * A 'Rom_session_component' exports a single file of the file system
 */
class Fs_rom::Rom_session_component : public  Rpc_object<Rom_session>
{
	private:

		friend class List<Rom_session_component>;
		friend class Packet_handler;

		Env                  &_env;
		Sessions             &_sessions;
		File_system::Session &_fs;

		Constructible<Sessions::Element> _watch_elem { };

		enum { PATH_MAX_LEN = 512 };
		typedef Genode::Path<PATH_MAX_LEN> Path;

		/**
		 * Name of requested file, interpreted at path into the file system
		 */
		Path const _file_path;

		/**
		 * Wandering notification handle
		 */
		Constructible<File_system::Watch_handle> _watch_handle { };

		/**
		 * Handle of associated file opened during read loop
		 */
		File_system::File_handle _file_handle { ~0UL };

		/**
		 * Size of current version of the file
		 */
		File_system::file_size_t _file_size = 0;

		/**
		 * Read offset of the file
		 */
		File_system::seek_off_t _file_seek = 0;

		/**
		 * Dataspace exposed as ROM module to the client
		 */
		Attached_ram_dataspace _file_ds;

		/**
		 * Signal destination for ROM file changes
		 */
		Signal_context_capability _sigh { };

		/*
		 * Version number used to track the need for ROM update notifications
		 */
		struct Version { unsigned value; };
		Version _curr_version       { 0 };
		Version _handed_out_version { 0 };

		/**
		 * Track if the session file or a directory is being watched
		 */
		bool _watching_file = false;

		/*
		 * Exception
		 */
		struct Watch_failed { };

		/**
		 * Watch a path or some parent directory
		 *
		 * If the given path or a parent directory could be watched, the
		 * corresponding watch handle is returned and 'path_watched' returns if
		 * the watch handle refers to the given path (true) or to a parent
		 * directory (false). Otherwise, 'Watch_failed' is thrown.
		 */
		File_system::Watch_handle _open_watch_handle_helper(Path watch_path,
		                                                    bool &path_watched)
		{
			try {
				File_system::Watch_handle watch_handle = _fs.watch(watch_path.base());
				path_watched = true;
				return watch_handle;
			}
			catch (File_system::Lookup_failed) { }
			catch (File_system::Unavailable) { }

			/* watching the given path failed, try to watch a parent directory */

			if (watch_path == "/")
				throw Watch_failed();

			try {

				Path immediate_parent_watch_path { watch_path };
				immediate_parent_watch_path.strip_last_element();

				bool immediate_parent_watched = false;

				File_system::Watch_handle some_parent_watch_handle =
					_open_watch_handle_helper(immediate_parent_watch_path,
					                          immediate_parent_watched);

				if (immediate_parent_watched) {

					/*
					 * Watching the immediate parent directory succeeded, now
					 * try to watch the original path again, in case it has
					 * been created in the meantime.
					 */

					try {
						File_system::Watch_handle watch_handle = _fs.watch(watch_path.base());
						_fs.close(some_parent_watch_handle);
						path_watched = true;
						return watch_handle;
					}
					catch (File_system::Lookup_failed) { }
					catch (File_system::Unavailable) { }
				}

				/*
				 * Watching the immediate parent directory or the original path
				 * again failed, so pass on the received parent watch handle.
				 */
				path_watched = false;
				return some_parent_watch_handle;

			} catch (Watch_failed) {
				/* None of the parent directories could be watched */
				throw;
			}
		}

		/**
		 * Watch the session ROM file or some parent directory
		 */
		void _open_watch_handle()
		{
			using namespace File_system;

			_close_watch_handle();

			Path watch_path(_file_path);

			_watching_file = false;

			try {
				Watch_handle watch_handle = _open_watch_handle_helper(watch_path.base(),
					                                                  _watching_file);
				_watch_handle.construct(watch_handle);
				_watch_elem.construct(
					*this, _sessions, Sessions::Id{_watch_handle->value});
				return;
			} catch (File_system::Out_of_ram) {
				error("not enough RAM to watch '", watch_path, "'");
			} catch (File_system::Out_of_caps) {
				error("not enough caps to watch '", watch_path, "'");
			}
			throw Watch_failed();
		}

		void _close_watch_handle()
		{
			if (_watch_handle.constructed()) {
				_watch_elem.destruct();
				_fs.close(*_watch_handle);
				_watch_handle.destruct();
			}
			_watching_file = false;
		}

		enum { UPDATE_OR_REPLACE = false, UPDATE_ONLY = true };

		/**
		 * Fill dataspace with file content, return true if the
		 * current dataspace is reused.
		 */
		bool _read_dataspace(bool update_only)
		{
			using namespace File_system;

			Genode::Path<PATH_MAX_LEN> dir_path(_file_path);
			dir_path.strip_last_element();
			Genode::Path<PATH_MAX_LEN> file_name(_file_path);
			file_name.keep_only_last_element();

			Dir_handle parent_handle = _fs.dir(dir_path.base(), false);
			Handle_guard parent_guard(_fs, parent_handle);

			/* the file handle is opened here... */
			_file_handle = _fs.file(
				parent_handle, file_name.base() + 1,
				File_system::READ_ONLY, false);
			Handle_guard file_guard(_fs, _file_handle);
			Sessions::Element read_elem(
				*this, _sessions, Sessions::Id{_file_handle.value});
			/* ...but only for the lifetime of this procedure */

			_file_seek = 0;
			_file_size = _fs.status(_file_handle).size;

			if (_file_size > _file_ds.size()) {
				/* allocate new RAM dataspace according to file size */
				if (update_only)
					return false;

				try {
					_file_ds.realloc(&_env.ram(), (size_t)_file_size);
				} catch (...) {
					error("failed to allocate memory for ", _file_path);
					return false;
				}
			} else {
				memset(_file_ds.local_addr<char>(), 0x00, _file_ds.size());
			}

			/* omit read if file is empty */
			if (_file_size == 0)
				return false;

			/* read content from file */
			Tx_source &source = *_fs.tx();
			while (_file_seek < _file_size) {
				/* if we cannot submit then process acknowledgements */
				while (!source.ready_to_submit())
					_env.ep().wait_and_dispatch_one_io_signal();

				size_t chunk_size = min((size_t)(_file_size - _file_seek),
				                        source.bulk_buffer_size() / 2);

				File_system::Packet_descriptor
					packet(source.alloc_packet(chunk_size), _file_handle,
					       File_system::Packet_descriptor::READ,
					       chunk_size, _file_seek);

				source.submit_packet(packet);

				/*
				 * Process the global signal handler until we got a response
				 * for the read request (indicated by a change of the seek
				 * position).
				 */
				File_system::seek_off_t const orig_file_seek = _file_seek;
				while (_file_seek == orig_file_seek)
					_env.ep().wait_and_dispatch_one_io_signal();
			}

			_handed_out_version = _curr_version;
			return true;
		}

		bool _try_read_dataspace(bool update_only)
		{
			using namespace File_system;

			try { _open_watch_handle(); }
			catch (Watch_failed) { }

			try { return _read_dataspace(update_only); }
			catch (Lookup_failed)     { /* missing but may appear anytime soon */ }
			catch (Invalid_handle)    { warning(_file_path, ": invalid handle"); }
			catch (Invalid_name)      { warning(_file_path, ": invalid name"); }
			catch (Permission_denied) { warning(_file_path, ": permission denied"); }
			catch (...)               { warning(_file_path, ": unhandled error"); };

			return false;
		}

		void _notify_client_about_new_version()
		{
			using namespace File_system;

			if (_sigh.valid() && _curr_version.value != _handed_out_version.value) {

				/* notify if the file exists and is not empty */
				try {
					Node_handle file = _fs.node(_file_path.base());
					Handle_guard g(_fs, file);
					_file_size = _fs.status(file).size;
					if (_file_size > 0) {
						/* assume a transition between versions */
						Signal_transmitter(_sigh).submit();
					}
				}

				/* notify if the file is removed */
				catch (File_system::Lookup_failed) {
					if (_file_size > 0) {
						memset(_file_ds.local_addr<char>(), 0x00, (size_t)_file_size);
						_file_size = 0;
						Signal_transmitter(_sigh).submit();
					}
				}

				catch (...) { }
			}
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
		Rom_session_component(Env &env,
		                      Sessions &sessions,
		                      File_system::Session &fs,
		                      const char *file_path)
		:
			_env(env), _sessions(sessions), _fs(fs),
			_file_path(file_path),
			_file_ds(env.ram(), env.rm(), 0) /* realloc later */
		{
			try { _open_watch_handle(); }
			catch (Watch_failed) { }

			/**
			 * XXX: fix for live-lock, this constructor is called when this
			 * component handles a sesson_requests ROM signal, and preparing
			 * the dataspace now will hopefully prevent any interaction with
			 * the parent when the dataspace RPC method is called.
			 */
			_try_read_dataspace(UPDATE_OR_REPLACE);
		}

		/**
		 * Destructor
		 */
		~Rom_session_component()
		{
			_close_watch_handle();
		}

		/**
		 * Return dataspace with up-to-date content of file
		 */
		Rom_dataspace_capability dataspace() override
		{
			using namespace File_system;

			_try_read_dataspace(UPDATE_OR_REPLACE);

			/* always serve a valid, even empty, dataspace */
			if (_file_ds.size() < 1) {
				_file_ds.realloc(&_env.ram(), 1);
			}

			Dataspace_capability ds = _file_ds.cap();
			return static_cap_cast<Rom_dataspace>(ds);
		}

		void sigh(Signal_context_capability sigh) override
		{
			_sigh = sigh;

			if (_sigh.valid()) {
				try { _open_watch_handle(); }
				catch (Watch_failed) { }
			}

			_notify_client_about_new_version();
		}

		/**
		 * Update the current dataspace content
		 */
		bool update() override {
			return _try_read_dataspace(UPDATE_ONLY); }

		/**
		 * Called by the packet signal handler.
		 */
		void process_packet(File_system::Packet_descriptor const packet)
		{
			switch (packet.operation()) {

			case File_system::Packet_descriptor::CONTENT_CHANGED:
				if (!(packet.handle() == *_watch_handle))
					return;

				if (!_watching_file) {
					/* try and get closer to the file */
					_open_watch_handle();
				}

				if (_watching_file) {
					/* notify the client of the change */
					_curr_version = Version { _curr_version.value + 1 };
					_notify_client_about_new_version();
				}
				return;

			case File_system::Packet_descriptor::READ: {

				if (!(packet.handle() == _file_handle))
					return;

				if (packet.position() > _file_seek || _file_seek >= _file_size) {
					error("bad packet seek position");
					_file_ds.realloc(&_env.ram(), 0);
					_file_seek = 0;
					return;
				}

				size_t const n = min(packet.length(), (size_t)(_file_size - _file_seek));
				memcpy(_file_ds.local_addr<char>()+_file_seek,
				       _fs.tx()->packet_content(packet), n);
				_file_seek += n;
				return;
			}

			case File_system::Packet_descriptor::WRITE:
				warning("discarding strange WRITE acknowledgement");
				return;
			case File_system::Packet_descriptor::SYNC:
				warning("discarding strange SYNC acknowledgement");
				return;
			case File_system::Packet_descriptor::READ_READY:
				warning("discarding strange READ_READY acknowledgement");
				return;
			case File_system::Packet_descriptor::WRITE_TIMESTAMP:
				warning("discarding strange WRITE_TIMESTAMP acknowledgement");
				return;
			}
		}
};


class Fs_rom::Rom_root : public Root_component<Fs_rom::Rom_session_component>
{
	private:

		Env          &_env;
		Heap          _heap { _env.ram(), _env.rm() };
		Sessions    _sessions { };

		Allocator_avl _fs_tx_block_alloc { &_heap };

		/* open file-system session */
		File_system::Connection _fs { _env, _fs_tx_block_alloc };

		Io_signal_handler<Rom_root> _packet_handler {
			_env.ep(), *this, &Rom_root::_handle_packets };

		void _handle_packets()
		{
			Tx_source &source = *_fs.tx();

			while (source.ack_avail()) {
				File_system::Packet_descriptor pkt = source.get_acked_packet();

				/* sessions are indexed in space by watch and read handles */

				auto const apply_fn = [pkt] (Rom_session_component &session) {
					session.process_packet(pkt); };

				try { _sessions.apply<Rom_session_component&>(
					Sessions::Id{pkt.handle().value}, apply_fn); }

				/* packet handle closed while packet in flight */
				catch (Sessions::Unknown_id) { }

				source.release_packet(pkt);
			}
		}

		Rom_session_component *_create_session(const char *args) override
		{
			Session_label const label = label_from_args(args);
			Session_label const module_name = label.last_element();

			/* create new session for the requested file */
			return new (md_alloc())
				Rom_session_component(_env, _sessions, _fs, module_name.string());
		}

	public:

		/**
		 * Constructor
		 *
		 * \param  env         Component environment
		 * \param  md_alloc    meta-data allocator used for ROM sessions
		 */
		Rom_root(Env       &env,
		         Allocator &md_alloc)
		:
			Root_component<Rom_session_component>(env.ep(), md_alloc),
			_env(env)
		{
			/* Process CONTENT_CHANGED acknowledgement packets at the entrypoint  */
			_fs.sigh(_packet_handler);

			env.parent().announce(env.ep().manage(*this));
		}
};


void Component::construct(Genode::Env &env)
{
	static Genode::Sliced_heap sliced_heap(env.ram(), env.rm());
	static Fs_rom::Rom_root inst(env, sliced_heap);
}
