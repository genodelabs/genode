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
#include <tresor/ft_resizing.h>
#include <tresor/meta_tree.h>
#include <tresor/request_pool.h>
#include <tresor/superblock_control.h>
#include <tresor/trust_anchor.h>
#include <tresor/virtual_block_device.h>


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
}


class Vfs_tresor::Wrapper
:
	private Tresor::Module_composition,
	public  Tresor::Module
{
	private:

		Vfs::Env &_vfs_env;

		Constructible<Request_pool>         _request_pool { };
		Constructible<Tresor::Free_tree>       _free_tree    { };
		Constructible<Tresor::Ft_resizing>     _ft_resizing  { };
		Constructible<Virtual_block_device> _vbd          { };
		Constructible<Superblock_control>   _sb_control   { };
		Tresor::Meta_tree                      _meta_tree    { };
		Constructible<Tresor::Trust_anchor>    _trust_anchor { };
		Constructible<Tresor::Crypto>          _crypto       { };
		Constructible<Tresor::Block_io>        _block_io     { };

		Client_data_request _client_data_request { };

	public:

		/********************************
		 ** Module API for Client_data **
		 ********************************/

		bool ready_to_submit_request() override
		{
			return _client_data_request._type == Client_data_request::INVALID;
		}

		void submit_request(Module_request &req) override
		{
			if (_client_data_request._type != Client_data_request::INVALID) {

				class Exception_1 { };
				throw Exception_1 { };
			}
			req.dst_request_id(0);
			_client_data_request = *dynamic_cast<Client_data_request *>(&req);
			switch (_client_data_request._type) {
			case Client_data_request::OBTAIN_PLAINTEXT_BLK:
			{
				void const *src =
					_lookup_write_buffer(_client_data_request._client_req_tag,
					                     _client_data_request._vba);
				if (src == nullptr) {
					_client_data_request._success = false;
					break;
				}

				(void)memcpy((void*)_client_data_request._plaintext_blk_ptr,
				             src, sizeof(Tresor::Block));

				_client_data_request._success = true;
				break;
			}
			case Client_data_request::SUPPLY_PLAINTEXT_BLK:
			{
				void *dst =
					_lookup_read_buffer(_client_data_request._client_req_tag,
					                    _client_data_request._vba);
				if (dst == nullptr) {
					_client_data_request._success = false;
					break;
				}

				(void)memcpy(dst, (void const*)_client_data_request._plaintext_blk_ptr,
				             sizeof(Tresor::Block));

				_client_data_request._success = true;
				break;
			}
			case Client_data_request::INVALID:

				class Exception_2 { };
				throw Exception_2 { };
			}
		}

		void execute(bool &progress) override
		{
			if (_helper_read_request.pending()) {
				if (_request_pool->ready_to_submit_request()) {
					_helper_read_request.tresor_request.gen(
						_frontend_request.tresor_request.gen());
					_request_pool->submit_request(_helper_read_request.tresor_request);
					_helper_read_request.state = Helper_request::State::IN_PROGRESS;
				}
			}

			if (_helper_write_request.pending()) {
				if (_request_pool->ready_to_submit_request()) {
					_helper_write_request.tresor_request.gen(
						_frontend_request.tresor_request.gen());
					_request_pool->submit_request(_helper_write_request.tresor_request);
					_helper_write_request.state = Helper_request::State::IN_PROGRESS;
				}
			}

			if (_frontend_request.pending()) {

				using ST = Frontend_request::State;

				Tresor::Request &request = _frontend_request.tresor_request;

				if (_request_pool->ready_to_submit_request()) {
					_request_pool->submit_request(request);
					_frontend_request.state = ST::IN_PROGRESS;
					progress = true;
				}
			}
		}

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			if (_client_data_request._type != Client_data_request::INVALID) {
				if (sizeof(_client_data_request) > buf_size) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				Genode::memcpy(buf_ptr, &_client_data_request,
				               sizeof(_client_data_request));;
				return true;
			}
			return false;
		}

		void _drop_completed_request(Module_request &) override
		{
			if (_client_data_request._type == Client_data_request::INVALID) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_client_data_request._type = Client_data_request::INVALID;
		}

		struct Rekeying
		{
			enum State { UNKNOWN, IDLE, IN_PROGRESS, };
			enum Result { NONE, SUCCESS, FAILED, };
			State    state;
			Result   last_result;
			uint32_t key_id;

			Virtual_block_address max_vba;
			Virtual_block_address rekeying_vba;
			uint64_t percent_done;

			bool idle()        const { return state == IDLE; }
			bool in_progress() const { return state == IN_PROGRESS; }

			bool success()     const { return last_result == SUCCESS; }

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

		struct Deinitialize
		{
			enum State { IDLE, IN_PROGRESS, };
			enum Result { NONE, SUCCESS, FAILED, };
			State    state;
			Result   last_result;
			uint32_t key_id;

			static char const *state_to_cstring(State const s)
			{
				switch (s) {
				case State::IDLE:        return "idle";
				case State::IN_PROGRESS: return "in-progress";
				}

				return "-";
			}
		};

		struct Extending
		{
			enum Type { INVALID, VBD, FT };
			enum State { UNKNOWN, IDLE, IN_PROGRESS, };
			enum Result { NONE, SUCCESS, FAILED, };
			Type   type;
			State  state;
			Result last_result;

			Virtual_block_address resizing_nr_of_pbas;
			uint64_t percent_done;

			bool idle()        const { return state == IDLE; }
			bool in_progress() const { return state == IN_PROGRESS; }

			bool success()     const { return last_result == SUCCESS; }

			static char const *state_to_cstring(State const s)
			{
				switch (s) {
				case State::UNKNOWN:     return "unknown";
				case State::IDLE:        return "idle";
				case State::IN_PROGRESS: return "in-progress";
				}

				return "-";
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
				case Type::VBD:
					return "vbd";
				case Type::FT:
					return "ft";
				case Type::INVALID:
					return "invalid";
				}

				return nullptr;
			}
		};

	private:

		Rekeying _rekey_obj {
			.state        = Rekeying::State::UNKNOWN,
			.last_result  = Rekeying::Result::NONE,
			.key_id       = 0,
			.max_vba      = 0,
			.rekeying_vba = 0,
			.percent_done = 0, };

		Deinitialize _deinit_obj
		{
			.state       = Deinitialize::State::IDLE,
			.last_result = Deinitialize::Result::NONE,
			.key_id      = 0
		};

		Extending _extend_obj {
			.type                = Extending::Type::INVALID,
			.state               = Extending::State::UNKNOWN,
			.last_result         = Extending::Result::NONE,
			.resizing_nr_of_pbas = 0,
			.percent_done        = 0,
		};

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
			_ft_resizing.construct();
			add_module(FT_RESIZING, *_ft_resizing);

			_free_tree.construct();
			add_module(FREE_TREE, *_free_tree);

			_vbd.construct();
			add_module(VIRTUAL_BLOCK_DEVICE, *_vbd);

			_sb_control.construct();
			add_module(SUPERBLOCK_CONTROL, *_sb_control);

			_request_pool.construct();
			add_module(REQUEST_POOL, *_request_pool);
		}

		/*****************************
		 ** COMMAND_POOL Module API **
		 *****************************/

		bool _peek_generated_request(Genode::uint8_t * /* buf_ptr */,
		                             Genode::size_t    /* buf_size */) override
		{
			return false;
		}

		void _drop_generated_request(Module_request &/* mod_req */) override { }

	public:

		void generated_request_complete(Module_request &mod_req) override
		{
			using ST = Frontend_request::State;

			switch (mod_req.dst_module_id()) {
			case REQUEST_POOL:
			{
				Request const &tresor_request {
					*static_cast<Request *>(&mod_req)};

				if (tresor_request.operation() == Tresor::Request::Operation::REKEY) {
					bool const req_sucess = tresor_request.success();
					if (_verbose) {
						log("Complete request: backend request (", tresor_request, ")");
					}
					_rekey_obj.state = Rekeying::State::IDLE;
					_rekey_obj.last_result = req_sucess ? Rekeying::Result::SUCCESS
					                                    : Rekeying::Result::FAILED;

					_rekey_fs_trigger_watch_response();
					_rekey_progress_fs_trigger_watch_response();
					break;
				}

				if (tresor_request.operation() == Tresor::Request::Operation::DEINITIALIZE) {
					bool const req_sucess = tresor_request.success();
					if (_verbose) {
						log("Complete request: backend request (", tresor_request, ")");
					}
					_deinit_obj.state = Deinitialize::State::IDLE;
					_deinit_obj.last_result = req_sucess ? Deinitialize::Result::SUCCESS
					                                     : Deinitialize::Result::FAILED;

					_deinit_fs_trigger_watch_response();
					break;
				}

				if (tresor_request.operation() == Tresor::Request::Operation::EXTEND_VBD) {
					bool const req_sucess = tresor_request.success();
					if (_verbose) {
						log("Complete request: backend request (", tresor_request, ")");
					}
					_extend_obj.state = Extending::State::IDLE;
					_extend_obj.last_result =
						req_sucess ? Extending::Result::SUCCESS
						           : Extending::Result::FAILED;

					_extend_fs_trigger_watch_response();
					_extend_progress_fs_trigger_watch_response();
					break;
				}

				if (tresor_request.operation() == Tresor::Request::Operation::EXTEND_FT) {
					bool const req_sucess = tresor_request.success();
					if (_verbose) {
						log("Complete request: backend request (", tresor_request, ")");
					}
					_extend_obj.state = Extending::State::IDLE;
					_extend_obj.last_result =
						req_sucess ? Extending::Result::SUCCESS
						           : Extending::Result::FAILED;

					_extend_fs_trigger_watch_response();
					break;
				}

				if (tresor_request.operation() == Tresor::Request::Operation::CREATE_SNAPSHOT) {
					if (_verbose) {
						log("Complete request: (", tresor_request, ")");
					}
					_create_snapshot_request.tresor_request = Tresor::Request();
					_snapshots_fs_update_snapshot_registry();
					break;
				}

				if (tresor_request.operation() == Tresor::Request::Operation::DISCARD_SNAPSHOT) {
					if (_verbose) {
						log("Complete request: (", tresor_request, ")");
					}
					_discard_snapshot_request.tresor_request = Tresor::Request();
					_snapshots_fs_update_snapshot_registry();
					break;
				}

				if (!tresor_request.success()) {
					_helper_read_request.state  = Helper_request::State::NONE;
					_helper_write_request.state = Helper_request::State::NONE;

					bool const eof = tresor_request.block_number() > _sb_control->max_vba();
					_frontend_request.state = eof ? ST::ERROR_EOF : ST::ERROR;
					_frontend_request.tresor_request.success(false);
					if (_verbose) {
						Genode::log("Request failed: ",
						            " (frontend request: ", _frontend_request.tresor_request,
						            " count: ", _frontend_request.count, ")");
					}
					break;
				}

				if (_helper_read_request.in_progress()) {
					_helper_read_request.state = Helper_request::State::COMPLETE;
					_helper_read_request.tresor_request.success(
						tresor_request.success());
				} else if (_helper_write_request.in_progress()) {
					_helper_write_request.state = Helper_request::State::COMPLETE;
					_helper_write_request.tresor_request.success(
						tresor_request.success());
				} else {
					_frontend_request.state = ST::COMPLETE;
					_frontend_request.tresor_request.success(tresor_request.success());
					if (_verbose) {
						Genode::log("Complete request: ",
						            " (frontend request: ", _frontend_request.tresor_request,
						            " count: ", _frontend_request.count, ")");
					}
				}

				if (_helper_read_request.complete()) {
					if (_frontend_request.tresor_request.read()) {
						char       * dst = reinterpret_cast<char*>
							(_frontend_request.tresor_request.offset());
						char const * src = reinterpret_cast<char const*>
							(&_helper_read_request.block_data) + _frontend_request.helper_offset;

						Genode::memcpy(dst, src, _frontend_request.count);

						_helper_read_request.state = Helper_request::State::NONE;
						_frontend_request.state = ST::COMPLETE;
						_frontend_request.tresor_request.success(
							_helper_read_request.tresor_request.success());

						if (_verbose) {
							Genode::log("Complete unaligned READ request: ",
										" (frontend request: ", _frontend_request.tresor_request,
										" (helper request: ", _helper_read_request.tresor_request,
										" offset: ", _frontend_request.helper_offset,
										" count: ", _frontend_request.count, ")");
						}
					}

					if (_frontend_request.tresor_request.write()) {
						/* copy whole block first */
						{
							char       * dst = reinterpret_cast<char*>
								(&_helper_write_request.block_data);
							char const * src = reinterpret_cast<char const*>
								(&_helper_read_request.block_data);
							Genode::memcpy(dst, src, sizeof (Tresor::Block));
						}

						/* and than actual request data */
						{
							char       * dst = reinterpret_cast<char*>
								(&_helper_write_request.block_data) + _frontend_request.helper_offset;
							char const * src = reinterpret_cast<char const*>
								(_frontend_request.tresor_request.offset());
							Genode::memcpy(dst, src, _frontend_request.count);
						}

						/* re-use request */
						_helper_write_request.tresor_request = Tresor::Request(
							Tresor::Request::Operation::WRITE,
							false,
							_helper_read_request.tresor_request.block_number(),
							(uint64_t) &_helper_write_request.block_data,
							_helper_read_request.tresor_request.count(),
							_helper_read_request.tresor_request.key_id(),
							_helper_read_request.tresor_request.tag(),
							_helper_read_request.tresor_request.gen(),
							COMMAND_POOL, 0);

						_helper_write_request.state = Helper_request::State::PENDING;
						_helper_read_request.state  = Helper_request::State::NONE;
					}
				}

				if (_helper_write_request.complete()) {
					if (_verbose) {
						Genode::log("Complete unaligned WRITE request: ",
									" (frontend request: ", _frontend_request.tresor_request,
									" (helper request: ", _helper_read_request.tresor_request,
									" offset: ", _frontend_request.helper_offset,
									" count: ", _frontend_request.count, ")");
					}

					_helper_write_request.state = Helper_request::State::NONE;
					_frontend_request.state = ST::COMPLETE;
				}
				break;
			}
			default:
				class Exception_2 { };
				throw Exception_2 { };
			}
		}

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

		template <typename FN>
		void with_node(char const *name, char const *path, FN const &fn)
		{
			char xml_buffer[128] { };

			Genode::Xml_generator xml {
				xml_buffer, sizeof(xml_buffer), name,
				[&] { xml.attribute("path", path); }
			};

			Genode::Xml_node node { xml_buffer, sizeof(xml_buffer) };
			fn(node);
		}

		Wrapper(Vfs::Env &vfs_env, Xml_node config) : _vfs_env { vfs_env }
		{
			_read_config(config);

			using S = Genode::String<32>;

			S const block_path =
				config.attribute_value("block", S());
			if (block_path.valid())
				with_node("block_io", block_path.string(),
					[&] (Xml_node const &node) {
						_block_io.construct(vfs_env, node);
					});

			S const trust_anchor_path =
				config.attribute_value("trust_anchor", S());
			if (trust_anchor_path.valid())
				with_node("trust_anchor", trust_anchor_path.string(),
					[&] (Xml_node const &node) {
						_trust_anchor.construct(vfs_env, node);
					});

			S const crypto_path =
				config.attribute_value("crypto", S());
			if (crypto_path.valid())
				with_node("crypto", crypto_path.string(),
					[&] (Xml_node const &node) {
						_crypto.construct(vfs_env, node);
					});

			add_module(COMMAND_POOL,  *this);
			add_module(META_TREE,     _meta_tree);
			add_module(CRYPTO,        *_crypto);
			add_module(TRUST_ANCHOR,  *_trust_anchor);
			add_module(CLIENT_DATA,  *this);
			add_module(BLOCK_IO,      *_block_io);

			_initialize_tresor();
		}

		Tresor::Request_pool &tresor()
		{
			if (!_request_pool.constructed()) {
				struct Tresor_Not_Initialized { };
				throw Tresor_Not_Initialized();
			}

			return *_request_pool;
		}

		Genode::uint64_t max_vba()
		{
			return _sb_control->max_vba();
		}

		struct Invalid_Request : Genode::Exception { };

		struct Helper_request
		{
			enum { BLOCK_SIZE = 512, };
			enum State { NONE, PENDING, IN_PROGRESS, COMPLETE, ERROR };

			State state { NONE };

			Tresor::Block   block_data     { };
			Tresor::Request tresor_request { };

			bool pending()     const { return state == PENDING; }
			bool in_progress() const { return state == IN_PROGRESS; }
			bool complete()    const { return state == COMPLETE; }
		};

		Helper_request _helper_read_request  { };
		Helper_request _helper_write_request { };

		struct Frontend_request
		{
			enum State {
				NONE,
				PENDING, IN_PROGRESS, COMPLETE,
				ERROR, ERROR_EOF
			};
			State            state          { NONE };
			size_t           count          { 0 };
			Tresor::Request  tresor_request { };
			void            *data           { nullptr };
			uint64_t         offset         { 0 };
			uint64_t         helper_offset  { 0 };

			bool pending()     const { return state == PENDING; }
			bool in_progress() const { return state == IN_PROGRESS; }
			bool complete()    const { return state == COMPLETE; }

			static char const *state_to_string(State s)
			{
				switch (s) {
				case State::NONE:         return "NONE";
				case State::PENDING:      return "PENDING";
				case State::IN_PROGRESS:  return "IN_PROGRESS";
				case State::COMPLETE:     return "COMPLETE";
				case State::ERROR:        return "ERROR";
				case State::ERROR_EOF:    return "ERROR_EOF";
				}
				return "<unknown>";
			}
		};

		uint64_t _next_client_request_tag()
		{
			static uint64_t _client_request_tag { 0 };
			return _client_request_tag++;
		}

		void const *_lookup_write_buffer(Genode::uint64_t /* tag */, Genode::uint64_t /* vba */)
		{
			if (_helper_write_request.in_progress())
				return (void const*)&_helper_write_request.block_data;
			if (_frontend_request.in_progress())
				return (void const*)_frontend_request.data;

			return nullptr;
		}

		void *_lookup_read_buffer(Genode::uint64_t /* tag */, Genode::uint64_t /* vba */)
		{
			if (_helper_read_request.in_progress())
				return (void *)&_helper_read_request.block_data;
			if (_frontend_request.in_progress())
				return (void *)_frontend_request.data;

			return nullptr;
		}

		Frontend_request _frontend_request { };

		Frontend_request const & frontend_request() const
		{
			return _frontend_request;
		}

		void ack_frontend_request(Vfs_handle &/* handle */)
		{
			// assert current state was *_COMPLETE
			_frontend_request.state = Frontend_request::State::NONE;
			_frontend_request.tresor_request = Tresor::Request { };
		}

		bool submit_frontend_request(Vfs_handle &handle,
		                             Byte_range_ptr const &data,
		                             Tresor::Request::Operation op,
		                             Generation gen)
		{
			if (_frontend_request.state != Frontend_request::State::NONE) {
				return false;
			}

			uint64_t const tag = _next_client_request_tag();

			/* short-cut for SYNC requests */
			if (op == Tresor::Request::Operation::SYNC) {
				_frontend_request.tresor_request = Tresor::Request(
					op,
					false,
					0,
					0,
					1,
					0,
					(Genode::uint32_t)tag,
					0,
					COMMAND_POOL, 0);
				_frontend_request.count   = 0;
				_frontend_request.state   = Frontend_request::State::PENDING;
				if (_verbose) {
					Genode::log("Req: (front req: ",
					            _frontend_request.tresor_request, ")");
				}
				return true;
			}

			file_size const offset = handle.seek();
			bool unaligned_request = false;

			/* unaligned request if any condition is true */
			unaligned_request |= (offset % Tresor::BLOCK_SIZE) != 0;
			unaligned_request |= (data.num_bytes < Tresor::BLOCK_SIZE);

			size_t count = data.num_bytes;

			if ((count % Tresor::BLOCK_SIZE) != 0 &&
			    !unaligned_request)
			{
				count = count - (count % Tresor::BLOCK_SIZE);
			}

			if (unaligned_request) {
				_helper_read_request.tresor_request = Tresor::Request(
					Tresor::Request::Operation::READ,
					false,
					offset / Tresor::BLOCK_SIZE,
					(uint64_t)&_helper_read_request.block_data,
					1,
					0,
					(Genode::uint32_t)tag,
					0,
					COMMAND_POOL, 0);
				_helper_read_request.state = Helper_request::State::PENDING;

				_frontend_request.helper_offset = (offset % Tresor::BLOCK_SIZE);
				if (count >= (Tresor::BLOCK_SIZE - _frontend_request.helper_offset)) {

					uint64_t const count_u64 {
						Tresor::BLOCK_SIZE - _frontend_request.helper_offset };

					if (count_u64 > ~(size_t)0) {
						class Exception_3 { };
						throw Exception_3 { };
					}
					_frontend_request.count = (size_t)count_u64;
				} else {
					_frontend_request.count = count;
				}

				/* skip handling by Tresor library, helper requests will do that for us */
				_frontend_request.state = Frontend_request::State::IN_PROGRESS;

			} else {
				_frontend_request.count = count;
				_frontend_request.state = Frontend_request::State::PENDING;
			}

			_frontend_request.data   = data.start;
			_frontend_request.offset = offset;
			_frontend_request.tresor_request = Tresor::Request(
				op, false, offset / Tresor::BLOCK_SIZE, (uint64_t)data.start,
				(uint32_t)(count / Tresor::BLOCK_SIZE), 0,
				(Genode::uint32_t)tag, gen, COMMAND_POOL, 0);

			if (_verbose) {
				if (unaligned_request) {
					Genode::log("Unaligned req: ",
					            "off: ", offset, " bytes: ", count,
					            " (front req: ", _frontend_request.tresor_request,
					            " (helper req: ", _helper_read_request.tresor_request,
					            " off: ", _frontend_request.helper_offset,
					            " count: ", _frontend_request.count, ")");
				} else {
					Genode::log("Req: ",
					            "off: ", offset, " bytes: ", count,
					            " (front req: ", _frontend_request.tresor_request, ")");
				}
			}

			return true;
		}

		void _snapshots_fs_update_snapshot_registry();

		void _extend_fs_trigger_watch_response();

		void _extend_progress_fs_trigger_watch_response();

		void _rekey_fs_trigger_watch_response();

		void _rekey_progress_fs_trigger_watch_response();

		void _deinit_fs_trigger_watch_response();

		void handle_frontend_request()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				execute_modules(progress);
			}
			_vfs_env.io().commit();

			Tresor::Superblock_info const sb_info {
				_sb_control->sb_info() };

			using ES = Extending::State;
			if (_extend_obj.state == ES::UNKNOWN && sb_info.valid) {
				if (sb_info.extending_ft) {

					_extend_obj.state = ES::IN_PROGRESS;
					_extend_obj.type  = Extending::Type::FT;
					_extend_fs_trigger_watch_response();

				} else

				if (sb_info.extending_vbd) {

					_extend_obj.state = ES::IN_PROGRESS;
					_extend_obj.type  = Extending::Type::VBD;
					_extend_fs_trigger_watch_response();

				} else {

					_extend_obj.state = ES::IDLE;
					_extend_fs_trigger_watch_response();
				}
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

		bool client_request_acceptable()
		{
			return _request_pool->ready_to_submit_request();
		}

		bool start_rekeying()
		{
			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			Tresor::Request req(
				Tresor::Request::Operation::REKEY,
				false,
				0, 0, 0,
				_rekey_obj.key_id,
				0, 0,
				COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (background req: ", req, ")");
			}

			_request_pool->submit_request(req);
			_rekey_obj.state        = Rekeying::State::IN_PROGRESS;
			_rekey_obj.last_result  = Rekeying::Rekeying::FAILED;
			_rekey_obj.max_vba      = _sb_control->max_vba();
			_rekey_obj.rekeying_vba = _sb_control->rekeying_vba();
			_rekey_fs_trigger_watch_response();
			_rekey_progress_fs_trigger_watch_response();

			// XXX kick-off rekeying
			handle_frontend_request();
			return true;
		}

		Rekeying const rekeying_progress() const
		{
			return _rekey_obj;
		}

		bool start_deinitialize()
		{
			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			Tresor::Request req(
				Tresor::Request::Operation::DEINITIALIZE,
				false,
				0, 0, 0,
				0,
				0, 0,
				COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (background req: ", req, ")");
			}

			_request_pool->submit_request(req);
			_deinit_obj.state       = Deinitialize::State::IN_PROGRESS;
			_deinit_obj.last_result = Deinitialize::Deinitialize::FAILED;
			_deinit_fs_trigger_watch_response();

			// XXX kick-off deinitialize
			handle_frontend_request();
			return true;
		}

		Deinitialize const deinitialize_progress() const
		{
			return _deinit_obj;
		}


		bool start_extending(Extending::Type       type,
		                     Tresor::Number_of_blocks blocks)
		{
			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			Tresor::Request::Operation op =
				Tresor::Request::Operation::INVALID;

			switch (type) {
			case Extending::Type::VBD:
				op = Tresor::Request::Operation::EXTEND_VBD;
				break;
			case Extending::Type::FT:
				op = Tresor::Request::Operation::EXTEND_FT;
				break;
			case Extending::Type::INVALID:
				return false;
			}

			Tresor::Request req(op, false,
			                 0, 0, blocks, 0, 0, 0,
			                 COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (background req: ", req, ")");
			}

			_request_pool->submit_request(req);
			_extend_obj.type                = type;
			_extend_obj.state               = Extending::State::IN_PROGRESS;
			_extend_obj.last_result         = Extending::Result::NONE;
			_extend_obj.resizing_nr_of_pbas = 0;
			_extend_fs_trigger_watch_response();
			_extend_progress_fs_trigger_watch_response();

			// XXX kick-off extending
			handle_frontend_request();
			return true;
		}

		Extending const extending_progress() const
		{
			return _extend_obj;
		}

		void snapshot_generations(Tresor::Snapshot_generations &generations)
		{
			if (!_request_pool.constructed()) {
				_initialize_tresor();
			}
			_sb_control->snapshot_generations(generations);
			handle_frontend_request();
		}


		Frontend_request _create_snapshot_request { };

		bool create_snapshot()
		{
			if (!_request_pool.constructed()) {
				_initialize_tresor();
			}

			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			if (_create_snapshot_request.tresor_request.valid()) {
				return false;
			}

			Tresor::Request::Operation const op =
				Tresor::Request::Operation::CREATE_SNAPSHOT;

			_create_snapshot_request.tresor_request =
				Tresor::Request(op, false, 0, 0, 1, 0, 0, 0,
				             COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (req: ", _create_snapshot_request.tresor_request, ")");
			}

			_request_pool->submit_request(_create_snapshot_request.tresor_request);

			_create_snapshot_request.state =
				Frontend_request::State::IN_PROGRESS;

			// XXX kick-off snapshot creation request
			handle_frontend_request();
			return true;
		}

		Frontend_request _discard_snapshot_request { };

		bool discard_snapshot(Generation snap_gen)
		{
			if (!_request_pool.constructed()) {
				_initialize_tresor();
			}

			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			if (_discard_snapshot_request.tresor_request.valid()) {
				return false;
			}

			Tresor::Request::Operation const op =
				Tresor::Request::Operation::DISCARD_SNAPSHOT;

			_discard_snapshot_request.tresor_request =
				Tresor::Request(op, false, 0, 0, 1, 0, 0, snap_gen, COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (req: ", _discard_snapshot_request.tresor_request, ")");
			}

			_request_pool->submit_request(_discard_snapshot_request.tresor_request);

			_discard_snapshot_request.state =
				Frontend_request::State::IN_PROGRESS;

			// XXX kick-off snapshot creation request
			handle_frontend_request();
			return true;
		}

		Genode::Mutex _frontend_mtx { };

		Genode::Mutex &frontend_mtx() { return _frontend_mtx; }
};


