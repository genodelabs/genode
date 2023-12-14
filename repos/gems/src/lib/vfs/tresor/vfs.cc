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
#include <tresor/client_data_interface.h>
#include <tresor/crypto.h>
#include <tresor/free_tree.h>
#include <tresor/meta_tree.h>
#include <tresor/superblock_control.h>
#include <tresor/trust_anchor.h>
#include <tresor/virtual_block_device.h>

/* vfs tresor includes */
#include <splitter.h>

using namespace Genode;
using namespace Vfs;
using namespace Tresor;

namespace Vfs_tresor {

	class Request_interface;
	class Data_operation;
	class Data_file_system;
	class Extend_operation;
	class Extend_file_system;
	class Rekey_operation;
	class Rekey_file_system;
	class Deinitialize_operation;
	class Deinitialize_file_system;
	class Control_local_factory;
	class Control_file_system;
	class Current_local_factory;
	class Current_file_system;
	class Local_factory;
	class File_system;
	class Plugin;
}

class Vfs_tresor::Data_operation : private Noncopyable
{
	public:

		enum Result { PENDING, SUCCEEDED, FAILED };

		struct Execute_attr
		{
			Splitter &splitter;
			Superblock_control &sb_control;
			Client_data_interface &client_data;
			Virtual_block_device &vbd;
			Free_tree &free_tree;
			Meta_tree &meta_tree;
			Block_io &block_io;
			Crypto &crypto;
			Trust_anchor &trust_anchor;
		};

	private:

		enum State {
			INIT, READ_REQUESTED, READ_STARTED, READ, READ_COMPLETE, WRITE_REQUESTED, WRITE_STARTED, WRITE,
			WRITE_COMPLETE, SYNC_REQUESTED, SYNC_STARTED, SYNC, SYNC_COMPLETE };

		State _state { INIT };
		bool const _verbose;
		Generation _generation { };
		addr_t _seek { };
		bool _success { };
		Constructible<Byte_range_ptr> _dst { };
		Constructible<Const_byte_range_ptr> _src { };
		Constructible<Splitter::Write> _write { };
		Constructible<Splitter::Read> _read { };
		Constructible<Superblock_control::Synchronize> _sync { };

		bool _range_violation(Superblock_control &sb_control, addr_t start, size_t num_bytes) const
		{
			addr_t last_byte = num_bytes ? start - 1 + num_bytes : start;
			addr_t last_file_byte = (sb_control.max_vba() * BLOCK_SIZE) + (BLOCK_SIZE - 1);
			return last_byte > last_file_byte;
		}

	public:

		Data_operation(bool verbose) : _verbose(verbose) { }

