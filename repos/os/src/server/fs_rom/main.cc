/*
 * \brief  Service that provides files of a file system as ROM sessions
 * \author Norman Feske
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
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
#include <base/log.h>

/*****************
 ** ROM service **
 *****************/

namespace Fs_rom {
	using namespace Genode;

	struct Packet_handler;

	class Rom_session_component;
	class Rom_root;

	typedef Genode::List<Rom_session_component> Sessions;

	typedef File_system::Session_client::Tx::Source Tx_source;
}


/**
 * A 'Rom_session_component' exports a single file of the file system
 */
class Fs_rom::Rom_session_component :
	public Genode::Rpc_object<Genode::Rom_session>, public Sessions::Element
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
		Genode::Constructible<File_system::File_handle> _file_handle;

		/**
		 * Size of current version of the file
		 */
		File_system::file_size_t _file_size = 0;

		/**
		 * Read offset of the file
		 */
		File_system::seek_off_t _file_seek = 0;

		/**
		 * Handle of currently watched compound directory
		 *
		 * The compund directory is watched only if the requested file could
		 * not be looked up.
		 */
		Genode::Constructible<File_system::Dir_handle> _compound_dir_handle;

		/**
		 * Dataspace exposed as ROM module to the client
		 */
		Genode::Attached_ram_dataspace _file_ds;

		/**
		 * Signal destination for ROM file changes
		 */
		Genode::Signal_context_capability _sigh;

		/*
		 * Exception
		 */
		struct Open_compound_dir_failed { };

		/*
		 * Version number used to track the need for ROM update notifications
		 */
		struct Version { unsigned value; };
		Version _curr_version       { 0 };
		Version _handed_out_version { ~0U };

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
			throw Open_compound_dir_failed();
		}

		/*
		 * Exception
		 */
		struct Open_file_failed { };

		/**
		 * Open file with specified name at the file system
		 */
		File_system::File_handle _open_file(File_system::Session &fs,
		                                    Path const &path)
		{
			using namespace File_system;

			try {

				Dir_handle dir = _open_compound_dir(fs, path, false);

				try {

					Handle_guard guard(fs, dir);

					/* open file */
					Genode::Path<PATH_MAX_LEN> file_name(path.base());
					file_name.keep_only_last_element();
					return fs.file(dir, file_name.base() + 1,
					               File_system::READ_ONLY, false);
				}
				catch (Invalid_handle)    { Genode::error(_file_path, ": Invalid_handle"); }
				catch (Invalid_name)      { Genode::error(_file_path, ": invalid_name"); }
				catch (Lookup_failed)     { Genode::error(_file_path, ": lookup_failed"); }
				catch (Permission_denied) { Genode::error(_file_path, ": Permission_denied"); }
				catch (...)               { Genode::error(_file_path, ": unhandled error"); };

				throw Open_file_failed();
				
			} catch (Open_compound_dir_failed) {
				throw Open_file_failed();
			}
		}

		void _register_for_compound_dir_changes()
		{
			using namespace File_system;

			/* forget about the previously watched compound directory */
			if (_compound_dir_handle.constructed()) {
				_fs.close(*_compound_dir_handle);
				_compound_dir_handle.destruct();
			}

			try {
				_compound_dir_handle.construct(_open_compound_dir(_fs, _file_path, true));

				/* register for changes in compound directory */
				_fs.tx()->submit_packet(File_system::Packet_descriptor(
					*_compound_dir_handle,
					File_system::Packet_descriptor::CONTENT_CHANGED));

			} catch (Open_compound_dir_failed) {
				Genode::warning("could not track compound dir, giving up");
			}
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
			try {
				File_handle const file_handle = _open_file(_fs, _file_path);
				File_system::file_size_t const new_file_size =
					_fs.status(file_handle).size;

				if (_file_ds.size() && (new_file_size > _file_size)) {
					/* mark as invalid */
					_file_ds.realloc(&_env.ram(), 0);
					_file_size = 0;
					_file_seek = 0;
				}
				_fs.close(file_handle);
			} catch (Open_file_failed) { }

			/* close and then re-open the file */
			if (_file_handle.constructed()) {
				_fs.close(*_file_handle);
				_file_handle.destruct();
			}

			try {
				_file_handle.construct(_open_file(_fs, _file_path));
			} catch (Open_file_failed) { }

			/*
			 * If we got the file, we can stop paying attention to the
			 * compound directory.
			 */
			if (_file_handle.constructed() && _compound_dir_handle.constructed()) {
				_fs.close(*_compound_dir_handle);
				_compound_dir_handle.destruct();
			}

			/* register for file changes */
			if (_file_handle.constructed())
				_fs.tx()->submit_packet(File_system::Packet_descriptor(
					*_file_handle, File_system::Packet_descriptor::CONTENT_CHANGED));

			size_t const file_size = _file_handle.constructed()
			                       ? _fs.status(*_file_handle).size : 0;

			/* allocate new RAM dataspace according to file size */
			if (file_size > 0) {
				try {
					_file_seek = 0;
					_file_ds.realloc(&_env.ram(), file_size);
					_file_size = file_size;
				} catch (...) {
					Genode::error("couldn't allocate memory for file, empty result");;
					return;
				}
			} else {
				_register_for_compound_dir_changes();
				return;
			}

			/* omit read if file is empty */
			if (_file_size == 0)
				return;

			/* read content from file */
			Tx_source &source = *_fs.tx();
			while (_file_seek < _file_size) {
				/* if we cannot submit then process acknowledgements */
				if (source.ready_to_submit()) {
					size_t chunk_size = min(_file_size - _file_seek,
					                        source.bulk_buffer_size() / 2);
					File_system::Packet_descriptor
						packet(source.alloc_packet(chunk_size),
						       *_file_handle,
						       File_system::Packet_descriptor::READ,
						       chunk_size,
						       _file_seek);
					source.submit_packet(packet);
				}

				/*
				 * Process the global signal handler until we got a response
				 * for the read request (indicated by a change of the seek
				 * position).
				 */
				size_t const orig_file_seek = _file_seek;
				while (_file_seek == orig_file_seek)
					_env.ep().wait_and_dispatch_one_io_signal();
			}
		}

		void _notify_client_about_new_version()
		{
			if (_sigh.valid() && _curr_version.value != _handed_out_version.value)
				Genode::Signal_transmitter(_sigh).submit();
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
			_env(env), _fs(fs),
			_file_path(file_path),
			_file_ds(env.ram(), env.rm(), 0) /* realloc later */
		{
			try {
				_file_handle.construct(_open_file(_fs, _file_path));
			} catch (Open_file_failed) { }

			_register_for_compound_dir_changes();
		}

		/**
		 * Destructor
		 */
		~Rom_session_component()
		{
			/* close re-open the file */
			if (_file_handle.constructed())
				_fs.close(*_file_handle);

			if (_compound_dir_handle.constructed())
				_fs.close(*_compound_dir_handle);
		}

		/**
		 * Return dataspace with up-to-date content of file
		 */
		Genode::Rom_dataspace_capability dataspace()
		{
			_update_dataspace();
			Genode::Dataspace_capability ds = _file_ds.cap();
			_handed_out_version = _curr_version;
			return Genode::static_cap_cast<Genode::Rom_dataspace>(ds);
		}

		void sigh(Genode::Signal_context_capability sigh)
		{
			_sigh = sigh;
			_notify_client_about_new_version();
		}

		/**
		 * If packet corresponds to this session then process and return true.
		 *
		 * Called from the signal handler.
		 */
		bool process_packet(File_system::Packet_descriptor const packet)
		{
			switch (packet.operation()) {

			case File_system::Packet_descriptor::CONTENT_CHANGED:

				_curr_version = Version { _curr_version.value + 1 };

				if ((_file_handle.constructed() && (*_file_handle == packet.handle())) ||
				    (_compound_dir_handle.constructed() && (*_compound_dir_handle == packet.handle())))
				{
					_notify_client_about_new_version();
					return true;
				}
				return false;

			case File_system::Packet_descriptor::READ: {

				if (!(_file_handle.constructed() && (*_file_handle == packet.handle())))
					return false;

				if (packet.position() > _file_seek || _file_seek >= _file_size) {
					error("bad packet seek position");
					_file_ds.realloc(&_env.ram(), 0);
					return true;
				}

				size_t const n = min(packet.length(), _file_size - _file_seek);
				memcpy(_file_ds.local_addr<char>()+_file_seek,
				       _fs.tx()->packet_content(packet), n);
				_file_seek += n;
				return true;
			}

			default:

				Genode::error("discarding strange packet acknowledgement");
				return true;
			}
			return false;
		}
};