class Vfs_tresor::Data_file_system : public Single_file_system
{
	private:

		Wrapper &_w;
		Generation _snap_gen;

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
				Genode::Mutex::Guard guard { _w.frontend_mtx() };

				using State = Wrapper::Frontend_request::State;

				State state = _w.frontend_request().state;
				if (state == State::NONE) {

					if (!_w.client_request_acceptable()) {
						return READ_QUEUED;
					}
					using Op = Tresor::Request::Operation;

					bool const accepted =
						_w.submit_frontend_request(*this, dst, Op::READ, _snap_gen);
					if (!accepted) { return READ_ERR_IO; }
				}

				_w.handle_frontend_request();
				state = _w.frontend_request().state;

				if (   state == State::PENDING
				    || state == State::IN_PROGRESS) {
					return READ_QUEUED;
				}

				if (state == State::COMPLETE) {
					out_count = _w.frontend_request().count;
					_w.ack_frontend_request(*this);
					return READ_OK;
				}

				if (state == State::ERROR_EOF) {
					out_count = 0;
					_w.ack_frontend_request(*this);
					return READ_OK;
				}

				if (state == State::ERROR) {
					out_count = 0;
					_w.ack_frontend_request(*this);
					return READ_ERR_IO;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src,
			                   size_t &out_count) override
			{
				Genode::Mutex::Guard guard { _w.frontend_mtx() };

				using State = Wrapper::Frontend_request::State;

				State state = _w.frontend_request().state;
				if (state == State::NONE) {

					if (!_w.client_request_acceptable())
						return Write_result::WRITE_ERR_WOULD_BLOCK;

					using Op = Tresor::Request::Operation;

					bool const accepted =
						_w.submit_frontend_request(
							*this, Byte_range_ptr(const_cast<char*>(src.start), src.num_bytes),
							Op::WRITE, _snap_gen);
					if (!accepted) { return WRITE_ERR_IO; }
				}

				_w.handle_frontend_request();
				state = _w.frontend_request().state;

				if (   state == State::PENDING
				    || state == State::IN_PROGRESS) {
					return WRITE_ERR_WOULD_BLOCK;
				}

				if (state == State::COMPLETE) {
					out_count = _w.frontend_request().count;
					_w.ack_frontend_request(*this);
					return WRITE_OK;
				}

				if (state == State::ERROR_EOF) {
					out_count = 0;
					_w.ack_frontend_request(*this);
					return WRITE_OK;
				}

				if (state == State::ERROR) {
					out_count = 0;
					_w.ack_frontend_request(*this);
					return WRITE_ERR_IO;
				}

				return WRITE_ERR_IO;
			}