		Result write(addr_t seek, Const_byte_range_ptr const &src)
		{
			switch (_state) {
			case INIT:

				_seek = seek;
				_src.construct(src.start, src.num_bytes);
				_state = WRITE_REQUESTED;
				if (_verbose)
					log("write (seek ", _seek, " num_bytes ", _src->num_bytes, ") requested");
				return PENDING;

			case WRITE_REQUESTED:
			case WRITE_STARTED:
			case WRITE: return PENDING;
			case WRITE_COMPLETE:

				_src.destruct();
				_state = INIT;
				return _success ? SUCCEEDED : FAILED;

			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		Result read(addr_t seek, Byte_range_ptr const &dst)
		{
			switch (_state) {
			case INIT:

				_seek = seek;
				_dst.construct(dst.start, dst.num_bytes);
				_state = READ_REQUESTED;
				if (_verbose)
					log("read (seek ", _seek, " num_bytes ", _dst->num_bytes, ") requested");
				return PENDING;

			case READ_REQUESTED:
			case READ_STARTED:
			case READ: return PENDING;
			case READ_COMPLETE:

				_dst.destruct();
				_state = INIT;
				return _success ? SUCCEEDED : FAILED;

			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		Result sync()
		{
			switch (_state) {
			case INIT:

				_state = SYNC_REQUESTED;
				if (_verbose)
					log("sync requested");
				return PENDING;

			case SYNC_REQUESTED:
			case SYNC_STARTED:
			case SYNC: return PENDING;
			case SYNC_COMPLETE:

				_state = INIT;
				return _success ? SUCCEEDED : FAILED;

			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		bool complete() const { return _state == WRITE_COMPLETE || _state == READ_COMPLETE || _state == SYNC_COMPLETE; }

		bool requested() const { return _state == WRITE_REQUESTED || _state == READ_REQUESTED || _state == SYNC_REQUESTED; }

		void start()
		{
			switch (_state) {
			case WRITE_REQUESTED: _state = WRITE_STARTED; break;
			case READ_REQUESTED: _state = READ_STARTED; break;
			case SYNC_REQUESTED: _state = SYNC_STARTED; break;
			default: ASSERT_NEVER_REACHED;
			}
		}

		bool execute(Execute_attr const &attr)
		{
			bool progress = false;
			switch (_state) {
			case WRITE_STARTED:

				if (_range_violation(attr.sb_control, _seek, _src->num_bytes)) {
					_success = false;
					_state = WRITE_COMPLETE;
					progress = true;
					if (_verbose)
						log("write (seek ", _seek, " num_bytes ", _src->num_bytes, ") failed: range violation");
					break;
				}
				_write.construct(Splitter::Write::Attr{_seek, _generation, _src->start, _src->num_bytes});
				_state = WRITE;
				progress = true;
				if (_verbose)
					log("write (seek ", _seek, " num_bytes ", _src->num_bytes, ") started");
				break;

			case WRITE:

				progress |= attr.splitter.execute(
					*_write, {attr.sb_control, attr.vbd, attr.client_data, attr.block_io,
					          attr.free_tree, attr.meta_tree, attr.crypto});

				if (_write->complete()) {
					_success = _write->success();
					_write.destruct();
					_state = WRITE_COMPLETE;
					progress = true;
					if (_verbose)
						log("write (seek ", _seek, " num_bytes ", _src->num_bytes, ") ", _success ? "succeeded" : "failed");
				}
				break;

			case READ_STARTED:

				if (_range_violation(attr.sb_control, _seek, _dst->num_bytes)) {
					_success = false;
					_state = READ_COMPLETE;
					progress = true;
					if (_verbose)
						log("read (seek ", _seek, " num_bytes ", _dst->num_bytes, ") failed: range violation");
					break;
				}
				_read.construct(Splitter::Read::Attr{_seek, _generation, _dst->start, _dst->num_bytes});
				_state = READ;
				progress = true;
				if (_verbose)
					log("read (seek ", _seek, " num_bytes ", _dst->num_bytes, ") started");
				break;

			case READ:

				progress |= attr.splitter.execute(
					*_read, {attr.sb_control, attr.vbd, attr.client_data, attr.block_io, attr.crypto});

				if (_read->complete()) {
					_success = _read->success();
					_read.destruct();
					_state = READ_COMPLETE;
					progress = true;
					if (_verbose)
						log("read (seek ", _seek, " num_bytes ", _dst->num_bytes, ") ", _success ? "succeeded" : "failed");
				}
				break;

			case SYNC_STARTED:

				_sync.construct(Superblock_control::Synchronize::Attr{});
				_state = SYNC;
				progress = true;
				if (_verbose)
					log("sync started");
				break;

			case SYNC:

				progress |= attr.sb_control.execute(*_sync, attr.block_io, attr.trust_anchor);
				if (_sync->complete()) {
					_success = _sync->success();
					_sync.destruct();
					_state = SYNC_COMPLETE;
					progress = true;
					if (_verbose)
						log("sync ", _success ? "succeeded" : "failed");
				}
				break;

			default: break;
			}
			return progress;
		}
};

class Vfs_tresor::Rekey_operation : private Noncopyable
{
	public:

		enum Result { NONE, SUCCEEDED, FAILED, PENDING };

		struct Execute_attr
		{
			Rekey_file_system *rekey_fs_ptr;
			Superblock_control &sb_control;
			Virtual_block_device &vbd;
			Free_tree &free_tree;
			Meta_tree &meta_tree;
			Block_io &block_io;
			Crypto &crypto;
			Trust_anchor &trust_anchor;
		};

	private:

		enum State { INIT, REQUESTED, STARTED, REKEY, PAUSED, RESUMED, COMPLETE };

		State _state { INIT };
		bool const _verbose;
		bool _success { };
		bool _complete { };
		Constructible<Superblock_control::Rekey> _rekey { };

	public:

		Rekey_operation(bool verbose) : _verbose(verbose) { }

		bool request()
		{
			switch (_state) {
			case INIT:
			case COMPLETE:

				_state = REQUESTED;
				if (_verbose)
					log("rekey requested");
				return true;

			default: return false;
			}
			ASSERT_NEVER_REACHED;
		}

		Result result() const
		{
			switch (_state) {
			case INIT: return NONE;
			case COMPLETE: return _success ? SUCCEEDED : FAILED;
			default: return PENDING;
			}
			ASSERT_NEVER_REACHED;
		}

		bool execute(Execute_attr const &attr);

		void resume()
		{
			switch (_state) {
			case PAUSED: _state = RESUMED; break;
			default: ASSERT_NEVER_REACHED;
			}
		}

		void start()
		{
			switch (_state) {
			case REQUESTED: _state = STARTED; break;
			default: ASSERT_NEVER_REACHED;
			}
		}

		bool complete() const { return _state == COMPLETE; }

		bool paused() const { return _state == PAUSED; }

		bool requested() const { return _state == REQUESTED; }
};

class Vfs_tresor::Deinitialize_operation : private Noncopyable
{
	public:

		enum Result { NONE, SUCCEEDED, FAILED, PENDING };

		struct Execute_attr
		{
			Deinitialize_file_system *deinit_fs_ptr;
			Superblock_control &sb_control;
			Block_io &block_io;
			Crypto &crypto;
			Trust_anchor &trust_anchor;
		};

	private:

		enum State { INIT, REQUESTED, STARTED, DEINIT_SB_CONTROL, COMPLETE };

		State _state { INIT };
		bool const _verbose;
		bool _success { };
		Constructible<Superblock_control::Deinitialize> _deinit_sb_control { };

	public:

		Deinitialize_operation(bool verbose) : _verbose(verbose) { }

		bool request()
		{
			switch (_state) {
			case INIT:
			case COMPLETE:

				_state = REQUESTED;
				if (_verbose)
					log("deinitialize requested");
				return true;

			default: return false;
			}
			ASSERT_NEVER_REACHED;
		}

		void start()
		{
			switch (_state) {
			case REQUESTED: _state = STARTED; break;
			default: ASSERT_NEVER_REACHED;
			}
		}

		Result result() const
		{
			switch (_state) {
			case INIT: return NONE;
			case COMPLETE: return _success ? SUCCEEDED : FAILED;
			default: return PENDING;
			}
			ASSERT_NEVER_REACHED;
		}

		bool complete() const { return _state == COMPLETE; }

		bool requested() const { return _state == REQUESTED; }

		bool execute(Execute_attr const &attr);
};

class Vfs_tresor::Extend_operation : private Noncopyable
{
	public:

		enum Result { NONE, SUCCEEDED, FAILED, PENDING };

		struct Execute_attr
		{
			Extend_file_system *extend_fs_ptr;
			Superblock_control &sb_control;
			Virtual_block_device &vbd;
			Free_tree &free_tree;
			Meta_tree &meta_tree;
			Block_io &block_io;
			Trust_anchor &trust_anchor;
		};

	private:

		enum State {
			INIT, EXTEND_FT_REQUESTED, EXTEND_FT_STARTED, EXTEND_FT, EXTEND_FT_PAUSED, EXTEND_FT_RESUMED,
			EXTEND_VBD_REQUESTED, EXTEND_VBD_STARTED, EXTEND_VBD, EXTEND_VBD_PAUSED, EXTEND_VBD_RESUMED, COMPLETE };

		State _state { INIT };
		bool const _verbose;
		bool _success { };
		bool _complete { };
		Number_of_blocks _num_blocks { };
		Constructible<Superblock_control::Extend_free_tree> _extend_ft { };
		Constructible<Superblock_control::Extend_vbd> _extend_vbd { };

	public:

		Extend_operation(bool verbose) : _verbose(verbose) { }

		bool request_for_free_tree(Number_of_blocks num_blocks)
		{
			switch (_state) {
			case INIT:
			case COMPLETE:

				_num_blocks = num_blocks;
				_state = EXTEND_FT_REQUESTED;
				if (_verbose)
					log("extend free tree requested");
				return true;

			default: break;
			}
			return false;
		}

		bool request_for_vbd(Number_of_blocks num_blocks)
		{
			switch (_state) {
			case INIT:
			case COMPLETE:

				_num_blocks = num_blocks;
				_state = EXTEND_VBD_REQUESTED;
				if (_verbose)
					log("extend virtual block device requested");
				return true;

			default: return false;
			}
			ASSERT_NEVER_REACHED;
		}

		Result result() const
		{
			switch (_state) {
			case INIT: return NONE;
			case COMPLETE: return _success ? SUCCEEDED : FAILED;
			default: return PENDING;
			}
			ASSERT_NEVER_REACHED;
		}

		bool execute(Execute_attr const &attr);

		void resume()
		{
			switch (_state) {
			case EXTEND_FT_PAUSED: _state = EXTEND_FT_RESUMED; break;
			case EXTEND_VBD_PAUSED: _state = EXTEND_VBD_RESUMED; break;
			default: ASSERT_NEVER_REACHED;
			}
		}

		void start()
		{
			switch (_state) {
			case EXTEND_FT_REQUESTED: _state = EXTEND_FT_STARTED; break;
			case EXTEND_VBD_REQUESTED: _state = EXTEND_VBD_STARTED; break;
			default: ASSERT_NEVER_REACHED;
			}
		}

		bool complete() const { return _state == COMPLETE; }

		bool paused() const { return _state == EXTEND_FT_PAUSED || _state == EXTEND_VBD_PAUSED; }

		bool requested() const { return _state == EXTEND_FT_REQUESTED || _state == EXTEND_VBD_REQUESTED; }
};

class Vfs_tresor::Plugin : private Noncopyable, private Client_data_interface, private Crypto_key_files_interface
{
	private:

		enum State {
			INIT_SB_CONTROL, NO_OPERATION, DATA_OPERATION, REKEY_OPERATION, EXTEND_OPERATION,
			DEINITIALIZE_OPERATION, DEINITIALIZED };

		enum { MAX_NUM_COMMANDS = 16 };

		struct Crypto_key
		{
			Key_id const key_id;
			Vfs_handle &encrypt_file;
			Vfs_handle &decrypt_file;
		};

		Vfs::Env &_vfs_env;
		bool const _verbose;
		Tresor::Path const _crypto_path;
		Tresor::Path const _block_io_path;
		Tresor::Path const _trust_anchor_path;
		Vfs_handle &_block_io_file { open_file(_vfs_env, _block_io_path, Directory_service::OPEN_MODE_RDWR) };
		Vfs_handle &_crypto_add_key_file { open_file(_vfs_env, { _crypto_path, "/add_key" }, Directory_service::OPEN_MODE_WRONLY) };
		Vfs_handle &_crypto_remove_key_file { open_file(_vfs_env, { _crypto_path, "/remove_key" }, Directory_service::OPEN_MODE_WRONLY) };
		Vfs_handle &_ta_decrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/decrypt" }, Directory_service::OPEN_MODE_RDWR) };
		Vfs_handle &_ta_encrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/encrypt" }, Directory_service::OPEN_MODE_RDWR) };
		Vfs_handle &_ta_generate_key_file { open_file(_vfs_env, { _trust_anchor_path, "/generate_key" }, Directory_service::OPEN_MODE_RDWR) };
		Vfs_handle &_ta_initialize_file { open_file(_vfs_env, { _trust_anchor_path, "/initialize" }, Directory_service::OPEN_MODE_RDWR) };
		Vfs_handle &_ta_hash_file { open_file(_vfs_env, { _trust_anchor_path, "/hash" }, Directory_service::OPEN_MODE_RDWR) };
		Tresor::Free_tree _free_tree { };
		Tresor::Virtual_block_device _vbd { };
		Superblock_control _sb_control { };
		Meta_tree _meta_tree { };
		Trust_anchor _trust_anchor { { _ta_decrypt_file, _ta_encrypt_file, _ta_generate_key_file, _ta_initialize_file, _ta_hash_file } };
		Crypto _crypto { {*this, _crypto_add_key_file, _crypto_remove_key_file} };
		Block_io _block_io { _block_io_file };
		Splitter _splitter { };
		Extend_file_system * _extend_fs_ptr  { };
		Rekey_file_system * _rekey_fs_ptr  { };
		Deinitialize_file_system *_deinit_fs_ptr  { };
		Constructible<Crypto_key> _crypto_keys[2] { };
		Superblock_control::Initialize *_init_sb_control_ptr { };
		Superblock::State _sb_state { Superblock::INVALID };
		Data_operation _data_operation { _verbose };
		Rekey_operation _rekey_operation { _verbose };
		Extend_operation _extend_operation { _verbose };
		Deinitialize_operation _deinit_operation { _verbose };
		State _state { INIT_SB_CONTROL };

		/*
		 * Noncopyable
		 */
		Plugin(Plugin const &) = delete;
		Plugin &operator = (Plugin const &) = delete;

		Constructible<Crypto_key> &_crypto_key(Key_id key_id)
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (key.constructed() && key->key_id == key_id)
					return key;
			ASSERT_NEVER_REACHED;
		}

		bool _try_start_operation()
		{
			if (_deinit_operation.requested()) {
				_deinit_operation.start();
				_state = DEINITIALIZE_OPERATION;
				return true;
			}
			if (_data_operation.requested()) {
				_data_operation.start();
				_state = DATA_OPERATION;
				return true;
			}
			if (_extend_operation.requested()) {
				_extend_operation.start();
				_state = EXTEND_OPERATION;
				return true;
			}
			if (_rekey_operation.requested()) {
				_rekey_operation.start();
				_state = REKEY_OPERATION;
				return true;
			}
			return false;
		}

		bool _try_resume_operation()
		{
			if (_extend_operation.paused()) {
				_extend_operation.resume();
				_state = EXTEND_OPERATION;
				return true;
			}
			if (_rekey_operation.paused()) {
				_rekey_operation.resume();
				_state = REKEY_OPERATION;
				return true;
			}
			return false;
		}

		bool _execute_operations()
		{
			bool progress = false;
			switch (_state) {
			case INIT_SB_CONTROL:

				progress |= _sb_control.execute(*_init_sb_control_ptr, _block_io, _crypto, _trust_anchor) ;
				if (_init_sb_control_ptr->complete()) {

					ASSERT(_init_sb_control_ptr->success());
					destroy(_vfs_env.alloc(), _init_sb_control_ptr);
					_init_sb_control_ptr = nullptr;
					if (_verbose)
						log("initialize succeeded");

					if (!_try_start_operation())
						_state = NO_OPERATION;
					progress = true;
				}
				break;

			case DATA_OPERATION:

				progress |= _data_operation.execute({_splitter, _sb_control, *this, _vbd, _free_tree, _meta_tree, _block_io, _crypto, _trust_anchor}) ;
				if (_data_operation.complete()) {
					if (!_try_resume_operation())
						if (!_try_start_operation())
							_state = NO_OPERATION;
					progress = true;
				}
				break;

			case DEINITIALIZE_OPERATION:

				progress |= _deinit_operation.execute({_deinit_fs_ptr, _sb_control, _block_io, _crypto, _trust_anchor}) ;
				if (_deinit_operation.complete()) {
					_state = DEINITIALIZED;
					progress = true;
				}
				break;

			case EXTEND_OPERATION:

				progress |= _extend_operation.execute({_extend_fs_ptr, _sb_control, _vbd, _free_tree, _meta_tree, _block_io, _trust_anchor}) ;
				if (_extend_operation.complete()) {
					if (!_try_start_operation())
						_state = NO_OPERATION;
					progress = true;
				}
				if (_extend_operation.paused()) {
					if (_data_operation.requested()) {
						_data_operation.start();
						_state = DATA_OPERATION;
					} else
						_extend_operation.resume();
					progress = true;
				}
				break;

			case REKEY_OPERATION:

				progress |= _rekey_operation.execute({_rekey_fs_ptr, _sb_control, _vbd, _free_tree, _meta_tree, _block_io, _crypto, _trust_anchor}) ;
				if (_rekey_operation.complete()) {
					if (!_try_start_operation())
						_state = NO_OPERATION;
					progress = true;
				}
				if (_rekey_operation.paused()) {
					if (_data_operation.requested()) {
						_data_operation.start();
						_state = DATA_OPERATION;
					} else
						_rekey_operation.resume();
					progress = true;
				}
				break;

			case NO_OPERATION: progress |= _try_start_operation(); break;
			default: break;
			}
			return progress;
		}

		void _execute()
		{
			while (_execute_operations()) ;
			_wakeup_back_end_services();
		}

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		size_t _data_file_size() const { return (_sb_control.max_vba() + 1) * BLOCK_SIZE; }

		/********************************
		 ** Crypto_key_files_interface **
		 ********************************/

		void add_crypto_key(Key_id key_id) override
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (!key.constructed()) {
					key.construct(key_id,
						open_file(_vfs_env, { _crypto_path, "/keys/", key_id, "/encrypt" }, Directory_service::OPEN_MODE_RDWR),
						open_file(_vfs_env, { _crypto_path, "/keys/", key_id, "/decrypt" }, Directory_service::OPEN_MODE_RDWR)
					);
					return;
				}
			ASSERT_NEVER_REACHED;
		}

		void remove_crypto_key(Key_id key_id) override
		{
			Constructible<Crypto_key> &crypto_key = _crypto_key(key_id);
			_vfs_env.root_dir().close(&crypto_key->encrypt_file);
			_vfs_env.root_dir().close(&crypto_key->decrypt_file);
			crypto_key.destruct();
		}

		Vfs_handle &encrypt_file(Key_id key_id) override { return _crypto_key(key_id)->encrypt_file; }
		Vfs_handle &decrypt_file(Key_id key_id) override { return _crypto_key(key_id)->decrypt_file; }

		/***************************
		 ** Client_data_interface **
		 ***************************/

		void obtain_data(Obtain_data_attr const &attr) override
		{
			attr.out_blk = _splitter.source_buffer(attr.in_vba);
		}

		void supply_data(Supply_data_attr const &attr) override
		{
			_splitter.destination_buffer(attr.in_vba) = attr.in_blk;
		}

	public:

		Plugin(Vfs::Env &vfs_env, Xml_node const &config)
		:
			_vfs_env(vfs_env),
			_verbose(config.attribute_value("verbose", _verbose)),
			_crypto_path(config.attribute_value("crypto", Tresor::Path())),
			_block_io_path(config.attribute_value("block", Tresor::Path())),
			_trust_anchor_path(config.attribute_value("trust_anchor", Tresor::Path()))
		{
			_init_sb_control_ptr = new (_vfs_env.alloc()) Superblock_control::Initialize({_sb_state});
			if (_verbose)
				log("initialize started");
		}

		template <typename FUNC>
		void with_data_file_size(FUNC && func)
		{
			_execute();
			if (_state != INIT_SB_CONTROL)
				func(_data_file_size());
		}

		template <typename FUNC>
		void with_data_operation(FUNC && func)
		{
			_execute();
			func(_data_operation);
			_execute();
		}

		template <typename FUNC>
		void with_rekey_operation(FUNC && func)
		{
			_execute();
			func(_rekey_operation);
			_execute();
		}

		template <typename FUNC>
		void with_extend_operation(FUNC && func)
		{
			_execute();
			func(_extend_operation);
			_execute();
		}

		template <typename FUNC>
		void with_deinit_operation(FUNC && func)
		{
			_execute();
			func(_deinit_operation);
			_execute();
		}

		void manage_extend_file_system(Extend_file_system &extend_fs)
		{
			ASSERT(!_extend_fs_ptr);
			_extend_fs_ptr = &extend_fs;
		}

		void dissolve_extend_file_system(Extend_file_system &extend_fs)
		{
			ASSERT(_extend_fs_ptr == &extend_fs);
			_extend_fs_ptr = nullptr;
		}

		void manage_rekey_file_system(Rekey_file_system &rekey_fs)
		{
			ASSERT(!_rekey_fs_ptr);
			_rekey_fs_ptr = &rekey_fs;
		}

		void dissolve_rekey_file_system(Rekey_file_system &rekey_fs)
		{
			ASSERT(_rekey_fs_ptr == &rekey_fs);
			_rekey_fs_ptr = nullptr;
		}

		void manage_deinit_file_system(Deinitialize_file_system &deinit_fs)
		{
			ASSERT(!_deinit_fs_ptr);
			_deinit_fs_ptr = &deinit_fs;
		}

		void dissolve_deinit_file_system(Deinitialize_file_system &deinit_fs)
		{
			ASSERT(_deinit_fs_ptr == &deinit_fs);
			_deinit_fs_ptr = nullptr;
		}

		bool verbose() const { return _verbose; }
};


class Vfs_tresor::Data_file_system : private Noncopyable, public Single_file_system
{
	private:

		class Vfs_handle : private Noncopyable, public Single_vfs_handle
		{
			private:

				Plugin &_plugin;

			public:

				Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service,
				           Allocator &alloc, Plugin &plugin)
				:
					Single_vfs_handle(dir_service, file_io_service, alloc, 0), _plugin(plugin)
				{ }

				/***********************
				 ** Single_vfs_handle **
				 ***********************/

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					out_count = 0;
					Read_result result = READ_QUEUED;
					_plugin.with_data_operation([&] (Data_operation &data_operation) {

						switch (data_operation.read(seek(), dst)) {
						case Data_operation::PENDING: break;
						case Data_operation::SUCCEEDED:

							out_count = dst.num_bytes;
							result = READ_OK;
							break;

						case Data_operation::FAILED: result = READ_ERR_IO; break;
						};
					});
					return result;
				}

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					out_count = 0;
					Write_result result = WRITE_ERR_WOULD_BLOCK;
					_plugin.with_data_operation([&] (Data_operation &data_operation) {

						switch (data_operation.write(seek(), src)) {
						case Data_operation::PENDING: break;
						case Data_operation::SUCCEEDED:

							out_count = src.num_bytes;
							result = WRITE_OK;
							break;

						case Data_operation::FAILED: result = WRITE_ERR_IO; break;
						};
					});
					return result;
				}

				Sync_result sync() override
				{
					Sync_result result = SYNC_QUEUED;
					_plugin.with_data_operation([&] (Data_operation &data_operation) {

						switch (data_operation.sync()) {
						case Data_operation::PENDING: break;
						case Data_operation::SUCCEEDED:

							result = SYNC_OK;
							break;

						case Data_operation::FAILED: result = SYNC_ERR_INVALID; break;
						};
					});
					return result;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return true; }
		};

		Plugin &_plugin;

	public:

		Data_file_system(Plugin &plugin)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, type_name(), Node_rwx::rw(), Xml_node("<data/>")),
			_plugin(plugin)
		{ }

