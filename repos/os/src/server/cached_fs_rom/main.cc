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

	struct Cached_rom;
	typedef Genode::Id_space<Cached_rom> Cache_space;

	struct Transfer;
	typedef Genode::Id_space<Transfer> Transfer_space;

	class Session_component;
	typedef Genode::Id_space<Session_component> Session_space;

	struct Main;

	typedef File_system::Session::Tx::Source::Packet_alloc_failed Packet_alloc_failed;
	typedef File_system::File_handle File_handle;
}


struct Cached_fs_rom::Cached_rom final
{
	Cached_rom(Cached_rom const &);
	Cached_rom &operator = (Cached_rom const &);

	Genode::Env   &env;
	Rm_connection &rm_connection;

	size_t const file_size;

	/**
	 * Backing RAM dataspace
	 *
	 * This shall be valid even if the file is empty.
	 */
	Attached_ram_dataspace ram_ds {
		env.pd(), env.rm(), file_size ? file_size : 1 };

	/**
	 * Read-only region map exposed as ROM module to the client
	 */
	Region_map_client      rm { rm_connection.create(ram_ds.size()) };
	Region_map::Local_addr rm_attachment { };
	Dataspace_capability   rm_ds { };

	Path const path;

	Cache_space::Element cache_elem;

	Transfer *transfer = nullptr;

	/**
	 * Reference count of cache entry
	 */
	int _ref_count = 0;

	Cached_rom(Cache_space   &cache_space,
	           Env           &env,
	           Rm_connection &rm,
	           Path const    &file_path,
	           size_t         size)
	:
		env(env), rm_connection(rm), file_size(size),
		path(file_path),
		cache_elem(*this, cache_space)
	{
		if (size == 0)
			complete();
	}

	/**
	 * Destructor
	 */
	~Cached_rom()
	{
		if (rm_attachment)
			rm.detach(rm_attachment);
	}

	bool completed() const { return rm_ds.valid(); }
	bool unused()    const { return (_ref_count < 1); }

	void complete()
	{
		/* attach dataspace read-only into region map */
		enum { OFFSET = 0, LOCAL_ADDR = false, EXEC = true, WRITE = false };
		rm_attachment = rm.attach(
			ram_ds.cap(), ram_ds.size(), OFFSET,
			LOCAL_ADDR, (addr_t)~0, EXEC, WRITE);
		rm_ds = rm.dataspace();
	}

	/**
	 * Return dataspace with content of file
	 */
	Rom_dataspace_capability dataspace() const {
		return static_cap_cast<Rom_dataspace>(rm_ds); }

	struct Guard
	{
		Cached_rom &_rom;

		Guard(Cached_rom &rom) : _rom(rom) {
			++_rom._ref_count; }
		~Guard() {
			--_rom._ref_count; };
	};
};


struct Cached_fs_rom::Transfer final
{
		Cached_rom                    &_cached_rom;
		Cached_rom::Guard              _cache_guard { _cached_rom };

		File_system::Session          &_fs;
		File_system::File_handle       _handle;

		File_system::file_size_t const _size;
		File_system::seek_off_t        _seek = 0;
		File_system::Packet_descriptor _raw_pkt = _alloc_packet();
		File_system::Packet_guard      _packet_guard { *_fs.tx(), _raw_pkt };

		Transfer_space::Element        _transfer_elem;

		/**
		 * Allocate space in the File_system packet buffer
		 *
		 * \throw  Packet_alloc_failed
		 */
		File_system::Packet_descriptor _alloc_packet()
		{
			if (!_fs.tx()->ready_to_submit())
				throw Packet_alloc_failed();

			size_t chunk_size = (size_t)min(_size, _fs.tx()->bulk_buffer_size()/2);
			return _fs.tx()->alloc_packet(chunk_size);
		}

		void _submit_next_packet()
		{
			using namespace File_system;

			File_system::Packet_descriptor
			packet(_raw_pkt, _handle,
			       File_system::Packet_descriptor::READ,
			       _raw_pkt.size(), _seek);

			_fs.tx()->submit_packet(packet);
		}

	public:

		/**
		 * Constructor
		 */
		Transfer(Transfer_space           &space,
		         Cached_rom               &rom,
		         File_system::Session     &fs,
		         File_system::File_handle  file_handle,
		         size_t                    file_size)
		:
			_cached_rom(rom), _fs(fs),
			_handle(file_handle), _size(file_size),
			_transfer_elem(*this, space, Transfer_space::Id{_handle.value})
		{
			_cached_rom.transfer = this;

			_submit_next_packet();
		}

		Path const &path() const { return _cached_rom.path; }

		bool completed() const { return (_seek >= _size); }