			Sync_result sync() override
			{
				Genode::Mutex::Guard guard { _w.frontend_mtx() };

				using State = Wrapper::Frontend_request::State;

				State state = _w.frontend_request().state;
				if (state == State::NONE) {

					if (!_w.client_request_acceptable()) {
						return SYNC_QUEUED;
					}
					using Op = Tresor::Request::Operation;

					bool const accepted =
						_w.submit_frontend_request(*this, Byte_range_ptr(nullptr, 0),
						                           Op::SYNC, 0);
					if (!accepted) { return SYNC_ERR_INVALID; }
				}

				_w.handle_frontend_request();
				state = _w.frontend_request().state;

				if (   state == State::PENDING
				    || state == State::IN_PROGRESS) {
					return SYNC_QUEUED;
				}

				if (state == State::COMPLETE) {
					_w.ack_frontend_request(*this);
					return SYNC_OK;
				}

				if (state == State::ERROR) {
					_w.ack_frontend_request(*this);
					return SYNC_ERR_INVALID;
				}

				return SYNC_ERR_INVALID;
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

		~Data_file_system() { /* XXX sync on close */ }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			try {
				(void)_w.tresor();
			} catch (...) {
				return STAT_ERR_NO_ENTRY;
			}

			Stat_result result = Single_file_system::stat(path, out);

			/* max_vba range is from 0 ... N - 1 */
			out.size = (_w.max_vba() + 1) * Tresor::BLOCK_SIZE;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}