		static char const *type_name() { return "data"; }

		/************************
		 ** Single_file_system **
		 ************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = STAT_ERR_NO_ENTRY;
			_plugin.with_data_file_size([&] (size_t size) {
				result = Single_file_system::stat(path, out);
				out.size = size;
			});
			return result;
		}

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override { return FTRUNCATE_OK; }

		Open_result open(char const *path, unsigned, Vfs::Vfs_handle **out_handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, _plugin);
			return OPEN_OK;
		}

		char const *type() override { return type_name(); }
};


class Vfs_tresor::Extend_file_system : private Noncopyable, public Single_file_system
{
	private:

		using Registered_watch_handle = Registered<Vfs_watch_handle>;
		using Watch_handle_registry = Registry<Registered_watch_handle>;
		using Content_string = String<11>;

		class Vfs_handle : private Noncopyable, public Single_vfs_handle
		{
			private:

				Plugin &_plugin;

				static Read_result _read_ok(Content_string const &content, Byte_range_ptr const &dst, size_t &out_count)
				{
					copy_cstring(dst.start, content.string(), dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

			public:

				Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service,
				           Allocator &alloc, Plugin &plugin)
				:
					Single_vfs_handle(dir_service, file_io_service, alloc, 0), _plugin(plugin)
				{ }

				/***********************
				 ** Single_vfs_handle **
				 ***********************/

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					out_count = 0;
					if (seek() == dst.num_bytes) {
						return READ_OK;
					}
					if (seek() || dst.num_bytes < Content_string::capacity()) {
						if (_plugin.verbose())
							log("reading extend file failed: malformed arguments");
						return READ_ERR_IO;
					}
					Read_result result = READ_QUEUED;
					_plugin.with_extend_operation([&] (Extend_operation &extend_operation) {

						switch (extend_operation.result()) {
						case Extend_operation::NONE: result = _read_ok("none", dst, out_count); break;
						case Extend_operation::SUCCEEDED: result = _read_ok("succeeded", dst, out_count); break;
						case Extend_operation::FAILED: result = _read_ok("failed", dst, out_count); break;
						case Extend_operation::PENDING: break;
						}
					});
					return result;
				}

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					out_count = 0;
					char tree_arg[16];
					Arg_string::find_arg(src.start, "tree").string(tree_arg, sizeof(tree_arg), "-");
					unsigned long blocks_arg = Arg_string::find_arg(src.start, "blocks").ulong_value(0);
					if (seek() || !blocks_arg) {
						if (_plugin.verbose())
							log("writing extend file failed: malformed arguments");
						return WRITE_ERR_IO;
					}
					Write_result result = WRITE_ERR_IO;
					_plugin.with_extend_operation([&] (Extend_operation &extend_operation) {

						if (!strcmp("ft", tree_arg, 2)) {

							if (!extend_operation.request_for_free_tree(blocks_arg)) {
								result = WRITE_ERR_IO;
								if (_plugin.verbose())
									log("writing extend file failed: failed to request operation");
								return;
							}

						} else if (!strcmp("vbd", tree_arg, 3)) {

							if (!extend_operation.request_for_vbd(blocks_arg)) {
								result = WRITE_ERR_IO;
								if (_plugin.verbose())
									log("writing extend file failed: failed to request operation");
								return;
							}

						} else {

							result = WRITE_ERR_IO;
							if (_plugin.verbose())
								log("writing extend file failed: malformed tree argument");
							return;
						}
						out_count = src.num_bytes;
						result = WRITE_OK;
					});
					return result;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return true; }
		};

		Watch_handle_registry _handle_registry { };
		Plugin &_plugin;

	public:

		Extend_file_system(Plugin &plugin)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(), Node_rwx::rw(), Xml_node("<extend/>")),
			_plugin(plugin)
		{
			_plugin.manage_extend_file_system(*this);
		}

		static char const *type_name() { return "extend"; }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		/************************
		 ** Single_file_system **
		 ************************/

		Watch_result watch(char const *path, Vfs_watch_handle **handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc) Registered_watch_handle(_handle_registry, *this, alloc);
				return WATCH_OK;
			}
			catch (Out_of_ram) { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(), static_cast<Registered_watch_handle *>(handle));
		}

		char const *type() override { return type_name(); }

		Open_result open(char const *path, unsigned, Vfs::Vfs_handle **out_handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, _plugin);
				return OPEN_OK;
			}
			catch (Out_of_ram) { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::capacity();
			return result;
		}

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override { return FTRUNCATE_OK; }
};


