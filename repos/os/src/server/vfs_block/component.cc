/*
 * \brief  VFS file to Block session
 * \author Josef Soentgen
 * \date   2020-05-05
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <block/request_stream.h>
#include <os/session_policy.h>
#include <util/string.h>
#include <vfs/simple_env.h>
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>

/* local includes */
#include "job.h"


using namespace Genode;


namespace Vfs_block {

	using File_path = String<Vfs::MAX_PATH_LEN>;
	struct File_info;
	File_info file_info_from_policy(Session_policy const &);
	class File;

} /* namespace Vfs_block */


struct Vfs_block::File_info
{
	File_path const path;
	bool      const writeable;
	size_t    const block_size;
};


Vfs_block::File_info Vfs_block::file_info_from_policy(Session_policy const &policy)
{
	File_path const file_path =
		policy.attribute_value("file", File_path());

	bool const writeable =
		policy.attribute_value("writeable", false);

	size_t const block_size =
		policy.attribute_value("block_size", 512u);

	return File_info {
		.path       = file_path,
		.writeable  = writeable,
		.block_size = block_size };
}


class Vfs_block::File
{
	private:

		File(const File&) = delete;
		File& operator=(const File&) = delete;

		Vfs::File_system &_vfs;
		Vfs::Vfs_handle  *_vfs_handle;

		Constructible<Vfs_block::Job> _job { };

		struct Io_response_handler : Vfs::Io_response_handler
		{
			Signal_context_capability sigh { };

			void read_ready_response() override { }

			void io_progress_response() override
			{
				if (sigh.valid()) {
					Signal_transmitter(sigh).submit();
				}
			}
		};
		Io_response_handler _io_response_handler { };

		Block::Session::Info _block_info { };

	public:

		File(Genode::Allocator         &alloc,
		     Vfs::File_system          &vfs,
		     Signal_context_capability  sigh,
		     File_info           const &info)
		:
			_vfs        { vfs },
			_vfs_handle { nullptr }
		{
			using DS = Vfs::Directory_service;

			unsigned const mode =
				info.writeable ? DS::OPEN_MODE_RDWR
				               : DS::OPEN_MODE_RDONLY;

			using Open_result = DS::Open_result;
			Open_result res = _vfs.open(info.path.string(), mode,
			                            &_vfs_handle, alloc);
			if (res != Open_result::OPEN_OK) {
				error("Could not open '", info.path.string(), "'");
				throw Genode::Exception();
			}

			using Stat_result = DS::Stat_result;
			Vfs::Directory_service::Stat stat { };
			Stat_result stat_res = _vfs.stat(info.path.string(), stat);
			if (stat_res != Stat_result::STAT_OK) {
				_vfs.close(_vfs_handle);
				error("Could not stat '", info.path.string(), "'");
				throw Genode::Exception();
			}

			Block::block_number_t const block_count =
				stat.size / info.block_size;

			_block_info = Block::Session::Info {
				.block_size  = info.block_size,
				.block_count = block_count,
				.align_log2  = log2(info.block_size),
				.writeable   = info.writeable,
			};

			_io_response_handler.sigh = sigh;
			_vfs_handle->handler(&_io_response_handler);
		}

		~File()
		{
			/*
			 * Sync is expected to be done through the Block
			 * request stream, omit it here.
			 */
			_vfs.close(_vfs_handle);
		}

		Block::Session::Info block_info() const { return _block_info; }

		bool execute()
		{
			if (!_job.constructed()) {
				return false;
			}

			return _job->execute();
		}

		bool acceptable() const
		{
			return !_job.constructed();
		}

		bool valid(Block::Request const &request)
		{
			using Type = Block::Operation::Type;

			/*
 			 * For READ/WRITE requests we need a valid block count
			 * and number. Other requests might not provide such
			 * information because it is not needed.
			 */

			Block::Operation const op = request.operation;
			switch (op.type) {
			case Type::READ: [[fallthrough]];
			case Type::WRITE:
				return op.count
				    && (op.block_number + op.count) <= _block_info.block_count;

			case Type::TRIM: [[fallthrough]];
			case Type::SYNC: return true;
			default:         return false;
			}
		}

		void submit(Block::Request req, void *ptr, size_t length)
		{
			file_offset const base_offset =
				req.operation.block_number * _block_info.block_size;

			_job.construct(*_vfs_handle, req, base_offset,
			               reinterpret_cast<char*>(ptr), length);
		}

		template <typename FN>
		void with_any_completed_job(FN const &fn)
		{
			if (!_job.constructed() || !_job->completed()) {
				return;
			}

			Block::Request req = _job->request;
			req.success = _job->succeeded();

			_job.destruct();

			fn(req);
		}
};


