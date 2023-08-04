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
#include <vfs/dir_file_system.h>
#include <vfs/single_file_system.h>
#include <util/arg_string.h>
#include <util/xml_generator.h>
#include <trace/timestamp.h>

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/client_data.h>
#include <tresor/crypto.h>
#include <tresor/free_tree.h>
#include <tresor/meta_tree.h>
#include <tresor/request_pool.h>
#include <tresor/superblock_control.h>
#include <tresor/trust_anchor.h>
#include <tresor/virtual_block_device.h>

#include "splitter.h"

namespace Vfs_tresor {
	using namespace Vfs;
	using namespace Genode;
	using namespace Tresor;

	class Data_file_system;

	class Extend_file_system;
	class Extend_progress_file_system;
	class Rekey_file_system;
	class Rekey_progress_file_system;
	class Deinitialize_file_system;
	class Create_snapshot_file_system;
	class Discard_snapshot_file_system;

	struct Control_local_factory;
	class  Control_file_system;

	struct Snapshot_local_factory;
	class  Snapshot_file_system;

	struct Snapshots_local_factory;
	class  Snapshots_file_system;

	struct Local_factory;
	class  File_system;

	class Client_data;
	class Wrapper;

	template <typename T>
	class Pointer
	{
		private:

			T *_obj;

		public:

			struct Invalid : Genode::Exception { };

			Pointer() : _obj(nullptr) { }

			Pointer(T &obj) : _obj(&obj) { }

			T &obj() const
			{
				if (_obj == nullptr)
					throw Invalid();

				return *_obj;
			}

			bool valid() const { return _obj != nullptr; }
	};
} /* namespace Vfs_tresor */


class Vfs_tresor::Client_data : public Tresor::Module, public Tresor::Module_channel
{
	private:

		using Request = Client_data_request;

		Lookup_buffer &_lookup;

		NONCOPYABLE(Client_data);

		void _request_submitted(Module_request &mod_req) override
		{
			Request &req { *static_cast<Request *>(&mod_req) };
			switch (req._type) {
			case Request::OBTAIN_PLAINTEXT_BLK:
			{
				req._blk = _lookup.src_for_writing_vba(req._req_tag, req._vba);
				req._success = true;
				break;
			}
			case Request::SUPPLY_PLAINTEXT_BLK:
			{
				_lookup.dst_for_reading_vba(req._req_tag, req._vba) = req._blk;
				req._success = true;
				break;
			} }
		}

		bool _request_complete() override { return true; }

	public:

		Client_data(Lookup_buffer &lb) : Module_channel { CLIENT_DATA, 0 }, _lookup { lb } { add_channel(*this);}
};