		/**
		 * Called from the packet signal handler.
		 */
		void process_packet(File_system::Packet_descriptor const packet)
		{
			auto const pkt_seek = packet.position();

			if (pkt_seek > _seek || _seek >= _size) {
				error("bad packet seek position for ", path());
				error("packet seek is ", packet.position(), ", file seek is ", _seek, ", file size is ", _size);
				_seek = _size;
			} else {
				size_t const n = min(packet.length(), (size_t)(_size - pkt_seek));
				memcpy(_cached_rom.ram_ds.local_addr<char>()+pkt_seek,
				       _fs.tx()->packet_content(packet), n);
				_seek = pkt_seek+n;
			}

			if (completed())
					_cached_rom.complete();
			else
				_submit_next_packet();
		}
};


class Cached_fs_rom::Session_component final : public  Rpc_object<Rom_session>
{
	private:

		Cached_rom             &_cached_rom;
		Cached_rom::Guard       _cache_guard { _cached_rom };
		Session_space::Element  _sessions_elem;
		Session_label    const  _label;

	public:

		Session_component(Cached_rom &cached_rom,
		                  Session_space &space, Session_space::Id id,
		                  Session_label const &label)
		:
			_cached_rom(cached_rom),
			_sessions_elem(*this, space, id),
			_label(label)
		{ }


		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override {
			return _cached_rom.dataspace(); }

		void sigh(Signal_context_capability) override { }

		bool update() override { return false; }
};


struct Cached_fs_rom::Main final : Genode::Session_request_handler
{
	Genode::Env &env;

	Rm_connection rm { env };

	Cache_space    cache     { };
	Transfer_space transfers { };
	Session_space  sessions  { };

	Heap heap { env.pd(), env.rm() };

	Allocator_avl           fs_tx_block_alloc { &heap };
	File_system::Connection fs { env, fs_tx_block_alloc, "", "/", false, 4*1024*1024 };

	Session_requests_rom session_requests { env, *this };

	Io_signal_handler<Main> packet_handler {
		env.ep(), *this, &Main::handle_packets };

	/**
	 * Return true when a cache element is freed
	 */
	bool cache_evict()
	{
		Cached_rom *discard = nullptr;

		cache.for_each<Cached_rom&>([&] (Cached_rom &rom) {
			if (!discard && rom.unused()) discard = &rom; });

		if (discard)
			destroy(heap, discard);
		return (bool)discard;
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

		Session_space::Id  const id { pid.value };

		Session_label const label = label_from_args(args.string());
		Path          const path(label.last_element().string());

		Cached_rom *rom = nullptr;

		/* lookup the ROM in the cache */
		cache.for_each<Cached_rom&>([&] (Cached_rom &other) {
			if (!rom && other.path == path)
				rom = &other;
		});

		if (!rom) {
			File_system::File_handle handle = try_open(path);
			File_system::Handle_guard guard(fs, handle);
			File_system::file_size_t file_size = fs.status(handle).size;

			while (env.pd().avail_ram().value < file_size || env.pd().avail_caps().value < 8) {
				/* drop unused cache entries */
				if (!cache_evict()) break;
			}

			rom = new (heap) Cached_rom(cache, env, rm, path, (size_t)file_size);
		}

		if (rom->completed()) {
			/* Create new RPC object */
			Session_component *session = new (heap)
				Session_component(*rom, sessions, id, label);
			if (session_diag_from_args(args.string()).enabled)
				log("deliver ROM \"", label, "\"");
			env.parent().deliver_session_cap(pid, env.ep().manage(*session));

		} else if (!rom->transfer) {
			File_system::File_handle handle = try_open(path);

			try {
				new (heap) Transfer(transfers, *rom, fs, handle, rom->file_size);
			}
			catch (...) {
				fs.close(handle);
				/* retry when next pending transfer completes */
				return;
			}
		}
	}

	void handle_session_close(Parent::Server::Id pid) override
	{
		Session_space::Id id { pid.value };
		sessions.apply<Session_component&>(
			id, [&] (Session_component &session)
		{
			env.ep().dissolve(session);
			destroy(heap, &session);
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
			transfers.apply<Transfer&>(
				Transfer_space::Id{pkt.handle().value}, [&] (Transfer &transfer)
			{
				transfer.process_packet(pkt);
				if (transfer.completed()) {
					session_requests.schedule();
					destroy(heap, &transfer);
				}
				stray_pkt = false;
			});

			if (stray_pkt)
				source.release_packet(pkt);
		}
	}

	Main(Genode::Env &env) : env(env)
	{
		fs.sigh(packet_handler);

		/* process any requests that have already queued */
		session_requests.schedule();
	}
};


void Component::construct(Genode::Env &env)
{
	static Cached_fs_rom::Main inst(env);
	env.parent().announce("ROM");
}