class Vfs_tresor::Rekey_file_system : private Noncopyable, public Single_file_system
{
	private:

		using Registered_watch_handle = Registered<Vfs_watch_handle>;
		using Watch_handle_registry = Registry<Registered_watch_handle>;

		Watch_handle_registry _handle_registry { };

		Plugin &_plugin;

		using Content_string = String<11>;

		class Vfs_handle : private Noncopyable, public Single_vfs_handle
		{
			private:

				Plugin &_plugin;

				static Read_result _read_ok(Content_string const &content, Byte_range_ptr const &dst, size_t &out_count)
				{
					copy_cstring(dst.start, content.string(), dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

			public:

				Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service,
				           Allocator &alloc, Plugin &plugin)
				:
					Single_vfs_handle(dir_service, file_io_service, alloc, 0), _plugin(plugin)
				{ }

				/***********************
				 ** Single_vfs_handle **
				 ***********************/

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					out_count = 0;
					if (seek() == dst.num_bytes) {
						return READ_OK;
					}
					if (seek() || dst.num_bytes < Content_string::capacity()) {
						if (_plugin.verbose())
							log("reading rekey file failed: malformed arguments");
						return READ_ERR_IO;
					}
					Read_result result = READ_QUEUED;
					_plugin.with_rekey_operation([&] (Rekey_operation &rekey_operation) {

						switch (rekey_operation.result()) {
						case Rekey_operation::NONE: result = _read_ok("none", dst, out_count); break;
						case Rekey_operation::SUCCEEDED: result = _read_ok("succeeded", dst, out_count); break;
						case Rekey_operation::FAILED: result = _read_ok("failed", dst, out_count); break;
						case Rekey_operation::PENDING: break;
						}
					});
					return result;
				}

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					out_count = 0;
					bool rekey_arg { false };
					Genode::ascii_to(src.start, rekey_arg);
					if (seek() || !rekey_arg) {
						if (_plugin.verbose())
							log("writing rekey file failed: malformed arguments");
						return WRITE_ERR_IO;
					}
					Write_result result = WRITE_ERR_IO;
					_plugin.with_rekey_operation([&] (Rekey_operation &rekey_operation) {

						if (!rekey_operation.request()) {
							result = WRITE_ERR_IO;
							if (_plugin.verbose())
								log("writing rekey file failed: failed to request operation");
							return;
						}
						out_count = src.num_bytes;
						result = WRITE_OK;
					});
					return result;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return true; }
		};

	public:

		Rekey_file_system(Plugin &plugin)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(), Node_rwx::rw(), Xml_node("<rekey/>")),
			_plugin(plugin)
		{
			_plugin.manage_rekey_file_system(*this);
		}

		static char const *type_name() { return "rekey"; }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		/************************
		 ** Single_file_system **
		 ************************/

		char const *type() override { return type_name(); }

		Watch_result watch(char const *path, Vfs_watch_handle **handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc) Registered_watch_handle(_handle_registry, *this, alloc);
				return WATCH_OK;
			}
			catch (Out_of_ram) { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(), static_cast<Registered_watch_handle *>(handle));
		}

		Open_result open(char const *path, unsigned, Vfs::Vfs_handle **out_handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, _plugin);
				return OPEN_OK;
			}
			catch (Out_of_ram) { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::capacity();
			return result;
		}

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override { return FTRUNCATE_OK; }
};