class Vfs_tresor::Wrapper
:
	private Tresor::Module_composition,
	public  Tresor::Module
{
	private:

		NONCOPYABLE(Wrapper);

		Vfs::Env &_vfs_env;

		Constructible<Request_pool>            _request_pool { };
		Constructible<Tresor::Free_tree>       _free_tree    { };
		Constructible<Virtual_block_device>    _vbd          { };
		Constructible<Superblock_control>      _sb_control   { };
		Tresor::Meta_tree                      _meta_tree    { };
		Constructible<Tresor::Trust_anchor>    _trust_anchor { };
		Constructible<Tresor::Crypto>          _crypto       { };
		Constructible<Tresor::Block_io>        _block_io     { };

		Constructible<Tresor::Splitter>        _splitter     { };
		Constructible<Client_data>             _client_data  { };


	public:

		enum class Result { UNKNOWN, OK, ERROR, EOF };


	private:

		class Command : public Module_channel
		{
			public:

				using Operation = Tresor::Request::Operation;

				enum State { IDLE, PENDING, IN_PROGRESS, COMPLETED };

				static char const *op_to_string(Command::Operation op) {
					return Tresor::Request::op_to_string(op); }

			private:

				NONCOPYABLE(Command);

				Vfs_tresor::Wrapper &_main;

				State _state { IDLE };
				bool  _success { false };

				void _generated_req_completed(State_uint) override { _main.mark_command_completed(id()); }

			public:

				Result           result { Result::UNKNOWN };
				Operation        op     { Operation::READ };
				Number_of_blocks count  { 0 };
				Generation       gen    { 0 };
				Key_id           key_id { 0 };

				/* for READ/WRITE */
				Genode::uint64_t  offset           { 0 };
				char             *buffer_start     { nullptr };
				size_t            buffer_num_bytes { 0 };

				Command(Vfs_tresor::Wrapper &main, Module_channel_id id)
				: Module_channel { COMMAND_POOL, id }, _main { main } { }

				void reset()
				{
					_success = false;

					result = Result::UNKNOWN;
					op = Operation::READ;
					count = 0;
					gen = 0;
					key_id = 0;
					offset = 0;
					buffer_start = nullptr;
					buffer_num_bytes = 0;
				}

				bool success() const { return _success; }

				bool eof(Virtual_block_address max) const {
					return offset / Tresor::BLOCK_SIZE > max; }

				bool synchronize() const { return op == Operation::SYNC; }

				State state() const { return _state; }

				void state(State state)
				{
					ASSERT(state != _state);
					_state = state;
				}

				void execute(bool verbose, bool &progress)
				{
					ASSERT(_state == State::PENDING);

					if (verbose)
						log("Execute request ", *this);

					switch (op) {
					case Command::Operation::READ:
						generate_req<Splitter_request>(State::COMPLETED,
							progress, Splitter_request::Operation::READ, _success,
							offset, Byte_range_ptr(buffer_start, buffer_num_bytes), key_id, gen);
						break;
					case Command::Operation::WRITE:
						generate_req<Splitter_request>(State::COMPLETED,
							progress, Splitter_request::Operation::WRITE, _success,
							offset, Byte_range_ptr(buffer_start, buffer_num_bytes), key_id, gen);
						break;
					default:
						generate_req<Tresor::Request>(State::COMPLETED,
							progress, op, 0, 0, count, key_id, id(), gen, _success);
						break;
					}

					_main.mark_command_in_progress(id());
				}

				void print(Genode::Output &out) const
				{
					Genode::print(out, "op: ", op_to_string(op), " "
					                   "count: ", count, " "
					                   "gen: ", gen, " "
					                   "key_id: ", key_id, " "
					                   "offset: ", offset, " "
					                   "buffer_start: ", (void*)buffer_start, " "
					                   "buffer_num_bytes: ", buffer_num_bytes);
				}
		};

		template <typename FUNC>
		void _with_first_processable_cmd(FUNC && func)
		{
			bool first_uncompleted_cmd { true };
			bool done { false };
			for_each_channel<Command>([&] (Command &cmd) {

				if (done)
					return;

				if (cmd.state() == Command::PENDING) {
					done = true;
					if (first_uncompleted_cmd || !cmd.synchronize())
						func(cmd);
				}

				if (cmd.state() == Command::IN_PROGRESS) {
					if (cmd.synchronize())
						done = true;
					else
						first_uncompleted_cmd = false;
				}
			});
		}

		template <typename FUNC>
		bool _with_first_idle_cmd(FUNC && func)
		{
			bool done { false };
			for_each_channel<Command>([&] (Command &cmd) {
				if (done)
					return;

				if (cmd.state() == Command::IDLE) {
					done = true;
					/*
					 * Always provide a fresh Command to ease burden on
					 * the callee and set it PENDING afterwards as
					 * 'func' may not fail.
					 */
					cmd.reset();
					func(cmd);
					cmd.state(Command::PENDING);
				}
			});
			return done;
		}

		enum { MAX_NUM_COMMANDS = 16 };
		Constructible<Command> _commands[MAX_NUM_COMMANDS] { };

		bool ready_to_submit_request()
		{
			bool result = false;
			for_each_channel<Command>([&] (Command const &cmd) {
				if (cmd.state() == Command::State::IDLE)
					result = true;
			});
			return result;
		}

	public:

		struct Control_request
		{
			enum State { UNKNOWN, IDLE, IN_PROGRESS, };
			enum Result { NONE, SUCCESS, FAILED, };

			State  state;
			Result last_result;

			Control_request() : state { State::UNKNOWN }, last_result { Result::NONE } { }

			bool idle()        const { return state == IDLE; }
			bool in_progress() const { return state == IN_PROGRESS; }

			bool success()     const { return last_result == SUCCESS; }

			void mark_in_progress()
			{
				state       = State::IN_PROGRESS;
				last_result = Result::NONE;
			}

			static char const *state_to_cstring(State const s)
			{
				switch (s) {
				case State::UNKNOWN:     return "unknown";
				case State::IDLE:        return "idle";
				case State::IN_PROGRESS: return "in-progress";
				}
				return "-";
			}
		};

		struct Rekeying : Control_request
		{
			uint32_t              key_id;
			Virtual_block_address max_vba;
			Virtual_block_address rekeying_vba;
			uint64_t              percent_done;

			Rekeying() : Control_request { }, key_id { 0 }, max_vba { 0 },
			             rekeying_vba { 0 }, percent_done { 0 } { }

			void mark_in_progress(Virtual_block_address max,
			                      Virtual_block_address rekeying)
			{
				max_vba      = max;
				rekeying_vba = rekeying;
				Control_request::mark_in_progress();
			}
		};

		struct Deinitialize : Control_request
		{
			uint32_t key_id;

			Deinitialize() : Control_request { }, key_id { 0 }
			{
				state = State::IDLE;
			}
		};

		struct Extending : Control_request
		{
			enum Type { INVALID, VBD, FT };

			Type                  type;
			Virtual_block_address resizing_nr_of_pbas;
			uint64_t              percent_done;

			Extending() : Control_request { }, type { Type::INVALID},
			              resizing_nr_of_pbas { 0 }, percent_done { 0 } { }

			void mark_in_progress(Type type, Virtual_block_address resizing_nr_of_pbas)
			{
				type                = type;
				resizing_nr_of_pbas = resizing_nr_of_pbas;
				Control_request::mark_in_progress();
			}

			static Type string_to_type(char const *s)
			{
				if (Genode::strcmp("vbd", s, 3) == 0) {
					return Type::VBD;
				} else

				if (Genode::strcmp("ft", s, 2) == 0) {
					return Type::FT;
				}

				return Type::INVALID;
			}

			static char const *type_to_string(Type type)
			{
				switch (type) {
				case Type::VBD:     return "vbd";
				case Type::FT:      return "ft";
				case Type::INVALID: return "invalid";
				}
				return nullptr;
			}
		};

	private:

		/*
		 * XXX The initial object state of Rekeying and
		 *     Extending relies on 'execute()' querying
		 *     the Superblock_info to switch from UNKNOWN
		 *     to the current state and could therefore
		 *     deny attempts.
		 */
		Rekeying     _rekey_obj  { };
		Extending    _extend_obj { };
		Deinitialize _deinit_obj { };

		Genode::Mutex     _io_mutex { };
		Vfs_handle const *_io_handle_ptr { nullptr };
		Command          *_io_cmd_ptr    { nullptr };

		bool _active_io_cmd_for_handle(Vfs_handle const &handle) const {
			return _io_handle_ptr == &handle; }

		template <typename SETUP_FN>
		bool _setup_io_cmd_for_handle(Vfs_handle const &handle,
		                              SETUP_FN   const &setup_fn)
		{
			ASSERT(_io_handle_ptr == nullptr);

			bool done = _with_first_idle_cmd([&] (Command &cmd) {
				setup_fn(cmd);

				_io_cmd_ptr    = &cmd;
				_io_handle_ptr = &handle;
			});
			return done;
		}

		template <typename PENDING_FN, typename COMPLETE_FN>
		bool _with_io_active_cmd_for_handle(Vfs_handle  const &handle,
		                                    PENDING_FN  const &pending_fn,
		                                    COMPLETE_FN const &complete_fn)
		{
			bool found = false;
			if (_io_handle_ptr && _io_handle_ptr == &handle) {
				if (_io_cmd_ptr) {
					Command &cmd = *_io_cmd_ptr;

					switch (cmd.state()) {
					case Command::State::IDLE:
						/* should never happen */
						break;
					case Command::State::PENDING: [[fallthrough]];
					case Command::State::IN_PROGRESS:
						pending_fn();
						break;
					case Command::State::COMPLETED:
						complete_fn(*_io_cmd_ptr);
						cmd.state(Command::IDLE);

						_io_cmd_ptr    = nullptr;
						_io_handle_ptr = nullptr;
						break;
					}
					found = true;
				}
			}
			return found;
		}

		Pointer<Snapshots_file_system>       _snapshots_fs       { };
		Pointer<Extend_file_system>          _extend_fs          { };
		Pointer<Extend_progress_file_system> _extend_progress_fs { };
		Pointer<Rekey_file_system>           _rekey_fs           { };
		Pointer<Rekey_progress_file_system>  _rekey_progress_fs  { };
		Pointer<Deinitialize_file_system>    _deinit_fs          { };

		/* configuration options */
		bool _verbose       { false };
		bool _debug         { false };

		void _read_config(Xml_node config)
		{
			_verbose      = config.attribute_value("verbose", _verbose);
			_debug        = config.attribute_value("debug",   _debug);
		}

		struct Could_not_open_block_backend : Genode::Exception { };
		struct No_valid_superblock_found    : Genode::Exception { };

		void _initialize_tresor()
		{
			_free_tree.construct();
			_vbd.construct();
			_sb_control.construct();
			_request_pool.construct();

			add_module(FREE_TREE, *_free_tree);
			add_module(VIRTUAL_BLOCK_DEVICE, *_vbd);
			add_module(SUPERBLOCK_CONTROL, *_sb_control);
			add_module(REQUEST_POOL, *_request_pool);

			Module_channel_id id = 0;
			for (auto & cmd : _commands) {
				cmd.construct(*this, id++);
				add_channel(*cmd);
			}
		}

		void _process_completed(Command &cmd)
		{
			using R = Result;

			bool const success = cmd.success();

			if (_verbose)
				log("Completed request ", cmd, " ",
				    success ? "successfull" : "failed");

			switch (cmd.op) {
			case Command::Operation::REKEY:
			{
				_rekey_obj.state = Rekeying::State::IDLE;
				_rekey_obj.last_result = success ? Rekeying::Result::SUCCESS
				                                 : Rekeying::Result::FAILED;

				_rekey_fs_trigger_watch_response();
				_rekey_progress_fs_trigger_watch_response();

				cmd.state(Command::IDLE);
				break;
			}
			case Command::Operation::DEINITIALIZE:
			{
				_deinit_obj.state = Deinitialize::State::IDLE;
				_deinit_obj.last_result = success ? Deinitialize::Result::SUCCESS
				                                  : Deinitialize::Result::FAILED;

				_deinit_fs_trigger_watch_response();

				cmd.state(Command::IDLE);
				break;
			}
			case Command::Operation::EXTEND_VBD:
			{
				_extend_obj.state = Extending::State::IDLE;
				_extend_obj.last_result =
					success ? Extending::Result::SUCCESS
					        : Extending::Result::FAILED;

				_extend_fs_trigger_watch_response();
				_extend_progress_fs_trigger_watch_response();

				cmd.state(Command::IDLE);
				break;
			}
			case Command::Operation::EXTEND_FT:
			{
				_extend_obj.state = Extending::State::IDLE;
				_extend_obj.last_result =
					success ? Extending::Result::SUCCESS
					        : Extending::Result::FAILED;

				_extend_fs_trigger_watch_response();

				cmd.state(Command::IDLE);
				break;
			}
			case Command::Operation::CREATE_SNAPSHOT:
			{
				// FIXME more TODO here?
				_snapshots_fs_update_snapshot_registry();

				cmd.state(Command::IDLE);
				break;
			}
			case Command::Operation::DISCARD_SNAPSHOT:
			{
				// FIXME more TODO here?
				_snapshots_fs_update_snapshot_registry();

				cmd.state(Command::IDLE);
				break;
			}
			case Command::Operation::READ: [[fallthrough]];
			case Command::Operation::WRITE:
			{
				bool const eof = cmd.eof(_sb_control->max_vba());

				cmd.result = success ? R::OK
				                     : eof ? R::EOF
				                           : R::ERROR;
				break;
			}
			case Command::Operation::SYNC:
				cmd.result = success ? R::OK : R::ERROR;
				break;
			/* not handled here */
			case Command::Operation::RESUME_REKEYING:
				cmd.result = success ? R::OK : R::ERROR;
				break;
			case Command::Operation::INITIALIZE:
				cmd.result = success ? R::OK : R::ERROR;
				break;
			} /* switch */
		}

		void _snapshots_fs_update_snapshot_registry();

		void _extend_fs_trigger_watch_response();

		void _extend_progress_fs_trigger_watch_response();

		void _rekey_fs_trigger_watch_response();

		void _rekey_progress_fs_trigger_watch_response();

		void _deinit_fs_trigger_watch_response();

		template <typename FN>
		void _with_node(char const *name, char const *path, FN const &fn)
		{
			char xml_buffer[128] { };

			Genode::Xml_generator xml {
				xml_buffer, sizeof(xml_buffer), name,
				[&] { xml.attribute("path", path); }
			};

			Genode::Xml_node node { xml_buffer, sizeof(xml_buffer) };
			fn(node);
		}

	public:

		Wrapper(Vfs::Env &vfs_env, Xml_node config) : _vfs_env { vfs_env }
		{
			_read_config(config);

			using S = Genode::String<32>;

			S const block_path =
				config.attribute_value("block", S());
			if (block_path.valid())
				_with_node("block_io", block_path.string(),
					[&] (Xml_node const &node) {
						_block_io.construct(vfs_env, node);
					});

			S const trust_anchor_path =
				config.attribute_value("trust_anchor", S());
			if (trust_anchor_path.valid())
				_with_node("trust_anchor", trust_anchor_path.string(),
					[&] (Xml_node const &node) {
						_trust_anchor.construct(vfs_env, node);
					});

			S const crypto_path =
				config.attribute_value("crypto", S());
			if (crypto_path.valid())
				_with_node("crypto", crypto_path.string(),
					[&] (Xml_node const &node) {
						_crypto.construct(vfs_env, node);
					});

			_splitter.construct();
			_client_data.construct(*_splitter);

			add_module(COMMAND_POOL,  *this);
			add_module(META_TREE,     _meta_tree);
			add_module(CRYPTO,        *_crypto);
			add_module(TRUST_ANCHOR,  *_trust_anchor);
			add_module(CLIENT_DATA,   *_client_data);
			add_module(BLOCK_IO,      *_block_io);
			add_module(SPLITTER,      *_splitter);

			_initialize_tresor();
		}

		void mark_command_in_progress(Module_channel_id cmd_id)
		{
			with_channel<Command>(cmd_id, [&] (Command &cmd) {
				cmd.state(Command::IN_PROGRESS);
			});
		}

		void mark_command_completed(Module_channel_id cmd_id)
		{
			with_channel<Command>(cmd_id, [&] (Command &cmd) {
				cmd.state(Command::COMPLETED);
				_process_completed(cmd);
			});
		}

		void execute(bool &progress) override
		{
			_with_first_processable_cmd([&] (Command &cmd) {
				cmd.execute(_verbose, progress);
			});
		}

		Genode::uint64_t max_vba()
		{
			return _sb_control->max_vba();
		}

		/*
		 * Handle a I/O request
		 *
		 * We rely on the 'handle' as well as the memory covered by
		 * 'data' being valid throughout the processing of the pending
		 * request.
		 */
		template <typename PENDING_FN, typename COMPLETE_FN>
		void handle_io_request(Vfs_handle           const &handle,
		                       Byte_range_ptr       const &data,
		                       Tresor::Request::Operation  op,
		                       Generation                  gen,
		                       PENDING_FN           const &pending_fn,
		                       COMPLETE_FN          const &complete_fn)
		{

			Genode::Mutex::Guard guard { _io_mutex };

			/* queue new I/O request */
			if (!_active_io_cmd_for_handle(handle)) {
				bool const done =
					_setup_io_cmd_for_handle(handle, [&] (Command &cmd) {

					cmd.op               = op;
					cmd.gen              = gen;
					cmd.offset           = handle.seek();
					/* make a copy as the object may be dynamic */
					cmd.buffer_start     = data.start;
					cmd.buffer_num_bytes = data.num_bytes;
				});

				if (!done) {
					pending_fn();
					return;
				}
			}

			execute();

			_with_io_active_cmd_for_handle(handle,
				[&] { pending_fn(); },
				[&] (Command const &cmd) {
					complete_fn(cmd.result,
					            cmd.buffer_num_bytes);
			});
		}

		void execute()
		{
			execute_modules();
			_vfs_env.io().commit();

			Tresor::Superblock_info const sb_info {
				_sb_control->sb_info() };

			using ES = Extending::State;
			if (_extend_obj.state == ES::UNKNOWN && sb_info.valid) {
				if (sb_info.extending_ft) {

					_extend_obj.state = ES::IN_PROGRESS;
					_extend_obj.type  = Extending::Type::FT;
				} else

				if (sb_info.extending_vbd) {

					_extend_obj.state = ES::IN_PROGRESS;
					_extend_obj.type  = Extending::Type::VBD;
				} else {

					_extend_obj.state = ES::IDLE;
				}
				_extend_fs_trigger_watch_response();
			}

			if (_extend_obj.in_progress()) {

				Virtual_block_address const current_nr_of_pbas =
					_sb_control->resizing_nr_of_pbas();

				/* initial query */
				if (_extend_obj.resizing_nr_of_pbas == 0)
					_extend_obj.resizing_nr_of_pbas = current_nr_of_pbas;

				/* update user-facing state */
				uint64_t const last_percent_done = _extend_obj.percent_done;
				_extend_obj.percent_done =
					(_extend_obj.resizing_nr_of_pbas - current_nr_of_pbas)
					* 100 / _extend_obj.resizing_nr_of_pbas;

				if (last_percent_done != _extend_obj.percent_done)
					_extend_progress_fs_trigger_watch_response();
			}

			using RS = Rekeying::State;
			if (_rekey_obj.state == RS::UNKNOWN && sb_info.valid) {
				_rekey_obj.state =
					sb_info.rekeying ? RS::IN_PROGRESS : RS::IDLE;

				_rekey_fs_trigger_watch_response();
			}

			if (_rekey_obj.in_progress()) {
				_rekey_obj.rekeying_vba = _sb_control->rekeying_vba();

				/* update user-facing state */
				uint64_t const last_percent_done = _rekey_obj.percent_done;
				_rekey_obj.percent_done =
					_rekey_obj.rekeying_vba * 100 / _rekey_obj.max_vba;

				if (last_percent_done != _rekey_obj.percent_done)
					_rekey_progress_fs_trigger_watch_response();
			}
		}

		bool start_rekeying()
		{
			if (!ready_to_submit_request())
				return false;

			bool result = _with_first_idle_cmd([&] (Command &cmd) {

				cmd.op = Command::Operation::REKEY;
				cmd.key_id = _rekey_obj.key_id;

				_rekey_obj.mark_in_progress(_sb_control->max_vba(),
				                            _sb_control->rekeying_vba());

				_rekey_fs_trigger_watch_response();
				_rekey_progress_fs_trigger_watch_response();
			});

			execute();
			return result;
		}

		Rekeying const rekeying_progress() const {
			return _rekey_obj; }

		bool start_deinitialize()
		{
			if (!ready_to_submit_request())
				return false;

			bool result = _with_first_idle_cmd([&] (Command &cmd) {

				cmd.op = Command::Operation::DEINITIALIZE;

				_deinit_obj.mark_in_progress();
				_deinit_fs_trigger_watch_response();
			});

			execute();
			return result;
		}

		Deinitialize const deinitialize_progress() const {
			return _deinit_obj; }

		bool start_extending(Extending::Type       type,
		                     Tresor::Number_of_blocks blocks)
		{
			if (!ready_to_submit_request() || type == Extending::Type::INVALID)
				return false;

			Command::Operation op = Command::Operation::EXTEND_VBD;

			switch (type) {
			case Extending::Type::VBD:
				op = Command::Operation::EXTEND_VBD;
				break;
			case Extending::Type::FT:
				op = Command::Operation::EXTEND_FT;
				break;
			case Extending::Type::INVALID:
				/* never reached */
				return false;
			}

			bool result = _with_first_idle_cmd([&] (Command &cmd) {

				cmd.op    = op;
				cmd.count = blocks;

				_extend_obj.mark_in_progress(type, 0);

				_extend_fs_trigger_watch_response();
				_extend_progress_fs_trigger_watch_response();
			});

			execute();
			return result;
		}

		Extending const extending_progress() const {
			return _extend_obj; }

		void snapshots_info(Tresor::Snapshots_info &info)
		{
			info = _sb_control->snapshots_info();
			execute();
		}

		bool create_snapshot()
		{
			if (!ready_to_submit_request())
				return false;

			bool result = _with_first_idle_cmd([&] (Command &cmd) {
				cmd.op = Command::Operation::CREATE_SNAPSHOT; });

			execute();
			return result;
		}

		bool discard_snapshot(Generation snap_gen)
		{
			if (!ready_to_submit_request())
				return false;

			bool result = _with_first_idle_cmd([&] (Command &cmd) {
				cmd.op  = Command::Operation::DISCARD_SNAPSHOT;
				cmd.gen = snap_gen;
			});

			execute();
			return result;
		}

		/***********************************************************
		 ** Manange/Disolve interface needed for FS notifications **
		 ***********************************************************/

		void manage_snapshots_file_system(Snapshots_file_system &snapshots_fs)
		{
			if (_snapshots_fs.valid()) {

				class Already_managing_an_snapshots_file_system { };
				throw Already_managing_an_snapshots_file_system { };
			}
			_snapshots_fs = snapshots_fs;
		}

		void dissolve_snapshots_file_system(Snapshots_file_system &snapshots_fs)
		{
			if (_snapshots_fs.valid()) {

				if (&_snapshots_fs.obj() != &snapshots_fs) {

					class Snapshots_file_system_not_managed { };
					throw Snapshots_file_system_not_managed { };
				}
				_snapshots_fs = Pointer<Snapshots_file_system> { };

			} else {

				class No_snapshots_file_system_managed { };
				throw No_snapshots_file_system_managed { };
			}
		}

		void manage_extend_file_system(Extend_file_system &extend_fs)
		{
			if (_extend_fs.valid()) {

				class Already_managing_an_extend_file_system { };
				throw Already_managing_an_extend_file_system { };
			}
			_extend_fs = extend_fs;
		}

		void dissolve_extend_file_system(Extend_file_system &extend_fs)
		{
			if (_extend_fs.valid()) {

				if (&_extend_fs.obj() != &extend_fs) {

					class Extend_file_system_not_managed { };
					throw Extend_file_system_not_managed { };
				}
				_extend_fs = Pointer<Extend_file_system> { };

			} else {

				class No_extend_file_system_managed { };
				throw No_extend_file_system_managed { };
			}
		}

		void manage_extend_progress_file_system(Extend_progress_file_system &extend_progress_fs)
		{
			if (_extend_progress_fs.valid()) {

				class Already_managing_an_extend_progres_file_system { };
				throw Already_managing_an_extend_progres_file_system { };
			}
			_extend_progress_fs = extend_progress_fs;
		}

		void dissolve_extend_progress_file_system(Extend_progress_file_system &extend_progress_fs)
		{
			if (_extend_progress_fs.valid()) {

				if (&_extend_progress_fs.obj() != &extend_progress_fs) {

					class Extend_file_system_not_managed { };
					throw Extend_file_system_not_managed { };
				}
				_extend_progress_fs = Pointer<Extend_progress_file_system> { };

			} else {

				class No_extend_file_system_managed { };
				throw No_extend_file_system_managed { };
			}
		}

		void manage_rekey_file_system(Rekey_file_system &rekey_fs)
		{
			if (_rekey_fs.valid()) {

				class Already_managing_an_rekey_file_system { };
				throw Already_managing_an_rekey_file_system { };
			}
			_rekey_fs = rekey_fs;
		}

		void dissolve_rekey_file_system(Rekey_file_system &rekey_fs)
		{
			if (_rekey_fs.valid()) {

				if (&_rekey_fs.obj() != &rekey_fs) {

					class Rekey_file_system_not_managed { };
					throw Rekey_file_system_not_managed { };
				}
				_rekey_fs = Pointer<Rekey_file_system> { };

			} else {

				class No_rekey_file_system_managed { };
				throw No_rekey_file_system_managed { };
			}
		}

		void manage_rekey_progress_file_system(Rekey_progress_file_system &rekey_progress_fs)
		{
			if (_rekey_progress_fs.valid()) {

				class Already_managing_an_rekey_progress_file_system { };
				throw Already_managing_an_rekey_progress_file_system { };
			}
			_rekey_progress_fs = rekey_progress_fs;
		}

		void dissolve_rekey_progress_file_system(Rekey_progress_file_system &rekey_progress_fs)
		{
			if (_rekey_progress_fs.valid()) {

				if (&_rekey_progress_fs.obj() != &rekey_progress_fs) {

					class Rekey_progress_file_system_not_managed { };
					throw Rekey_progress_file_system_not_managed { };
				}
				_rekey_progress_fs = Pointer<Rekey_progress_file_system> { };

			} else {

				class No_rekey_progress_file_system_managed { };
				throw No_rekey_progress_file_system_managed { };
			}
		}

		void manage_deinit_file_system(Deinitialize_file_system &deinit_fs)
		{
			if (_deinit_fs.valid()) {

				class Already_managing_an_deinit_file_system { };
				throw Already_managing_an_deinit_file_system { };
			}
			_deinit_fs = deinit_fs;
		}

		void dissolve_deinit_file_system(Deinitialize_file_system &deinit_fs)
		{
			if (_deinit_fs.valid()) {

				if (&_deinit_fs.obj() != &deinit_fs) {

					class Deinitialize_file_system_not_managed { };
					throw Deinitialize_file_system_not_managed { };
				}
				_deinit_fs = Pointer<Deinitialize_file_system> { };

			} else {

				class No_deinit_file_system_managed { };
				throw No_deinit_file_system_managed { };
			}
		}

};


