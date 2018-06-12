/*
 * \brief  Component that caches files to be served as ROMs
 * \author Emery Hemingway
 * \date   2018-04-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/path.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <rom_session/rom_session.h>
#include <region_map/client.h>
#include <rm_session/connection.h>
#include <base/attached_ram_dataspace.h>
#include <base/session_label.h>
#include <base/heap.h>
#include <base/component.h>

/* local session-requests utility */
#include "session_requests.h"

/*****************
 ** ROM service **
 *****************/

namespace Cached_fs_rom {

	using namespace Genode;
	typedef Genode::Path<File_system::MAX_PATH_LEN> Path;
	typedef File_system::Session_client::Tx::Source Tx_source;

	class Rom_file;
	typedef Genode::Id_space<Rom_file> Rom_files;

	class Session_component;
	typedef Genode::Id_space<Session_component> Sessions;

	struct Packet_handler;
	struct Main;

}


class Cached_fs_rom::Rom_file final
{
	private:

		Env &_env;
		Rm_connection &_rm_connection;
		File_system::Session &_fs;

		Rom_files                         &_roms;
		Constructible<Rom_files::Element>  _roms_elem { };

		/**
		 * Name of requested file, interpreted at path into the file system
		 */
		Path const _file_path;

		/**
		 * Handle of associated file opened during read loop
		 */
		Constructible<File_system::File_handle> _file_handle { };

		/**
		 * Size of file
		 */
		File_system::file_size_t _file_size = 0;

		/**
		 * Read offset of file handle
		 */
		File_system::seek_off_t _file_seek = 0;

		/**
		 * Dataspace to read into
		 */
		Attached_ram_dataspace _file_ds { _env.pd(), _env.rm(), (size_t)_file_size };

		/**
		 * Read-only region map exposed as ROM module to the client
		 */
		Region_map_client      _rm { _rm_connection.create(_file_ds.size()) };
		Region_map::Local_addr _rm_attachment { };
		Dataspace_capability   _rm_ds { };

		/**
		 * Packet space used by this session
		 */
		File_system::Packet_descriptor _raw_pkt { };

		/**
		 * Reference count of ROM file, initialize to one and
		 * decrement after read completes.
		 */
		int _ref_count = 1;

		void _submit_next_packet()
		{
			using namespace File_system;

			Tx_source &source = *_fs.tx();

			while (!source.ready_to_submit()) {
				warning("FS packet queue congestion");
				_env.ep().wait_and_dispatch_one_io_signal();
			}

			File_system::Packet_descriptor
				packet(_raw_pkt, *_file_handle,
				       File_system::Packet_descriptor::READ,
				       _raw_pkt.size(), _file_seek);

			source.submit_packet(packet);
		}

