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
#include <root/root.h>
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
	File_info file_info_from_policy(Node const &);
	class File;

} /* namespace Vfs_block */


struct Vfs_block::File_info
{
	File_path const path;
	bool      const writeable;
	size_t    const block_size;

	size_t    const maximum_transfer_size;
};


Vfs_block::File_info Vfs_block::file_info_from_policy(Node const &policy)
{
	File_path const file_path =
		policy.attribute_value("file", File_path());

	bool const writeable =
		policy.attribute_value("writeable", false);

	Num_bytes const block_size =
		policy.attribute_value("block_size", Num_bytes(512u));

	Num_bytes const maximum_transfer_size =
		policy.attribute_value("maximum_transfer_size", Num_bytes(0));

	return File_info {
		.path                  = file_path,
		.writeable             = writeable,
		.block_size            = block_size,
		.maximum_transfer_size = maximum_transfer_size };
}


class Vfs_block::File
{
	private:

		File(const File&) = delete;
		File& operator=(const File&) = delete;

		Vfs::File_system &_vfs;
		Vfs::Vfs_handle  *_vfs_handle;

		Constructible<Vfs_block::Job> _job { };

		Block::Session::Info _block_info { };

		Block::block_number_t _last_allowed_block { 0 };

		Block::Constrained_view const _view;

		Block::block_count_t _transfer_block_count_limit { 0 };

	public:

		File(Genode::Allocator             &alloc,
		     Vfs::File_system              &vfs,
		     File_info               const &info,
		     Block::Constrained_view const &view)
		:
			_vfs         { vfs },
			_vfs_handle  { nullptr },
			_view        { view }
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

			uint64_t const file_block_count = stat.size / info.block_size;
			if (view.offset.value >= file_block_count) {
				error("offset larger than total block count");
				throw Genode::Exception();
			}

			uint64_t const limited_block_count = file_block_count - view.offset.value;
			uint64_t const num_blocks =
				min(limited_block_count, view.num_blocks.value ? view.num_blocks.value
				                                               : ~0ull);

			_last_allowed_block = view.offset.value + num_blocks - 1;

			_block_info = Block::Session::Info {
				.block_size  = info.block_size,
				.block_count = num_blocks,
				.align_log2  = log2(info.block_size, 0u),
				.writeable   = info.writeable && view.writeable,
			};