class Vfs_tresor::Data_file_system : public Single_file_system
{
	private:

		Wrapper &_w;
		Generation _snap_gen;

		using OP = Tresor::Request::Operation;
		using FR = Wrapper::Result;
		using RR = Vfs::File_io_service::Read_result;
		using SR = Vfs::File_io_service::Sync_result;
		using WR = Vfs::File_io_service::Write_result;

		static RR read_result(FR r)
		{
			switch (r) {
			case FR::OK:      return RR::READ_OK;
			case FR::EOF:     return RR::READ_OK;
			case FR::ERROR:   return RR::READ_ERR_IO;
			case FR::UNKNOWN: return RR::READ_ERR_INVALID;
			}
			return RR::READ_ERR_INVALID;
		}

		static SR sync_result(FR r)
		{
			switch (r) {
			case FR::OK:      return SR::SYNC_OK;
			case FR::EOF:     return SR::SYNC_ERR_INVALID;
			case FR::ERROR:   return SR::SYNC_ERR_INVALID;
			case FR::UNKNOWN: return SR::SYNC_ERR_INVALID;
			}
			return SR::SYNC_ERR_INVALID;
		}

		static WR write_result(FR r)
		{
			switch (r) {
			case FR::OK:      return WR::WRITE_OK;
			case FR::EOF:     return WR::WRITE_OK;
			case FR::ERROR:   return WR::WRITE_ERR_IO;
			case FR::UNKNOWN: return WR::WRITE_ERR_INVALID;
			}
			return WR::WRITE_ERR_INVALID;
		}