		void _initiate_read()
		{
			using namespace File_system;

			Tx_source &source = *_fs.tx();

			size_t chunk_size = min(_file_size, source.bulk_buffer_size()/2);

			/* loop until a packet space is allocated */
			while (true) {
				try { _raw_pkt = source.alloc_packet(chunk_size); break; }
				catch (File_system::Session::Tx::Source::Packet_alloc_failed) {
					chunk_size = min(_file_size, source.bulk_buffer_size()/4);
					_env.ep().wait_and_dispatch_one_io_signal();
				}
			}

			_submit_next_packet();
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
		Rom_file(Env &env,
		         Rm_connection &rm,
		         File_system::Session &fs,
		         File_system::File_handle handle,
		         size_t      size,
		         Rom_files &cache,
		         Path const &file_path)
		:
			_env(env), _rm_connection(rm), _fs(fs), _roms(cache),
			_file_path(file_path), _file_size(size)
		{
			/*
			 * invert the file handle id to push the element to
			 * the opposite end of the space
			 */
			_roms_elem.construct(*this, _roms, Rom_files::Id{~handle.value});
			_file_handle.construct(handle);

			_initiate_read();
		}

		/**
		 * Destructor
		 */
		~Rom_file()
		{
			if (_file_handle.constructed())
				_fs.close(*_file_handle);
			_fs.tx()->release_packet(_raw_pkt);

			if (_rm_attachment)
				_rm.detach(_rm_attachment);
		}

		Path const &path() const { return _file_path; }

		bool completed() const { return _rm_ds.valid(); }

		void inc_ref() { ++_ref_count; }
		void dec_ref() { --_ref_count; }

		bool unused() const { return (_ref_count < 1); }

		bool matches(File_system::Node_handle h) const {
			return _file_handle.constructed() ? (*_file_handle == h) : false; }

		/**
		 * Return dataspace with content of file
		 */
		Rom_dataspace_capability dataspace() const {
			return static_cap_cast<Rom_dataspace>(_rm_ds); }

		/**
		 * Called from the signal handler.
		 */
		void process_packet(File_system::Packet_descriptor const packet)
		{
			if (!(packet.handle() == *_file_handle)) {
				error("packet and handle mismatch");
				throw ~0;
			}

			if (packet.position() > _file_seek || _file_seek >= _file_size) {
				error("bad packet seek position");
				_file_ds.realloc(&_env.ram(), 0);
				_file_seek = 0;
				_file_size = 0;
			} else {
				size_t const n = min(packet.length(), _file_size - _file_seek);
				memcpy(_file_ds.local_addr<char>()+_file_seek,
				       _fs.tx()->packet_content(packet), n);
				_file_seek += n;
			}

			if (_file_seek >= _file_size) {
				_fs.tx()->release_packet(_raw_pkt);
				_fs.close(*_file_handle);

				_file_handle.destruct();
				_roms_elem.destruct();

				_roms_elem.construct(*this, _roms);
				--_ref_count;

				/* attach dataspace read-only into region map */
				enum { OFFSET = 0, LOCAL_ADDR = false, EXEC = true, WRITE = false };
				_rm_attachment = _rm.attach(
					_file_ds.cap(), _file_size, OFFSET,
					LOCAL_ADDR, (addr_t)~0, EXEC, WRITE);
				_rm_ds = _rm.dataspace();
			} else {
				_submit_next_packet();
			}
		}
};


class Cached_fs_rom::Session_component final : public  Rpc_object<Rom_session>
{
	private:

		Rom_file &_rom_file;

		Sessions::Element _sessions_elem;

	public:

		Session_component(Rom_file &rom_file,
		                  Sessions &sessions,
		                  Sessions::Id id)
		:
			_rom_file(rom_file),
			_sessions_elem(*this, sessions, id)
		{
			_rom_file.inc_ref();
		}

		~Session_component()
		{
			_rom_file.dec_ref();
		}

		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override {
			return _rom_file.dataspace(); }

		void sigh(Signal_context_capability) override { }
		bool update() override { return false; }
};


struct Cached_fs_rom::Main final : Genode::Session_request_handler
{
	Genode::Env &env;

	Rm_connection rm { env };

	Rom_files rom_cache { };
	Sessions  rom_sessions { };

	/* Heap for local allocation */
	Heap heap { env.pd(), env.rm() };

	/* allocate sessions on a simple heap */
	Sliced_heap sliced_heap { env.pd(), env.rm() };

	Allocator_avl           fs_tx_block_alloc { &heap };
	File_system::Connection fs { env, fs_tx_block_alloc };

	Session_requests_rom session_requests { env, *this };

	Io_signal_handler<Main> packet_handler {
		env.ep(), *this, &Main::handle_packets };

	/**
	 * Signal handler to disable blocking behavior in 'Expanding_parent_client'
	 */
	Signal_handler<Main> resource_handler {
		env.ep(), *this, &Main::handle_resources };

	/**
	 * Process requests again if parent gives us an upgrade
	 */
	void handle_resources() {
		session_requests.process(); }

	/**
	 * Return true when a cache element is freed
	 */
	bool cache_evict()
	{
		Rom_file *discard = nullptr;

		rom_cache.for_each<Rom_file&>([&] (Rom_file &rf) {
			if (discard) return;
			if (rf.unused()) discard = &rf;
		});

		if (discard) {
			destroy(heap, discard);
			return true;
		}
		return false;
	}

	/**
	 * Open a file handle
	 */
	File_system::File_handle open(Path const &file_path)
	{
		using namespace File_system;

		Path dir_path(file_path);
		dir_path.strip_last_element();
		Path file_name(file_path);
		file_name.keep_only_last_element();

		Dir_handle parent_handle = fs.dir(dir_path.base(), false);
		Handle_guard parent_guard(fs, parent_handle);

		return fs.file(
			parent_handle, file_name.base() + 1,
			File_system::READ_ONLY, false);
	}