struct Block_session_component : Rpc_object<Block::Session>,
                                 private Block::Request_stream
{
	Entrypoint &_ep;

	using Block::Request_stream::with_requests;
	using Block::Request_stream::with_content;
	using Block::Request_stream::try_acknowledge;
	using Block::Request_stream::wakeup_client_if_needed;

	using Vfs_peers = Vfs::Remote_io::Deferred_wakeups;

	Vfs_block::File &_file;
	Vfs_peers       &_vfs_peers;

	Block_session_component(Region_map                 &rm,
	                        Entrypoint                 &ep,
	                        Dataspace_capability        ds,
	                        Signal_context_capability   sigh,
	                        Vfs_block::File            &file,
	                        Vfs_peers                  &vfs_peers)
	:
		Request_stream { rm, ds, ep, sigh, file.block_info() },
		_ep            { ep },
		_file          { file },
		_vfs_peers     { vfs_peers }
	{
		_ep.manage(*this);
	}

	~Block_session_component() { _ep.dissolve(*this); }

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }

	void handle_request()
	{
		for (;;) {

			bool progress = false;

			with_requests([&] (Block::Request request) {

				using Response = Block::Request_stream::Response;

				if (!_file.acceptable()) {
					return Response::RETRY;
				}

				if (!_file.valid(request)) {
					return Response::REJECTED;
				}

				using Op = Block::Operation;
				bool const payload =
					Op::has_payload(request.operation.type);

				try {
					if (payload) {
						with_content(request,
						[&] (void *ptr, size_t size) {
							_file.submit(request, ptr, size);
						});
					} else {
						_file.submit(request, nullptr, 0);
					}
				} catch (Vfs_block::Job::Unsupported_Operation) {
					return Response::REJECTED;
				}

				progress |= true;
				return Response::ACCEPTED;
			});

			progress |= _file.execute();

			try_acknowledge([&] (Block::Request_stream::Ack &ack) {

				auto ack_request = [&] (Block::Request request) {
					ack.submit(request);
					progress |= true;
				};

				_file.with_any_completed_job(ack_request);
			});

			if (!progress) {
				break;
			}
		}

		_vfs_peers.trigger();

		wakeup_client_if_needed();
	}
};


struct Main : Rpc_object<Typed_root<Block::Session>>
{
	Env &_env;

	Signal_handler<Main> _request_handler {
		_env.ep(), *this, &Main::_handle_requests };

	Heap                    _heap       { _env.ram(), _env.rm() };
	Attached_rom_dataspace  _config_rom { _env, "config" };

	Vfs::Simple_env _vfs_env { _env, _heap,
		_config_rom.xml().sub_node("vfs") };

	Constructible<Attached_ram_dataspace>  _block_ds { };
	Constructible<Vfs_block::File>         _block_file { };
	Constructible<Block_session_component> _block_session { };

	void _handle_requests()
	{
		if (!_block_session.constructed()) {
			return;
		}

		_block_session->handle_request();
	}


	/*
	 * Root interface
	 */

	Capability<Session> session(Root::Session_args const &args,
	                            Affinity const &) override
	{
		if (_block_session.constructed()) {
			throw Service_denied();
		}

		size_t const tx_buf_size =
			Arg_string::find_arg(args.string(),
			                     "tx_buf_size").aligned_size();

		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		if (tx_buf_size > ram_quota.value) {
			warning("communication buffer size exceeds session quota");
			throw Insufficient_ram_quota();
		}

		/* make sure policy is up-to-date */
		_config_rom.update();

		Session_label  const label  { label_from_args(args.string()) };
		Session_policy const policy { label, _config_rom.xml() };

		if (!policy.has_attribute("file")) {
			error("policy lacks 'file' attribute");
			throw Service_denied();
		}

		Vfs_block::File_info const file_info =
			Vfs_block::file_info_from_policy(policy);

		try {
			_block_ds.construct(_env.ram(), _env.rm(), tx_buf_size);
			_block_file.construct(_heap, _vfs_env.root_dir(),
			                      _request_handler, file_info);
			_block_session.construct(_env.rm(), _env.ep(),
			                         _block_ds->cap(),
			                         _request_handler, *_block_file,
			                         _vfs_env.deferred_wakeups());

			return _block_session->cap();
		} catch (...) {
			throw Service_denied();
		}
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const &) override { }

	void close(Capability<Session> cap) override
	{
		if (cap == _block_session->cap()) {
			_block_session.destruct();
			_block_file.destruct();
			_block_ds.destruct();
		}
	}

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(*this));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