		/***************************
		 ** File-system interface **
		 ***************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				(void)_w.tresor();
			} catch (...) {
				return OPEN_ERR_UNACCESSIBLE;
			}

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
				_w.handle_frontend_request();

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
				using State = Wrapper::Extending::State;
				if (_w.extending_progress().state != State::IDLE) {
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}
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
				_w.handle_frontend_request();

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

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}
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
				_w.handle_frontend_request();

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
				using State = Wrapper::Rekeying::State;
				if (_w.rekeying_progress().state != State::IDLE) {
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}
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
				_w.handle_frontend_request();

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

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}
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

			bool const in_progress {
				deinitialize_progress.state ==
					Wrapper::Deinitialize::State::IN_PROGRESS };

			bool const last_result {
				!in_progress &&
				deinitialize_progress.last_result !=
					Wrapper::Deinitialize::Result::NONE };

			bool const success {
				deinitialize_progress.last_result ==
					Wrapper::Deinitialize::Result::SUCCESS };

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
				_w.handle_frontend_request();

				Wrapper::Deinitialize const & deinitialize_progress {
					_w.deinitialize_progress() };

				bool const in_progress {
					deinitialize_progress.state ==
						Wrapper::Deinitialize::State::IN_PROGRESS };

				if (in_progress)
					return READ_QUEUED;

				Content_string const result { content_string(_w) };
				copy_cstring(dst.start, result.string(), dst.num_bytes);
				out_count = dst.num_bytes;

				return READ_OK;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				using State = Wrapper::Deinitialize::State;
				if (_w.deinitialize_progress().state != State::IDLE) {
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}
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

		bool read_ready(Vfs::Vfs_handle const &) const override
		{
			return true;
		}

		bool write_ready(Vfs::Vfs_handle const &) const override
		{
			return false;
		}

		Ftruncate_result ftruncate(Vfs::Vfs_handle * /* handle */, file_size) override
		{
			return FTRUNCATE_OK;
		}
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
	Tresor::Snapshot_generations generations { };
	_wrapper.snapshot_generations(generations);
	bool trigger_watch_response { false };

	/* alloc new */
	for (size_t i = 0; i < MAX_NR_OF_SNAPSHOTS; i++) {

		Generation const snap_gen = generations.items[i];
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
			Generation const snap_gen = generations.items[i];
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