class Vfs_tresor::Deinitialize_file_system : private Noncopyable, public Single_file_system
{
	private:

		using Registered_watch_handle = Registered<Vfs_watch_handle>;
		using Watch_handle_registry = Registry<Registered_watch_handle>;
		using Content_string = String<11>;

		Watch_handle_registry _handle_registry { };
		Plugin &_plugin;

		class Vfs_handle : public Single_vfs_handle
		{
			private:

				Plugin &_plugin;

				static Read_result _read_ok(Content_string const &content, Byte_range_ptr const &dst, size_t &out_count)
				{
					copy_cstring(dst.start, content.string(), dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

			public:

				Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service,
				           Allocator &alloc, Plugin &plugin)
				:
					Single_vfs_handle(dir_service, file_io_service, alloc, 0), _plugin(plugin)
				{ }

				/***********************
				 ** Single_vfs_handle **
				 ***********************/

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					out_count = 0;
					if (seek() == dst.num_bytes) {
						return READ_OK;
					}
					if (seek() || dst.num_bytes < Content_string::capacity()) {
						if (_plugin.verbose())
							log("reading deinitialize file failed: malformed arguments");
						return READ_ERR_IO;
					}
					Read_result result = READ_QUEUED;
					_plugin.with_deinit_operation([&] (Deinitialize_operation &deinit_operation) {

						switch (deinit_operation.result()) {
						case Deinitialize_operation::NONE: result = _read_ok("none", dst, out_count); break;
						case Deinitialize_operation::SUCCEEDED: result = _read_ok("succeeded", dst, out_count); break;
						case Deinitialize_operation::FAILED: result = _read_ok("failed", dst, out_count); break;
						case Deinitialize_operation::PENDING: break;
						}
					});
					return result;
				}

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					out_count = 0;
					bool deinitialize_arg { false };
					Genode::ascii_to(src.start, deinitialize_arg);
					if (seek() || !deinitialize_arg) {
						if (_plugin.verbose())
							log("writing deinitialize file failed: malformed arguments");

						return WRITE_ERR_IO;
					}
					Write_result result = WRITE_ERR_IO;
					_plugin.with_deinit_operation([&] (Deinitialize_operation &deinit_operation) {

						if (!deinit_operation.request()) {
							result = WRITE_ERR_IO;
							if (_plugin.verbose())
								log("writing deinitialize file failed: failed to request operation");
							return;
						}
						out_count = src.num_bytes;
						result = WRITE_OK;
					});
					return result;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return true; }
		};

	public:

		Deinitialize_file_system(Plugin &plugin)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(), Node_rwx::rw(), Xml_node("<deinitialize/>")),
			_plugin(plugin)
		{
			_plugin.manage_deinit_file_system(*this);
		}

		static char const *type_name() { return "deinitialize"; }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		/************************
		 ** Single_file_system **
		 ************************/

		char const *type() override { return type_name(); }

		Watch_result watch(char const *path, Vfs_watch_handle **handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc) Registered_watch_handle(_handle_registry, *this, alloc);
				return WATCH_OK;
			}
			catch (Out_of_ram) { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(), static_cast<Registered_watch_handle *>(handle));
		}

		Open_result open(char const  *path, unsigned, Vfs::Vfs_handle **out_handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, _plugin);
				return OPEN_OK;
			}
			catch (Out_of_ram) { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::capacity();
			return result;
		}

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override { return FTRUNCATE_OK; }
};