	public:

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;
			Generation _snap_gen { INVALID_GENERATION };

			Vfs_handle(Directory_service &ds,
			           File_io_service &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w,
			           Generation snap_gen)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w), _snap_gen(snap_gen)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				RR result = RR::READ_ERR_INVALID;
				_w.handle_io_request(*this, dst, OP::READ, _snap_gen,
					[&] { result = READ_QUEUED; },
					[&] (FR fresult, size_t count) {
						result    = read_result(fresult);
						out_count = count;
					}
				);
				return result;
			}

			Write_result write(Const_byte_range_ptr const &src,
			                   size_t &out_count) override
			{
				WR result = WR::WRITE_ERR_INVALID;
				_w.handle_io_request(*this,
				                     Byte_range_ptr(const_cast<char*>(src.start),
				                                    src.num_bytes),
				                     OP::WRITE, _snap_gen,
					[&] { result = WRITE_ERR_WOULD_BLOCK; },
					[&] (FR fresult, size_t count) {
						result    = write_result(fresult);
						out_count = count;
					}
				);
				return result;
			}

			Sync_result sync() override
			{
				SR result = SR::SYNC_ERR_INVALID;
				_w.handle_io_request(*this,
				                     Byte_range_ptr(nullptr, 0),
				                     OP::SYNC, 0,
					[&] { result = SYNC_QUEUED; },
					[&] (FR fresult, size_t) {
						result = sync_result(fresult);
					}
				);
				return result;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

		Data_file_system(Wrapper &w, Generation snap_gen)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<data/>")),
			_w(w), _snap_gen(snap_gen)
		{ }

		~Data_file_system()
		{
			/* XXX sync on close */
			/* XXX invalidate any still pending request */
		}

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);

			/* max_vba range is from 0 ... N - 1 */
			out.size = (_w.max_vba() + 1) * Tresor::BLOCK_SIZE;
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }

		/***************************
		 ** File-system interface **
		 ***************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle =
				new (alloc) Vfs_handle(*this, *this, alloc, _w, _snap_gen);

			return OPEN_OK;
		}

		static char const *type_name() { return "data"; }
		char const *type() override { return type_name(); }
};


class Vfs_tresor::Extend_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Wrapper &_w;

		using Content_string = String<32>;

		static file_size copy_content(Content_string const &content,
		                              char *dst, size_t const count)
		{
			copy_cstring(dst, content.string(), count);
			size_t const length_without_nul = content.length() - 1;
			return count > length_without_nul - 1 ? length_without_nul
			                                      : count;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				/* EOF */
				if (seek() != 0) {
					out_count = 0;
					return READ_OK;
				}

				/*
				 * For now trigger extending execution via this hook
				 * like we do in the Data_file_system.
				 */
				_w.execute();

				Wrapper::Extending const & extending {
					_w.extending_progress() };

				if (extending.in_progress())
					return READ_QUEUED;

				if (extending.idle()) {
					Content_string const content {
						extending.success() ? "successful"
						                    : "failed" };
					copy_content(content, dst.start, dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				using Type = Wrapper::Extending::Type;
				if (!_w.extending_progress().idle()) {
					return WRITE_ERR_IO;
				}

				char tree[16];
				Arg_string::find_arg(src.start, "tree").string(tree, sizeof (tree), "-");
				Type type = Wrapper::Extending::string_to_type(tree);
				if (type == Type::INVALID) {
					return WRITE_ERR_IO;
				}

				unsigned long blocks = Arg_string::find_arg(src.start, "blocks").ulong_value(0);
				if (blocks == 0) {
					return WRITE_ERR_IO;
				}

				bool const okay = _w.start_extending(type, blocks);
				if (!okay) {
					return WRITE_ERR_IO;
				}

				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Extend_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<extend/>")),
			_w(w)
		{
			_w.manage_extend_file_system(*this);
		}

		static char const *type_name() { return "extend"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::size();
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Extend_progress_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Wrapper &_w;

		using Content_string = String<32>;

		static file_size copy_content(Content_string const &content,
		                              char *dst, size_t const count)
		{
			copy_cstring(dst, content.string(), count);
			size_t const length_without_nul = content.length() - 1;
			return count > length_without_nul - 1 ? length_without_nul
			                                      : count;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(Byte_range_ptr const &dst,
			                 size_t               &out_count) override
			{
				/* EOF */
				if (seek() != 0) {
					out_count = 0;
					return READ_OK;
				}

				/*
				 * For now trigger extending execution via this hook
				 * like we do in the Data_file_system.
				 */
				_w.execute();

				Wrapper::Extending const & extending {
					_w.extending_progress() };

				if (extending.idle()) {
					Content_string const content { "idle" };
					copy_content(content, dst.start, dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

				if (extending.in_progress()) {
					char const * const type =
						Wrapper::Extending::type_to_string(extending.type);
					Content_string const content { type, " at ", extending.percent_done, "%" };
					copy_content(content, dst.start, dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &,
			                   size_t                     &) override
			{
				return WRITE_ERR_IO;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Extend_progress_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<extend_progress/>")),
			_w(w)
		{
			_w.manage_extend_progress_file_system(*this);
		}

		static char const *type_name() { return "extend_progress"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::size();
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Rekey_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Wrapper &_w;

		using Content_string = String<32>;

		static file_size copy_content(Content_string const &content,
		                              char *dst, size_t const count)
		{
			copy_cstring(dst, content.string(), count);
			size_t const length_without_nul = content.length() - 1;
			return count > length_without_nul - 1 ? length_without_nul
			                                      : count;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			/* store VBA in case the handle is kept open */
			Virtual_block_address _last_rekeying_vba;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w),
				_last_rekeying_vba(_w.rekeying_progress().rekeying_vba)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				/* EOF */
				if (seek() != 0) {
					out_count = 0;
					return READ_OK;
				}

				/*
				 * For now trigger rekeying execution via this hook
				 * like we do in the Data_file_system.
				 */
				_w.execute();

				Wrapper::Rekeying const & rekeying {
					_w.rekeying_progress() };

				if (rekeying.in_progress())
					return READ_QUEUED;

				if (rekeying.idle()) {
					Content_string const content {
						rekeying.success() ? "successful"
						                   : "failed" };
					copy_content(content, dst.start, dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				if (!_w.rekeying_progress().idle()) {
					return WRITE_ERR_IO;
				}

				bool start_rekeying { false };
				Genode::ascii_to(src.start, start_rekeying);

				if (!start_rekeying) {
					return WRITE_ERR_IO;
				}

				if (!_w.start_rekeying()) {
					return WRITE_ERR_IO;
				}

				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Rekey_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<rekey/>")),
			_w(w)
		{
			_w.manage_rekey_file_system(*this);
		}

		static char const *type_name() { return "rekey"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::size();
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Rekey_progress_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Wrapper &_w;

		using Content_string = String<32>;

		static file_size copy_content(Content_string const &content,
		                              char *dst, size_t const count)
		{
			copy_cstring(dst, content.string(), count);
			size_t const length_without_nul = content.length() - 1;
			return count > length_without_nul - 1 ? length_without_nul
			                                      : count;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(Byte_range_ptr const &dst,
			                 size_t               &out_count) override
			{
				/* EOF */
				if (seek() != 0) {
					out_count = 0;
					return READ_OK;
				}

				/*
				 * For now trigger rekeying execution via this hook
				 * like we do in the Data_file_system.
				 */
				_w.execute();

				Wrapper::Rekeying const & rekeying {
					_w.rekeying_progress() };

				if (rekeying.idle()) {
					Content_string const content { "idle" };
					copy_content(content, dst.start, dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

				if (rekeying.in_progress()) {
					Content_string const content { "at ", rekeying.percent_done, "%" };
					copy_content(content, dst.start, dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &,
			                   size_t                     &) override
			{
				return WRITE_ERR_IO;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Rekey_progress_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<rekey_progress/>")),
			_w(w)
		{
			_w.manage_rekey_progress_file_system(*this);
		}

		static char const *type_name() { return "rekey_progress"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::size();
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Deinitialize_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Wrapper &_w;

		using Content_string = String<32>;

		static Content_string content_string(Wrapper const &wrapper)
		{
			Wrapper::Deinitialize const & deinitialize_progress {
				wrapper.deinitialize_progress() };

			bool const in_progress { deinitialize_progress.in_progress() };

			bool const last_result {
				!in_progress &&
				deinitialize_progress.last_result !=
					Wrapper::Deinitialize::Result::NONE };

			bool const success { deinitialize_progress.success() };

			Content_string const result {
				Wrapper::Deinitialize::state_to_cstring(deinitialize_progress.state),
				" last-result:",
				last_result ? success ? "success" : "failed" : "none",
				"\n" };

			return result;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				if (seek() != 0) {
					out_count = 0;
					return READ_OK;
				}
				_w.execute();

				Wrapper::Deinitialize const & deinitialize_progress {
					_w.deinitialize_progress() };

				if (deinitialize_progress.in_progress())
					return READ_QUEUED;

				Content_string const result { content_string(_w) };
				copy_cstring(dst.start, result.string(), dst.num_bytes);
				out_count = dst.num_bytes;

				return READ_OK;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				if (!_w.deinitialize_progress().idle()) {
					return WRITE_ERR_IO;
				}

				bool start_deinitialize { false };
				Genode::ascii_to(src.start, start_deinitialize);

				if (!start_deinitialize) {
					return WRITE_ERR_IO;
				}

				if (!_w.start_deinitialize()) {
					return WRITE_ERR_IO;
				}

				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Deinitialize_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<deinitialize/>")),
			_w(w)
		{
			_w.manage_deinit_file_system(*this);
		}

		static char const *type_name() { return "deinitialize"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = content_string(_w).length() - 1;
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Create_snapshot_file_system : public Vfs::Single_file_system
{
	private:

		Wrapper &_w;

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(Byte_range_ptr const &, size_t &) override
			{
				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				bool create_snapshot { false };
				Genode::ascii_to(src.start, create_snapshot);
				Genode::String<64> str(Genode::Cstring(src.start, src.num_bytes));
				if (!create_snapshot)
					return WRITE_ERR_IO;

				if (!_w.create_snapshot()) {
					out_count = 0;
					return WRITE_OK;
				}
				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Create_snapshot_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::wo(), Xml_node("<create_snapshot/>")),
			_w(w)
		{ }

		static char const *type_name() { return "create_snapshot"; }

		char const *type() override { return type_name(); }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Discard_snapshot_file_system : public Vfs::Single_file_system
{
	private:

		Wrapper &_w;

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(Byte_range_ptr const &, size_t &) override
			{
				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src,
			                   size_t &out_count) override
			{
				out_count = 0;
				Generation snap_gen { INVALID_GENERATION };
				Genode::ascii_to(src.start, snap_gen);
				if (snap_gen == INVALID_GENERATION)
					return WRITE_ERR_IO;

				if (!_w.discard_snapshot(snap_gen)) {
					out_count = 0;
					return WRITE_OK;
				}
				return WRITE_ERR_IO;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Discard_snapshot_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::wo(), Xml_node("<discard_snapshot/>")),
			_w(w)
		{ }

		static char const *type_name() { return "discard_snapshot"; }

		char const *type() override { return type_name(); }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


struct Vfs_tresor::Snapshot_local_factory : File_system_factory
{
	Data_file_system _block_fs;

	Snapshot_local_factory(Vfs::Env & /* env */,
	                       Wrapper &tresor,
	                       Generation snap_gen)
	: _block_fs(tresor, snap_gen) { }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Data_file_system::type_name()))
			return &_block_fs;

		return nullptr;
	}
};


class Vfs_tresor::Snapshot_file_system : private Snapshot_local_factory,
                                         public Vfs::Dir_file_system
{
	private:

		Generation _snap_gen;

		typedef String<128> Config;

		static Config _config(Generation snap_gen, bool readonly)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {

				xml.attribute("name", !readonly ? String<16>("current") : String<16>(snap_gen));
				xml.node("data", [&] () {
					xml.attribute("readonly", readonly);
				});
			});

			return Config(Cstring(buf));
		}

	public:

		Snapshot_file_system(Vfs::Env &vfs_env,
		                    Wrapper &tresor,
		                    Generation snap_gen,
		                    bool readonly = false)
		:
			Snapshot_local_factory(vfs_env, tresor, snap_gen),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(snap_gen, readonly).string()), *this),
			_snap_gen(snap_gen)
		{ }

		static char const *type_name() { return "snapshot"; }

		char const *type() override { return type_name(); }

		Generation snap_gen() const { return _snap_gen; }
};


class Vfs_tresor::Snapshots_file_system : public Vfs::File_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Vfs::Env &_vfs_env;

		bool _root_dir(char const *path) { return strcmp(path, "/snapshots") == 0; }
		bool _top_dir(char const *path) { return strcmp(path, "/") == 0; }

		struct Snapshot_registry
		{
			Genode::Allocator                                          &_alloc;
			Wrapper                                                    &_wrapper;
			Snapshots_file_system                                      &_snapshots_fs;
			uint32_t                                                    _number_of_snapshots { 0 };
			Genode::Registry<Genode::Registered<Snapshot_file_system>>  _registry            { };

			struct Invalid_index : Genode::Exception { };
			struct Invalid_path  : Genode::Exception { };



			Snapshot_registry(Genode::Allocator     &alloc,
			                  Wrapper               &wrapper,
			                  Snapshots_file_system &snapshots_fs)
			:
				_alloc        { alloc },
				_wrapper      { wrapper },
				_snapshots_fs { snapshots_fs }
			{ }

			void update(Vfs::Env &vfs_env);

			uint32_t number_of_snapshots() const { return _number_of_snapshots; }

			Snapshot_file_system const &by_index(uint64_t idx) const
			{
				uint64_t i = 0;
				Snapshot_file_system const *fsp { nullptr };
				auto lookup = [&] (Snapshot_file_system const &fs) {
					if (i == idx) {
						fsp = &fs;
					}
					++i;
				};
				_registry.for_each(lookup);
				if (fsp == nullptr) {
					throw Invalid_index();
				}
				return *fsp;
			}

			Snapshot_file_system &_by_gen(Generation snap_gen)
			{
				Snapshot_file_system *fsp { nullptr };
				auto lookup = [&] (Snapshot_file_system &fs) {
					if (fs.snap_gen() == snap_gen) {
						fsp = &fs;
					}
				};
				_registry.for_each(lookup);
				if (fsp == nullptr)
					throw Invalid_path();

				return *fsp;
			}

			Snapshot_file_system &by_path(char const *path)
			{
				if (!path)
					throw Invalid_path();

				if (path[0] == '/')
					path++;

				Generation snap_gen { INVALID_GENERATION };
				Genode::ascii_to(path, snap_gen);
				return _by_gen(snap_gen);
			}
		};

	public:

		void update_snapshot_registry()
		{
			_snap_reg.update(_vfs_env);
		}

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_root_dir(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}

		struct Snap_vfs_handle : Vfs::Vfs_handle
		{
			using Vfs_handle::Vfs_handle;

			virtual Read_result read(Byte_range_ptr const &, size_t &out_count) = 0;

			virtual Write_result write(Const_byte_range_ptr const &, size_t &out_count) = 0;

			virtual Sync_result sync()
			{
				return SYNC_OK;
			}

			virtual bool read_ready() const = 0;
		};


		struct Dir_vfs_handle : Snap_vfs_handle
		{
			Snapshot_registry const &_snap_reg;

			bool const _root_dir { false };

			Read_result _query_snapshots(size_t  index,
			                             size_t &out_count,
			                             Dirent &out)
			{
				if (index >= _snap_reg.number_of_snapshots()) {
					out_count = sizeof(Dirent);
					out.type = Dirent_type::END;
					return READ_OK;
				}

				try {
					Snapshot_file_system const &fs = _snap_reg.by_index(index);
					Genode::String<32> name { fs.snap_gen() };

					out = {
						.fileno = (Genode::addr_t)this | index,
						.type   = Dirent_type::DIRECTORY,
						.rwx    = Node_rwx::rx(),
						.name   = { name.string() },
					};
					out_count = sizeof(Dirent);
					return READ_OK;
				} catch (Snapshot_registry::Invalid_index) {
					return READ_ERR_INVALID;
				}
			}

			Read_result _query_root(size_t  index,
			                        size_t &out_count,
			                        Dirent &out)
			{
				if (index == 0) {
					out = {
						.fileno = (Genode::addr_t)this,
						.type   = Dirent_type::DIRECTORY,
						.rwx    = Node_rwx::rx(),
						.name   = { "snapshots" }
					};
				} else {
					out.type = Dirent_type::END;
				}

				out_count = sizeof(Dirent);
				return READ_OK;
			}

			Dir_vfs_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Snapshot_registry const &snap_reg,
			               bool root_dir)
			:
				Snap_vfs_handle(ds, fs, alloc, 0),
				_snap_reg(snap_reg), _root_dir(root_dir)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				out_count = 0;

				if (dst.num_bytes < sizeof(Dirent))
					return READ_ERR_INVALID;

				size_t index = size_t(seek() / sizeof(Dirent));

				Dirent &out = *(Dirent*)dst.start;

				if (!_root_dir) {

					/* opended as "/snapshots" */
					return _query_snapshots(index, out_count, out);

				} else {
					/* opened as "/" */
					return _query_root(index, out_count, out);
				}
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override
			{
				return WRITE_ERR_INVALID;
			}

			bool read_ready() const override { return true; }
		};

		struct Dir_snap_vfs_handle : Vfs::Vfs_handle
		{
			Vfs_handle &vfs_handle;

			Dir_snap_vfs_handle(Directory_service &ds,
			                    File_io_service   &fs,
			                    Genode::Allocator &alloc,
			                    Vfs::Vfs_handle   &vfs_handle)
			: Vfs_handle(ds, fs, alloc, 0), vfs_handle(vfs_handle) { }

			~Dir_snap_vfs_handle()
			{
				vfs_handle.close();
			}
		};

		Snapshot_registry  _snap_reg;
		Wrapper           &_wrapper;

		char const *_sub_path(char const *path) const
		{
			/* skip heading slash in path if present */
			if (path[0] == '/') {
				path++;
			}

			Genode::size_t const name_len = strlen(type_name());
			if (strcmp(path, type_name(), name_len) != 0) {
				return nullptr;
			}

			path += name_len;

			/*
			 * The first characters of the first path element are equal to
			 * the current directory name. Let's check if the length of the
			 * first path element matches the name length.
			 */
			if (*path != 0 && *path != '/') {
				return 0;
			}

			return path;
		}


		Snapshots_file_system(Vfs::Env         &vfs_env,
		                      Genode::Xml_node  /* node */,
		                      Wrapper          &wrapper)
		:
			_vfs_env  { vfs_env },
			_snap_reg { vfs_env.alloc(), wrapper, *this },
			_wrapper  { wrapper }
		{
			_wrapper.manage_snapshots_file_system(*this);
		}

		static char const *type_name() { return "snapshots"; }

		char const *type() override { return type_name(); }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Dataspace_capability dataspace(char const * /* path */) override
		{
			return Genode::Dataspace_capability();
		}

		void release(char const * /* path */, Dataspace_capability) override
		{
		}

		Open_result open(char const       *path,
		                 unsigned          mode,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator        &alloc) override
		{
			path = _sub_path(path);
			if (!path || path[0] != '/') {
				return OPEN_ERR_UNACCESSIBLE;
			}

			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				return fs.open(path, mode, out_handle, alloc);
			} catch (Snapshot_registry::Invalid_path) { }

			return OPEN_ERR_UNACCESSIBLE;
		}

		Opendir_result opendir(char const       *path,
		                       bool              create,
		                       Vfs::Vfs_handle **out_handle,
		                       Allocator        &alloc) override
		{
			if (create) {
				return OPENDIR_ERR_PERMISSION_DENIED;
			}

			bool const top = _top_dir(path);
			if (_root_dir(path) || top) {
				_snap_reg.update(_vfs_env);

				*out_handle = new (alloc) Dir_vfs_handle(*this, *this, alloc,
				                                         _snap_reg, top);
				return OPENDIR_OK;
			} else {
				char const *sub_path = _sub_path(path);
				if (!sub_path) {
					return OPENDIR_ERR_LOOKUP_FAILED;
				}
				try {
					Snapshot_file_system &fs = _snap_reg.by_path(sub_path);
					Vfs::Vfs_handle *handle = nullptr;
					Opendir_result const res = fs.opendir(sub_path, create, &handle, alloc);
					if (res != OPENDIR_OK) {
						return OPENDIR_ERR_LOOKUP_FAILED;
					}
					*out_handle = new (alloc) Dir_snap_vfs_handle(*this, *this,
					                                              alloc, *handle);
					return OPENDIR_OK;
				} catch (Snapshot_registry::Invalid_path) { }
			}
			return OPENDIR_ERR_LOOKUP_FAILED;
		}

		void close(Vfs_handle *handle) override
		{
			if (handle && (&handle->ds() == this))
				destroy(handle->alloc(), handle);
		}

		Stat_result stat(char const *path, Stat &out_stat) override
		{
			out_stat = Stat { };
			path = _sub_path(path);

			/* path does not match directory name */
			if (!path) {
				return STAT_ERR_NO_ENTRY;
			}

			/*
			 * If path equals directory name, return information about the
			 * current directory.
			 */
			if (strlen(path) == 0 || _top_dir(path)) {

				out_stat.type   = Node_type::DIRECTORY;
				out_stat.inode  = 1;
				out_stat.device = (Genode::addr_t)this;
				return STAT_OK;
			}

			if (!path || path[0] != '/') {
				return STAT_ERR_NO_ENTRY;
			}

			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				Stat_result const res = fs.stat(path, out_stat);
				return res;
			} catch (Snapshot_registry::Invalid_path) { }

			return STAT_ERR_NO_ENTRY;
		}

		Unlink_result unlink(char const * /* path */) override
		{
			return UNLINK_ERR_NO_PERM;
		}

		Rename_result rename(char const * /* from */, char const * /* to */) override
		{
			return RENAME_ERR_NO_PERM;
		}

		file_size num_dirent(char const *path) override
		{
			if (_top_dir(path)) {
				return 1;
			}
			if (_root_dir(path)) {
				_snap_reg.update(_vfs_env);
				file_size const num = _snap_reg.number_of_snapshots();
				return num;
			}
			_snap_reg.update(_vfs_env);

			path = _sub_path(path);
			if (!path) {
				return 0;
			}
			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				file_size const num = fs.num_dirent(path);
				return num;
			} catch (Snapshot_registry::Invalid_path) {
				return 0;
			}
		}

		bool directory(char const *path) override
		{
			if (_root_dir(path)) {
				return true;
			}

			path = _sub_path(path);
			if (!path) {
				return false;
			}
			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				return fs.directory(path);
			} catch (Snapshot_registry::Invalid_path) { }

			return false;
		}

		char const *leaf_path(char const *path) override
		{
			path = _sub_path(path);
			if (!path) {
				return nullptr;
			}

			if (strlen(path) == 0 || strcmp(path, "") == 0) {
				return path;
			}

			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				char const *leaf_path = fs.leaf_path(path);
				if (leaf_path) {
					return leaf_path;
				}
			} catch (Snapshot_registry::Invalid_path) { }

			return nullptr;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs::Vfs_handle * /* vfs_handle */,
		                   Const_byte_range_ptr const &, size_t & /* out_count */) override
		{
			return WRITE_ERR_IO;
		}

		bool queue_read(Vfs::Vfs_handle *vfs_handle, size_t size) override
		{
			Dir_snap_vfs_handle *dh =
				dynamic_cast<Dir_snap_vfs_handle*>(vfs_handle);
			if (dh) {
				return dh->vfs_handle.fs().queue_read(&dh->vfs_handle, size);
			}

			return true;
		}

		Read_result complete_read(Vfs::Vfs_handle *vfs_handle,
		                          Byte_range_ptr const &dst,
		                          size_t & out_count) override
		{
			Snap_vfs_handle *sh =
				dynamic_cast<Snap_vfs_handle*>(vfs_handle);
			if (sh) {
				Read_result const res = sh->read(dst, out_count);
				return res;
			}

			Dir_snap_vfs_handle *dh =
				dynamic_cast<Dir_snap_vfs_handle*>(vfs_handle);
			if (dh) {
				return dh->vfs_handle.fs().complete_read(&dh->vfs_handle,
				                                         dst, out_count);
			}

			return READ_ERR_IO;
		}

		bool read_ready(Vfs::Vfs_handle const &) const override {
			return true; }

		bool write_ready(Vfs::Vfs_handle const &) const override {
			return false; }

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


struct Vfs_tresor::Control_local_factory : File_system_factory
{
	Wrapper                      &_wrapper;
	Rekey_file_system             _rekeying_fs;
	Rekey_progress_file_system    _rekeying_progress_fs;
	Deinitialize_file_system      _deinitialize_fs;
	Create_snapshot_file_system   _create_snapshot_fs;
	Discard_snapshot_file_system  _discard_snapshot_fs;
	Extend_file_system            _extend_fs;
	Extend_progress_file_system   _extend_progress_fs;

	Control_local_factory(Vfs::Env & /* env */,
	                      Xml_node   /* config */,
	                      Wrapper  & wrapper)
	:
		_wrapper(wrapper),
		_rekeying_fs(wrapper),
		_rekeying_progress_fs(wrapper),
		_deinitialize_fs(wrapper),
		_create_snapshot_fs(wrapper),
		_discard_snapshot_fs(wrapper),
		_extend_fs(wrapper),
		_extend_progress_fs(wrapper)
	{ }

	~Control_local_factory()
	{
		_wrapper.dissolve_rekey_file_system(_rekeying_fs);
		_wrapper.dissolve_rekey_progress_file_system(_rekeying_progress_fs);
		_wrapper.dissolve_deinit_file_system(_deinitialize_fs);
		_wrapper.dissolve_extend_file_system(_extend_fs);
		_wrapper.dissolve_extend_progress_file_system(_extend_progress_fs);
	}

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Rekey_file_system::type_name())) {
			return &_rekeying_fs;
		}

		if (node.has_type(Rekey_progress_file_system::type_name())) {
			return &_rekeying_progress_fs;
		}

		if (node.has_type(Deinitialize_file_system::type_name())) {
			return &_deinitialize_fs;
		}

		if (node.has_type(Create_snapshot_file_system::type_name())) {
			return &_create_snapshot_fs;
		}

		if (node.has_type(Discard_snapshot_file_system::type_name())) {
			return &_discard_snapshot_fs;
		}

		if (node.has_type(Extend_file_system::type_name())) {
			return &_extend_fs;
		}

		if (node.has_type(Extend_progress_file_system::type_name())) {
			return &_extend_progress_fs;
		}

		return nullptr;
	}
};


class Vfs_tresor::Control_file_system : private Control_local_factory,
                                        public Vfs::Dir_file_system
{
	private:

		typedef String<256> Config;

		static Config _config(Xml_node /* node */)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				xml.attribute("name", "control");
				xml.node("rekey", [&] () { });
				xml.node("rekey_progress", [&] () { });
				xml.node("extend", [&] () { });
				xml.node("extend_progress", [&] () { });
				xml.node("create_snapshot", [&] () { });
				xml.node("discard_snapshot", [&] () { });
				xml.node("deinitialize", [&] () { });
			});

			return Config(Cstring(buf));
		}

	public:

		Control_file_system(Vfs::Env         &vfs_env,
		                    Genode::Xml_node  node,
		                    Wrapper          &tresor)
		:
			Control_local_factory(vfs_env, node, tresor),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(node).string()),
			                     *this)
		{ }

		static char const *type_name() { return "control"; }

		char const *type() override { return type_name(); }
};


struct Vfs_tresor::Local_factory : File_system_factory
{
	Wrapper               &_wrapper;
	Snapshot_file_system   _current_snapshot_fs;
	Snapshots_file_system  _snapshots_fs;
	Control_file_system    _control_fs;

	Local_factory(Vfs::Env &env, Xml_node config,
	              Wrapper &wrapper)
	:
		_wrapper(wrapper),
		_current_snapshot_fs(env, wrapper, 0, false),
		_snapshots_fs(env, config, wrapper),
		_control_fs(env, config, wrapper)
	{ }

	~Local_factory()
	{
		_wrapper.dissolve_snapshots_file_system(_snapshots_fs);
	}

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		using Name = String<64>;
		if (node.has_type(Snapshot_file_system::type_name())
		    && node.attribute_value("name", Name()) == "current")
			return &_current_snapshot_fs;

		if (node.has_type(Control_file_system::type_name()))
			return &_control_fs;

		if (node.has_type(Snapshots_file_system::type_name()))
			return &_snapshots_fs;

		return nullptr;
	}
};


class Vfs_tresor::File_system : private Local_factory,
                                public Vfs::Dir_file_system
{
	private:

		Wrapper &_wrapper;

		typedef String<256> Config;

		static Config _config(Xml_node node)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				typedef String<64> Name;

				xml.attribute("name",
				              node.attribute_value("name",
				                                   Name("tresor")));

				xml.node("control", [&] () { });

				xml.node("snapshot", [&] () {
					xml.attribute("name", "current");
				});

				xml.node("snapshots", [&] () { });
			});

			return Config(Cstring(buf));
		}

	public:

		File_system(Vfs::Env &vfs_env, Genode::Xml_node node,
		            Wrapper &wrapper)
		:
			Local_factory(vfs_env, node, wrapper),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(node).string()),
			                     *this),
			_wrapper(wrapper)
		{ }

		~File_system()
		{
			// XXX rather then destroying the wrapper here, it should be
			//     done on the out-side where it was allocated in the first
			//     place but the factory interface does not support that yet
			// destroy(vfs_env.alloc().alloc()), &_wrapper);
		}
};