struct Fs_rom::Packet_handler : Genode::Io_signal_handler<Packet_handler>
{
	Tx_source &source;

	/* list of open sessions */
	Sessions sessions;

	void handle_packets()
	{
		while (source.ack_avail()) {
			File_system::Packet_descriptor pack = source.get_acked_packet();
			for (Rom_session_component *session = sessions.first();
			     session; session = session->next())
			{
				if (session->process_packet(pack))
					break;
			}
			source.release_packet(pack);
		}
	}

	Packet_handler(Genode::Entrypoint &ep, Tx_source &source)
	:
		Genode::Io_signal_handler<Packet_handler>(
			ep, *this, &Packet_handler::handle_packets),
		source(source)
	{ }
};


class Fs_rom::Rom_root : public Genode::Root_component<Fs_rom::Rom_session_component>
{
	private:

		Genode::Env          &_env;
		Genode::Heap          _heap { _env.ram(), _env.rm() };
		Genode::Allocator_avl _fs_tx_block_alloc { &_heap };

		/* open file-system session */
		File_system::Connection _fs { _env, _fs_tx_block_alloc };

		Packet_handler _packet_handler { _env.ep(), *_fs.tx() };

		Rom_session_component *_create_session(const char *args) override
		{
			Genode::Session_label const label = label_from_args(args);
			Genode::Session_label const module_name = label.last_element();

			Genode::log("request for ", label);

			/* create new session for the requested file */
			Rom_session_component *session = new (md_alloc())
				Rom_session_component(_env, _fs, module_name.string());

			_packet_handler.sessions.insert(session);
			return session;
		}

		void _destroy_session(Rom_session_component *session) override
		{
			_packet_handler.sessions.remove(session);
			Genode::destroy(md_alloc(), session);
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
			/* Process CONTENT_CHANGED acknowledgement packets at the entrypoint  */
			_fs.sigh_ack_avail(_packet_handler);

			env.parent().announce(env.ep().manage(*this));
		}
};


void Component::construct(Genode::Env &env)
{
	static Genode::Sliced_heap sliced_heap(env.ram(), env.rm());
	static Fs_rom::Rom_root inst(env, sliced_heap);
}