class Vfs_tresor::Current_local_factory : private Noncopyable, public File_system_factory
{
	private:

		Data_file_system _data_fs;

		/*************************
		 ** File_system_factory **
		 *************************/

		Vfs::File_system *create(Vfs::Env&, Xml_node node) override
		{
			if (node.has_type(Data_file_system::type_name()))
				return &_data_fs;

			return nullptr;
		}

	public:

		Current_local_factory(Vfs::Env &, Plugin &plugin) : _data_fs(plugin) { }
};


class Vfs_tresor::Current_file_system : private Current_local_factory, public Dir_file_system
{
	private:

		using Config = String<128>;

		static Config _config()
		{
			char buf[Config::capacity()] { };
			Xml_generator xml(buf, sizeof(buf), "dir", [&] ()
			{
				xml.attribute("name", String<16>("current"));
				xml.node("data", [&] () {
					xml.attribute("readonly", false);
				});
			});

			return Config(Cstring(buf));
		}

	public:

		Current_file_system(Vfs::Env &vfs_env, Plugin &plugin)
		:
			Current_local_factory(vfs_env, plugin),
			Dir_file_system(vfs_env, Xml_node(_config().string()), *this)
		{ }

		static char const *type_name() { return "current"; }

		/*********************
		 ** Dir_file_system **
		 *********************/