/**************************
 ** VFS plugin interface **
 **************************/

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &vfs_env,
		                         Genode::Xml_node node) override
		{
			try {
				/* XXX wrapper is not managed and will leak */
				Vfs_tresor::Wrapper *wrapper =
					new (vfs_env.alloc()) Vfs_tresor::Wrapper { vfs_env, node };
				return new (vfs_env.alloc())
					Vfs_tresor::File_system(vfs_env, node, *wrapper);
			} catch (...) {
				Genode::error("could not create 'tresor_fs' ");
			}
			return nullptr;
		}
	};

	static Factory factory;
	return &factory;
}


/**********************
 ** Vfs_tresor::Wrapper **
 **********************/

void Vfs_tresor::Wrapper::_snapshots_fs_update_snapshot_registry()
{
	if (_snapshots_fs.valid()) {
		_snapshots_fs.obj().update_snapshot_registry();
	}
}


void Vfs_tresor::Wrapper::_extend_fs_trigger_watch_response()
{
	if (_extend_fs.valid()) {
		_extend_fs.obj().trigger_watch_response();
	}
}


void Vfs_tresor::Wrapper::_extend_progress_fs_trigger_watch_response()
{
	if (_extend_progress_fs.valid()) {
		_extend_progress_fs.obj().trigger_watch_response();
	}
}