			/*
			 * If set make sure we limit to at least on block,
			 * the upper limit is unchecked as too large counts
			 * in a request are rejected anyway.
			*/
			if (info.maximum_transfer_size)
				_transfer_block_count_limit =
					max(info.maximum_transfer_size,
					    info.block_size) / info.block_size;
		}

		~File()
		{
			/*
			 * Sync is expected to be done through the Block
			 * request stream, omit it here.
			 */
			_vfs.close(_vfs_handle);
		}

		Block::block_count_t transfer_block_count_limit() const {
			return _transfer_block_count_limit; }

		Block::Session::Info block_info() const { return _block_info; }

		Block::Constrained_view const view() const { return _view; }

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

			bool const valid_range = op.count
			                      && (op.block_number + op.count - 1)
			                      <= _last_allowed_block;
			switch (op.type) {
			case Type::WRITE:
				return valid_range && _view.writeable;
			case Type::READ:
				return valid_range;

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

	Vfs_block::File &_file;
	Vfs::Env::Io    &_io;

	Block_session_component(Env::Local_rm              &rm,
	                        Entrypoint                 &ep,
	                        Dataspace_capability        ds,
	                        Signal_context_capability   sigh,
	                        Vfs_block::File            &file,
	                        Vfs::Env::Io               &io)
	:
		Request_stream { rm, ds, ep, sigh, file.block_info(), file.view() },
		_ep   { ep },
		_file { file },
		_io   { io }
	{
		_ep.manage(*this);
	}

	~Block_session_component() { _ep.dissolve(*this); }

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }

	Block::Request _limit_transfer_size(Block::Request request) const
	{
		if (_file.transfer_block_count_limit())
			request.operation.count =
				min(request.operation.count,
				    _file.transfer_block_count_limit());

		return request;
	}

	void handle_request()
	{
		for (;;) {

			bool progress = false;

			with_requests([&] (Block::Request request) {

				using Response = Block::Request_stream::Response;

				if (!_file.acceptable())
					return Response::RETRY;

				if (!_file.valid(request))
					return Response::REJECTED;

				using Op = Block::Operation;
				bool const payload =
					Op::has_payload(request.operation.type);

				try {
					/*
					 * Restrict the maximum data transfer size
					 * artificially to aid in testing corner-cases.
					 */
					request = _limit_transfer_size(request);

					if (payload) {
						with_content(request,
						[&] (void *ptr, size_t size) {
							_file.submit(request, ptr, size);
						});
					} else {
						_file.submit(request, nullptr, 0);
					}
				} catch (Vfs_block::Job::Unsupported_Operation) {
					return Response::REJECTED; }

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

		_io.commit();

		wakeup_client_if_needed();
	}
};


struct Main : Rpc_object<Typed_root<Block::Session>>,
              private Vfs::Env::User
{
	Env &_env;

	Signal_handler<Main> _request_handler {
		_env.ep(), *this, &Main::_handle_requests };

	Heap                    _heap       { _env.ram(), _env.rm() };
	Attached_rom_dataspace  _config_rom { _env, "config" };

	Vfs::Simple_env _vfs_env = _config_rom.node().with_sub_node("vfs",
		[&] (Node const &config) -> Vfs::Simple_env {
			return { _env, _heap, config, *this }; },
		[&] () -> Vfs::Simple_env {
			error("VFS not configured");
			return { _env, _heap, Node() }; });

	struct Block_session : Genode::Registry<Block_session>::Element
	{
		Attached_ram_dataspace  _bulk_dataspace;
		Vfs_block::File         _file;
		Block_session_component _session_component;

		Block_session(Registry<Block_session>       &registry,
		              Vfs::Simple_env               &vfs_env,
		              Block::Constrained_view const &view,
		              size_t                         tx_buf_size,
		              Vfs_block::File_info           file_info,
		              Signal_handler<Main>          &request_handler)
		:
			Registry<Block_session>::Element { registry, *this },

			_bulk_dataspace    { vfs_env.env().ram(), vfs_env.env().rm(),
			                     tx_buf_size },
			_file              { vfs_env.alloc(), vfs_env.root_dir(),
			                     file_info, view },
			_session_component { vfs_env.env().rm(), vfs_env.env().ep(),
			                     _bulk_dataspace.cap(), request_handler,
			                     _file, vfs_env.io() }
		{ }

		void handle_request() {
			_session_component.handle_request(); }

		Capability<Block::Session> cap() const {
			return _session_component.cap(); }
	};

	Registry<Block_session> _sessions { };

	void _handle_requests()
	{
		_sessions.for_each([&] (Block_session &session) {
			session.handle_request(); });
	}

	/*
	 * Vfs::Env::User interface
	 */
	void wakeup_vfs_user() override
	{
		_request_handler.local_submit();
	}

	/*
	 * Root interface
	 */

	Root::Result session(Root::Session_args const &args,
	                     Affinity const &) override
	{
		size_t const tx_buf_size =
			Arg_string::find_arg(args.string(),
			                     "tx_buf_size").aligned_size();

		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		if (tx_buf_size > ram_quota.value) {
			warning("communication buffer size exceeds session quota");
			return Session_error::INSUFFICIENT_RAM;
		}

		/* make sure policy is up-to-date */
		_config_rom.update();

		return with_matching_policy(label_from_args(args.string()), _config_rom.node(),

			[&] (Node const &policy) -> Root::Result {

				if (!policy.has_attribute("file")) {
					error("policy lacks 'file' attribute");
					return Session_error::DENIED;
				}

				bool const writeable_policy =
					policy.attribute_value("writeable", false);

				Vfs_block::File_info const file_info =
					Vfs_block::file_info_from_policy(policy);

				Block::Constrained_view view =
					Block::Constrained_view::from_args(args.string());
				view.writeable = writeable_policy && view.writeable;

				try {
					Block_session const &session =
						*new (_heap) Block_session(_sessions,
						                           _vfs_env, view, tx_buf_size,
						                           file_info, _request_handler);
					return { session.cap() };

				} catch (...) { return Session_error::DENIED; }
			},
			[&] () -> Root::Result { return Session_error::DENIED; });
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const &) override { }

	void close(Capability<Session> cap) override
	{
		_sessions.for_each([&] (Block_session &session) {
			if (cap == session.cap())
				destroy(_heap, &session);
		});
	}

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(*this));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