	/**
	 * Open a file with some exception management
	 */
	File_system::File_handle try_open(Path const &file_path)
	{
		using namespace File_system;
		try { return open(file_path); }
		catch (Lookup_failed)     { error(file_path, " not found");          }
		catch (Invalid_handle)    { error(file_path, ": invalid handle");    }
		catch (Invalid_name)      { error(file_path, ": invalid nme");       }
		catch (Permission_denied) { error(file_path, ": permission denied"); }
		catch (...)               { error(file_path, ": unhandled error");   }
		throw Service_denied();
	}

	/**
	 * Create new sessions
	 */
	void handle_session_create(Session_state::Name const &name,
	                           Parent::Server::Id pid,
	                           Session_state::Args const &args) override
	{
		if (name != "ROM") throw Service_denied();


		/************************************************
		 ** Enforce sufficient donation for RPC object **
		 ************************************************/

		size_t ram_quota =
			Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);
		size_t session_size =
			max((size_t)4096, sizeof(Session_component));

		if (ram_quota < session_size)
			throw Insufficient_ram_quota();


		/***********************
		 ** Find ROM in cache **
		 ***********************/

		Session_label const label = label_from_args(args.string());
		Path          const path(label.last_element().string());
		Sessions::Id  const id { pid.value };

		Rom_file *rom_file = nullptr;

		/* lookup the Rom_file in the cache */
		rom_cache.for_each<Rom_file&>([&] (Rom_file &rf) {
			if (!rom_file && rf.path() == path)
				rom_file = &rf;
		});

		if (!rom_file) {
			File_system::File_handle handle = try_open(path);
			size_t file_size = fs.status(handle).size;

			/* alloc-or-evict loop */
			do {
				try {
					new (heap)
						Rom_file(env, rm, fs, handle, file_size, rom_cache, path);
					/* session is ready when read completes */
					return;
				}
				/*
				 * There is an assumption that failure to allocate
				 * will implicitly trigger a resource request to the
				 * parent. If this behavior changes in the base library
				 * then this local mechanism cannot be expected to work.
				 */
				catch (Quota_guard<Ram_quota>::Limit_exceeded) { }
				catch (Quota_guard<Cap_quota>::Limit_exceeded) { }
			} while (cache_evict());
			/* eviction failed */
			warning("insufficient resources for '", label, "'"
			        ", stalling for upgrade");
			return;
		}

		if (!rom_file->completed())
			return;


		/***************************
		 ** Create new RPC object **
		 ***************************/

		try {
			Session_component *session = new (sliced_heap)
				Session_component(*rom_file, rom_sessions, id);
			env.parent().deliver_session_cap(pid, env.ep().manage(*session));
		}

		catch (Sessions::Conflicting_id) {
			Genode::warning("session request handled twice, ", args);
		}
	}

	void handle_session_close(Parent::Server::Id pid) override
	{
		Sessions::Id id { pid.value };
		rom_sessions.apply<Session_component&>(
			id, [&] (Session_component &session)
		{
			env.ep().dissolve(session);
			destroy(sliced_heap, &session);
			env.parent().session_response(pid, Parent::SESSION_CLOSED);
		});
	}

	void handle_packets()
	{
		Tx_source &source = *fs.tx();

		while (source.ack_avail()) {
			File_system::Packet_descriptor pkt = source.get_acked_packet();
			if (pkt.operation() != File_system::Packet_descriptor::READ) continue;

			bool stray_pkt = true;

			/* find the appropriate session */
			rom_cache.apply<Rom_file&>(
				Rom_files::Id{~(pkt.handle().value)}, [&] (Rom_file &rom)
			{
				rom.process_packet(pkt);
				if (rom.completed())
					session_requests.process();
				stray_pkt = false;
			});

			if (stray_pkt)
				source.release_packet(pkt);
		}
	}

	Main(Genode::Env &env) : env(env)
	{
		env.parent().resource_avail_sigh(resource_handler);

		fs.sigh_ack_avail(packet_handler);

		/* process any requests that have already queued */
		session_requests.process();
	}
};


void Component::construct(Genode::Env &env)
{
	static Cached_fs_rom::Main inst(env);
	env.parent().announce("ROM");


	/**
	 * XXX This workaround can be removed with the eager creation of the
	 *     the env log session.
	 */
	Genode::log("--- cached_fs_rom ready ---");
}