		char const *type() override { return type_name(); }
};


class Vfs_tresor::Control_local_factory : private Noncopyable, public File_system_factory
{
	private:

		Plugin &_plugin;
		Rekey_file_system _rekey_fs;
		Deinitialize_file_system _deinitialize_fs;
		Extend_file_system _extend_fs;

	public:

		Control_local_factory(Vfs::Env &, Xml_node, Plugin &plugin)
		:
			_plugin(plugin), _rekey_fs(plugin), _deinitialize_fs(plugin), _extend_fs(plugin)
		{ }

		~Control_local_factory()
		{
			_plugin.dissolve_rekey_file_system(_rekey_fs);
			_plugin.dissolve_deinit_file_system(_deinitialize_fs);
			_plugin.dissolve_extend_file_system(_extend_fs);
		}

		/*************************
		 ** File_system_factory **
		 *************************/

		Vfs::File_system *create(Vfs::Env&, Xml_node node) override
		{
			if (node.has_type(Rekey_file_system::type_name()))
				return &_rekey_fs;

			if (node.has_type(Deinitialize_file_system::type_name()))
				return &_deinitialize_fs;

			if (node.has_type(Extend_file_system::type_name()))
				return &_extend_fs;

			return nullptr;
		}
};


class Vfs_tresor::Control_file_system : private Control_local_factory, public Dir_file_system
{
	private:

		using Config = String<256>;

		static Config _config()
		{
			char buf[Config::capacity()] { };
			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				xml.attribute("name", "control");
				xml.node("rekey", [&] () { });
				xml.node("extend", [&] () { });
				xml.node("deinitialize", [&] () { });
			});
			return Config(Cstring(buf));
		}

	public:

		Control_file_system(Vfs::Env &vfs_env, Xml_node  node, Plugin &plugin)
		:
			Control_local_factory(vfs_env, node, plugin),
			Dir_file_system(vfs_env, Xml_node(_config().string()), *this)
		{ }

		static char const *type_name() { return "control"; }

		/*********************
		 ** Dir_file_system **
		 *********************/

		char const *type() override { return type_name(); }
};


class Vfs_tresor::Local_factory : private Noncopyable, public File_system_factory
{
	private:

		Plugin  &_plugin;
		Current_file_system _current_fs;
		Control_file_system _control_fs;

	public:

		Local_factory(Vfs::Env &vfs_env, Xml_node config, Plugin &plugin)
		:
			_plugin(plugin), _current_fs(vfs_env, plugin), _control_fs(vfs_env, config, plugin)
		{ }

		/*************************
		 ** File_system_factory **
		 *************************/

		Vfs::File_system *create(Vfs::Env&, Xml_node node) override
		{
			if (node.has_type(Current_file_system::type_name()))
				return &_current_fs;

			if (node.has_type(Control_file_system::type_name()))
				return &_control_fs;

			return nullptr;
		}
};


class Vfs_tresor::File_system : private Local_factory, public Dir_file_system
{
	private:

		using Config = String<256>;

		Plugin &_plugin;

		static Config _config(Xml_node node)
		{
			char buf[Config::capacity()] { };
			Xml_generator xml(buf, sizeof(buf), "dir", [&] ()
			{
				xml.attribute("name", node.attribute_value("name", String<64>("tresor")));
				xml.node("control", [&] () { });
				xml.node("current", [&] () { });
			});
			return Config(Cstring(buf));
		}

	public:

		File_system(Vfs::Env &vfs_env, Xml_node node, Plugin &plugin)
		:
			Local_factory(vfs_env, node, plugin),
			Dir_file_system(vfs_env, Xml_node(_config(node).string()), *this),
			_plugin(plugin)
		{ }
};