void Vfs_tresor::Wrapper::_rekey_fs_trigger_watch_response()
{
	if (_rekey_fs.valid()) {
		_rekey_fs.obj().trigger_watch_response();
	}
}


void Vfs_tresor::Wrapper::_rekey_progress_fs_trigger_watch_response()
{
	if (_rekey_progress_fs.valid()) {
		_rekey_progress_fs.obj().trigger_watch_response();
	}
}


void Vfs_tresor::Wrapper::_deinit_fs_trigger_watch_response()
{
	if (_deinit_fs.valid()) {
		_deinit_fs.obj().trigger_watch_response();
	}
}


/*******************************************************
 ** Vfs_tresor::Snapshots_file_system::Snapshot_registry **
 *******************************************************/

void Vfs_tresor::Snapshots_file_system::Snapshot_registry::update(Vfs::Env &vfs_env)
{
	Tresor::Snapshots_info snap_info { };
	_wrapper.snapshots_info(snap_info);
	bool trigger_watch_response { false };

	/* alloc new */
	for (size_t i = 0; i < MAX_NR_OF_SNAPSHOTS; i++) {

		Generation const snap_gen = snap_info.generations[i];
		if (snap_gen == INVALID_GENERATION)
			continue;

		bool is_old = false;
		auto find_old = [&] (Snapshot_file_system const &fs) {
			is_old |= (fs.snap_gen() == snap_gen);
		};
		_registry.for_each(find_old);

		if (!is_old) {

			new (_alloc)
				Genode::Registered<Snapshot_file_system> {
					_registry, vfs_env, _wrapper, snap_gen, true };

			++_number_of_snapshots;
			trigger_watch_response = true;
		}
	}

	/* destroy old */
	auto find_stale = [&] (Snapshot_file_system const &fs)
	{
		bool is_stale = true;
		for (size_t i = 0; i < MAX_NR_OF_SNAPSHOTS; i++) {
			Generation const snap_gen = snap_info.generations[i];
			if (snap_gen == INVALID_GENERATION)
				continue;

			if (fs.snap_gen() == snap_gen) {
				is_stale = false;
				break;
			}
		}

		if (is_stale) {
			destroy(&_alloc, &const_cast<Snapshot_file_system&>(fs));
			--_number_of_snapshots;
			trigger_watch_response = true;
		}
	};
	_registry.for_each(find_stale);
	if (trigger_watch_response) {
		_snapshots_fs.trigger_watch_response();
	}
}  