bool Vfs_tresor::Rekey_operation::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_state) {
	case STARTED:

		_rekey.construct(Superblock_control::Rekey::Attr{_complete});
		_state = REKEY;
		progress = true;
		if (_verbose)
			log("rekey started");
		break;

	case REKEY:

		progress |= attr.sb_control.execute(
			*_rekey, attr.vbd, attr.free_tree, attr.meta_tree, attr.block_io, attr.crypto, attr.trust_anchor);

		if (_rekey->complete()) {
			if (_rekey->success()) {
				if (_complete) {
					_success = true;
					_state = COMPLETE;
					if (attr.rekey_fs_ptr)
						attr.rekey_fs_ptr->trigger_watch_response();
					if (_verbose)
						log("rekey succeeded");
				} else
					_state = PAUSED;
			} else {
				_success = false;
				_state = COMPLETE;
				if (attr.rekey_fs_ptr)
					attr.rekey_fs_ptr->trigger_watch_response();
				if (_verbose)
					log("rekey failed");
			}
			_rekey.destruct();
			progress = true;
		}
		break;

	case RESUMED:

		_rekey.construct(Superblock_control::Rekey::Attr{_complete});
		_state = REKEY;
		progress = true;
		break;

	default: break;
	}
	return progress;
}


bool Vfs_tresor::Extend_operation::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_state) {
	case EXTEND_FT_STARTED:

		_extend_ft.construct(Superblock_control::Extend_free_tree::Attr{_num_blocks, _complete});
		_state = EXTEND_FT;
		progress = true;
		if (_verbose)
			log("extend free tree started");
		break;

	case EXTEND_FT:

		progress |= attr.sb_control.execute(
			*_extend_ft, attr.free_tree, attr.meta_tree, attr.block_io, attr.trust_anchor);

		if (_extend_ft->complete()) {
			if (_extend_ft->success()) {
				if (_complete) {
					_success = true;
					_state = COMPLETE;
					if (attr.extend_fs_ptr)
						attr.extend_fs_ptr->trigger_watch_response();
					if (_verbose)
						log("extend free tree succeeded");
				} else
					_state = EXTEND_FT_PAUSED;
			} else {
				_success = false;
				_state = COMPLETE;
				if (attr.extend_fs_ptr)
					attr.extend_fs_ptr->trigger_watch_response();
				if (_verbose)
					log("extend free tree failed");
			}
			_extend_ft.destruct();
			progress = true;
		}
		break;

	case EXTEND_FT_RESUMED:

		_extend_ft.construct(Superblock_control::Extend_free_tree::Attr{_num_blocks, _complete});
		_state = EXTEND_FT;
		progress = true;
		break;

	case EXTEND_VBD_STARTED:

		_extend_vbd.construct(Superblock_control::Extend_vbd::Attr{_num_blocks, _complete});
		_state = EXTEND_VBD;
		progress = true;
		if (_verbose)
			log("extend virtual block device started");
		break;

	case EXTEND_VBD:

		progress |= attr.sb_control.execute(
			*_extend_vbd, attr.vbd, attr.free_tree, attr.meta_tree, attr.block_io, attr.trust_anchor);

		if (_extend_vbd->complete()) {
			if (_extend_vbd->success()) {
				if (_complete) {
					_success = true;
					_state = COMPLETE;
					if (attr.extend_fs_ptr)
						attr.extend_fs_ptr->trigger_watch_response();
					if (_verbose)
						log("extend virtual block device succeeded");
				} else
					_state = EXTEND_VBD_PAUSED;
			} else {
				_success = false;
				_state = COMPLETE;
				if (attr.extend_fs_ptr)
					attr.extend_fs_ptr->trigger_watch_response();
				if (_verbose)
					log("extend virtual block device failed");
			}
			_extend_vbd.destruct();
			progress = true;
		}
		break;

	case EXTEND_VBD_RESUMED:

		_extend_vbd.construct(Superblock_control::Extend_vbd::Attr{_num_blocks, _complete});
		_state = EXTEND_VBD;
		progress = true;
		break;

	default: break;
	}
	return progress;
}


bool Vfs_tresor::Deinitialize_operation::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_state) {
	case STARTED:

		_deinit_sb_control.construct(Superblock_control::Deinitialize::Attr{});
		_state = DEINIT_SB_CONTROL;
		progress = true;
		if (_verbose)
			log("deinitialize started");
		break;

	case DEINIT_SB_CONTROL:

		progress |= attr.sb_control.execute(
			*_deinit_sb_control, attr.block_io, attr.crypto, attr.trust_anchor);

		if (_deinit_sb_control->complete()) {
			if (_deinit_sb_control->success()) {
				_success = true;
				_state = COMPLETE;
				if (_verbose)
					log("deinitialize succeeded");
			} else {
				_success = false;
				_state = DEINIT_SB_CONTROL;
				if (_verbose)
					log("deinitialize failed");
			}
			_deinit_sb_control.destruct();
			if (attr.deinit_fs_ptr)
				attr.deinit_fs_ptr->trigger_watch_response();
			progress = true;
		}
		break;

	default: break;
	}
	return progress;
}


extern "C" File_system_factory *vfs_file_system_factory(void)
{
	class Factory : public File_system_factory
	{
		private:

			Allocator *_plugin_alloc_ptr { };
			Vfs_tresor::Plugin *_plugin_ptr { };

		public:

			~Factory()
			{
				if (_plugin_ptr)
					destroy(_plugin_alloc_ptr, _plugin_ptr);
			}

			/*************************
			 ** File_system_factory **
			 *************************/

			Vfs::File_system *create(Vfs::Env &vfs_env, Xml_node node) override
			{
				try {
					if (!_plugin_ptr) {
						_plugin_alloc_ptr = &vfs_env.alloc();
						_plugin_ptr = new (_plugin_alloc_ptr) Vfs_tresor::Plugin { vfs_env, node };
					}
					return new (vfs_env.alloc()) Vfs_tresor::File_system(vfs_env, node, *_plugin_ptr);

				} catch (...) { error("could not create 'tresor_fs' "); }
				return nullptr;
			}
	};

	static Factory factory { };
	return &factory;
}
