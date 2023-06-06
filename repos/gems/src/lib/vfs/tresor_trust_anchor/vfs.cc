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

/* OpenSSL includes */
#include <openssl/sha.h>

/* tresor includes */
#include <tresor/vfs/io_job.h>

/* local includes */
#include <aes_256.h>

enum { PRIVATE_KEY_SIZE = 32 };
enum { PASSPHRASE_HASH_SIZE = 32 };
enum { VERBOSE = 0 };


namespace Vfs_tresor_trust_anchor {

	using namespace Vfs;
	using namespace Genode;

	class Generate_key_file_system;
	class Hashsum_file_system;
	class Encrypt_file_system;
	class Decrypt_file_system;
	class Initialize_file_system;

	struct Local_factory;
	class  File_system;
}


class Trust_anchor
{
	public:

		using Path = Genode::Path<256>;

		Path const key_file_name  { "encrypted_private_key" };
		Path const hash_file_name { "superblock_hash" };

		struct Complete_request
		{
			bool valid;
			bool success;
		};

	private:

		Trust_anchor(Trust_anchor const &) = delete;
		Trust_anchor &operator=(Trust_anchor const&) = delete;

		using size_t               = Genode::size_t;
		using Byte_range_ptr       = Genode::Byte_range_ptr;
		using Const_byte_range_ptr = Genode::Const_byte_range_ptr;

		Vfs::Env &_vfs_env;

		enum class State {
			UNINITIALIZED,
			INITIALIZIE_IN_PROGRESS,
			INITIALIZED,
		};
		State _state { State::UNINITIALIZED };

		enum class Lock_state { LOCKED, UNLOCKED };
		Lock_state _lock_state { Lock_state::LOCKED };

		enum class Job {
			NONE,
			DECRYPT,
			ENCRYPT,
			GENERATE,
			INIT,
			READ_HASH,
			UPDATE_HASH,
			UNLOCK
		};
		Job _job { Job::NONE };

		enum class Job_state
		{
			NONE,
			INIT_READ_JITTERENTROPY_PENDING,
			INIT_READ_JITTERENTROPY_IN_PROGRESS,
			PENDING,
			IN_PROGRESS,
			FINAL_SYNC,
			COMPLETE
		};

		Job_state _job_state { Job_state::NONE };

		bool _job_success { false };

		struct Private_key
		{
			unsigned char value[PRIVATE_KEY_SIZE] { };
		};
		Private_key _private_key { };

		struct Last_hash
		{
			enum { HASH_LEN = 32 };
			unsigned char value[HASH_LEN] { };
			static constexpr size_t length = HASH_LEN;
		};
		Last_hash _last_hash { };

		struct Key
		{
			enum { KEY_LEN = 32 };
			unsigned char value[KEY_LEN] { };
			static constexpr size_t length = KEY_LEN;
		};
		Key _decrypt_key   { };
		Key _encrypt_key   { };
		Key _generated_key { };

		bool _execute_encrypt()
		{
			switch (_job_state) {
			case Job_state::PENDING:
			{
				Key key_plaintext { };
				Genode::memcpy(
					key_plaintext.value, _encrypt_key.value, Key::KEY_LEN);

				Aes_256::encrypt_with_zeroed_iv(
					_encrypt_key.value,
					Key::KEY_LEN,
					key_plaintext.value,
					_private_key.value,
					PRIVATE_KEY_SIZE);

				_job_state = Job_state::COMPLETE;
				_job_success = true;
				return true;
			}
			case Job_state::COMPLETE:
			case Job_state::IN_PROGRESS:
			case Job_state::NONE:
			case Job_state::INIT_READ_JITTERENTROPY_PENDING:
			case Job_state::INIT_READ_JITTERENTROPY_IN_PROGRESS:
			case Job_state::FINAL_SYNC:
				break;
			}

			return false;
		}

		bool _execute_decrypt()
		{
			switch (_job_state) {
			case Job_state::PENDING:
			{
				Key key_ciphertext { };
				Genode::memcpy(
					key_ciphertext.value, _decrypt_key.value, Key::KEY_LEN);

				Aes_256::decrypt_with_zeroed_iv(
					_decrypt_key.value,
					Key::KEY_LEN,
					key_ciphertext.value,
					_private_key.value,
					PRIVATE_KEY_SIZE);

				_job_state = Job_state::COMPLETE;
				_job_success = true;
				return true;
			}
			case Job_state::COMPLETE:
			case Job_state::IN_PROGRESS:
			case Job_state::NONE:
			case Job_state::INIT_READ_JITTERENTROPY_PENDING:
			case Job_state::INIT_READ_JITTERENTROPY_IN_PROGRESS:
			case Job_state::FINAL_SYNC:
				break;
			}

			return false;
		}

		bool _execute_generate(Key &key)
		{
			bool progress = false;

			switch (_job_state) {
			case Job_state::INIT_READ_JITTERENTROPY_PENDING:
			case Job_state::PENDING:
			{
				if (!_open_jitterentropy_file_and_queue_read()) {
					break;
				}
				_job_state = Job_state::IN_PROGRESS;
				progress = true;
			}
			[[fallthrough]];

			case Job_state::INIT_READ_JITTERENTROPY_IN_PROGRESS:
			case Job_state::IN_PROGRESS:
			{
				if (!_read_jitterentropy_file_finished()) {
					break;
				}
				if (_jitterentropy_io_job_buffer.size != (size_t)Key::KEY_LEN) {
					class Bad_jitterentropy_io_buffer_size { };
					throw Bad_jitterentropy_io_buffer_size { };
				}
				Genode::memcpy(key.value,
				               _jitterentropy_io_job_buffer.base,
				               _jitterentropy_io_job_buffer.size);

				_job_state = Job_state::COMPLETE;
				_job_success = true;
				progress = true;
				break;
			}

			case Job_state::COMPLETE:
			case Job_state::FINAL_SYNC:
			case Job_state::NONE:
				break;
			}

			return progress;
		}

		bool _execute_unlock()
		{
			bool progress = false;

			switch (_job_state) {
			case Job_state::PENDING:
			{
				if (!_open_key_file_and_queue_read(_base_path)) {
					break;
				}

				_job_state = Job_state::IN_PROGRESS;
				progress |= true;
			}

			[[fallthrough]];
			case Job_state::IN_PROGRESS:
			{
				if (!_read_key_file_finished()) {
					break;
				}
				if (_key_io_job_buffer.size == Aes_256_key_wrap::CIPHERTEXT_SIZE) {

					bool private_key_corrupt;
					Aes_256_key_wrap::unwrap_key(
						_private_key.value,
						sizeof(_private_key.value),
						private_key_corrupt,
						(unsigned char *)_key_io_job_buffer.base,
						_key_io_job_buffer.size,
						(unsigned char *)_passphrase_hash_buffer.base,
						_passphrase_hash_buffer.size);

					if (private_key_corrupt) {

						Genode::error("failed to unwrap the private key");
						_job_success = false;

					} else {

						_job_success = true;
					}
					_job_state = Job_state::COMPLETE;
					progress = true;

				} else {

					Genode::error(
						"content read from file 'encrypted_private_key' "
						"has unexpected size");

					_job_state   = Job_state::COMPLETE;
					_job_success = false;
					progress = true;
				}
			}

			[[fallthrough]];
			case Job_state::COMPLETE:
				break;

			case Job_state::NONE: [[fallthrough]];
			default:              break;
			}

			return progress;
		}

		bool _execute_init()
		{
			bool progress = false;

			switch (_job_state) {
			case Job_state::INIT_READ_JITTERENTROPY_PENDING:
			{
				if (!_open_private_key_file_and_queue_read()) {
					break;
				}
				_job_state = Job_state::INIT_READ_JITTERENTROPY_IN_PROGRESS;
				progress = true;
			}
			[[fallthrough]];
			case Job_state::INIT_READ_JITTERENTROPY_IN_PROGRESS:
			{
				if (!_read_private_key_file_finished()) {
					break;
				}
				if (_private_key_io_job_buffer.size != (size_t)PRIVATE_KEY_SIZE) {
					class Bad_private_key_io_buffer_size { };
					throw Bad_private_key_io_buffer_size { };
				}
				Genode::memcpy(
					_private_key.value,
					_private_key_io_job_buffer.base,
					_private_key_io_job_buffer.size);

				_key_io_job_buffer.size = Aes_256_key_wrap::CIPHERTEXT_SIZE;
				Aes_256_key_wrap::wrap_key(
					(unsigned char *)_key_io_job_buffer.base,
					_key_io_job_buffer.size,
					(unsigned char *)_private_key_io_job_buffer.base,
					_private_key_io_job_buffer.size,
					(unsigned char *)_passphrase_hash_buffer.base,
					_passphrase_hash_buffer.size);

				_job_state = Job_state::PENDING;
				progress = true;
			}
			[[fallthrough]];
			case Job_state::PENDING:
			{
				if (!_open_key_file_and_write(_base_path)) {
					_job_state = Job_state::COMPLETE;
					_job_success = false;
					return true;
				}

				_job_state = Job_state::IN_PROGRESS;
				progress |= true;
			}

			[[fallthrough]];
			case Job_state::IN_PROGRESS:
				if (!_write_op_on_key_file_is_in_final_sync_step()) {
					break;
				}

				_job_state   = Job_state::FINAL_SYNC;
				_job_success = true;

				progress |= true;
			[[fallthrough]];
			case Job_state::FINAL_SYNC:

				if (!_final_sync_of_write_op_on_key_file_finished()) {
					break;
				}

				_job_state   = Job_state::COMPLETE;
				_job_success = true;

				progress |= true;
			[[fallthrough]];
			case Job_state::COMPLETE:
				break;

			case Job_state::NONE: [[fallthrough]];
			default:              break;
			}

			return progress;
		}

		bool _execute_read_hash()
		{
			bool progress = false;

			switch (_job_state) {
			case Job_state::PENDING:
				if (!_open_hash_file_and_queue_read(_base_path)) {
					_job_state = Job_state::COMPLETE;
					_job_success = false;
					return true;
				}

				_job_state = Job_state::IN_PROGRESS;
				progress |= true;

			[[fallthrough]];
			case Job_state::IN_PROGRESS:
			{
				if (!_read_hash_file_finished()) {
					break;
				}

				size_t const hash_len =
					Genode::min(_hash_io_job_buffer.size,
					            sizeof (_last_hash.value));
				Genode::memcpy(_last_hash.value, _hash_io_job_buffer.buffer,
				               hash_len);

				_job_state   = Job_state::COMPLETE;
				_job_success = true;

				progress |= true;
			}
			[[fallthrough]];
			case Job_state::COMPLETE:
				break;

			case Job_state::NONE: [[fallthrough]];
			default:              break;
			}

			return progress;
		}


		bool _execute_update_hash()
		{
			bool progress = false;

			switch (_job_state) {
			case Job_state::PENDING:
			{
				if (!_open_hash_file_and_write(_base_path)) {
					_job_state = Job_state::COMPLETE;
					_job_success = false;
					return true;
				}

				/* keep new hash in last hash */
				size_t const hash_len =
					Genode::min(_hash_io_job_buffer.size,
					            sizeof (_hash_io_job_buffer.size));
				Genode::memcpy(_last_hash.value, _hash_io_job_buffer.buffer,
				               hash_len);

				_job_state = Job_state::IN_PROGRESS;
				progress |= true;
			}
			[[fallthrough]];
			case Job_state::IN_PROGRESS:
				if (!_write_op_on_hash_file_is_in_final_sync_step()) {
					break;
				}

				_job_state   = Job_state::FINAL_SYNC;
				_job_success = true;

				progress |= true;
			[[fallthrough]];
			case Job_state::FINAL_SYNC:

				if (!_final_sync_of_write_op_on_hash_file_finished()) {
					break;
				}

				_job_state   = Job_state::COMPLETE;
				_job_success = true;

				progress |= true;
				break;

			case Job_state::COMPLETE:
			case Job_state::NONE:
			case Job_state::INIT_READ_JITTERENTROPY_PENDING:
			case Job_state::INIT_READ_JITTERENTROPY_IN_PROGRESS:
				break;
			}

			return progress;
		}

		bool _execute_one_step()
		{
			switch (_job) {
			case Job::DECRYPT:     return _execute_decrypt();
			case Job::ENCRYPT:     return _execute_encrypt();
			case Job::GENERATE:    return _execute_generate(_generated_key);
			case Job::INIT:        return _execute_init();
			case Job::READ_HASH:   return _execute_read_hash();
			case Job::UPDATE_HASH: return _execute_update_hash();
			case Job::UNLOCK     : return _execute_unlock();
			case Job::NONE: [[fallthrough]];
			default: return false;
			}

			/* never reached */
			return false;
		}

		void _close_handle(Vfs::Vfs_handle **handle)
		{
			if (*handle == nullptr)
				return;

			(*handle)->close();
			(*handle) = nullptr;
		}

		struct Jitterentropy_io_job_buffer : Util::Io_job::Buffer
		{
			char buffer[32] { };

			Jitterentropy_io_job_buffer()
			{
				Buffer::base = buffer;
				Buffer::size = sizeof (buffer);
			}
		};

		Vfs::Vfs_handle *_jitterentropy_handle  { nullptr };
		Genode::Constructible<Util::Io_job> _jitterentropy_io_job { };
		Jitterentropy_io_job_buffer _jitterentropy_io_job_buffer { };


		struct Private_key_io_job_buffer : Util::Io_job::Buffer
		{
			char buffer[PRIVATE_KEY_SIZE] { };

			Private_key_io_job_buffer()
			{
				Buffer::base = buffer;
				Buffer::size = sizeof (buffer);
			}
		};

		Vfs::Vfs_handle *_private_key_handle { nullptr };
		Genode::Constructible<Util::Io_job> _private_key_io_job { };
		Private_key_io_job_buffer _private_key_io_job_buffer { };

		/* key */

		Vfs::Vfs_handle *_key_handle  { nullptr };
		Genode::Constructible<Util::Io_job> _key_io_job { };

		struct Key_io_job_buffer : Util::Io_job::Buffer
		{
			char buffer[Aes_256_key_wrap::CIPHERTEXT_SIZE] { };

			Key_io_job_buffer()
			{
				Buffer::base = buffer;
				Buffer::size = sizeof (buffer);
			}
		};

		struct Passphrase_hash_buffer : Util::Io_job::Buffer
		{
			char buffer[PASSPHRASE_HASH_SIZE] { };

			Passphrase_hash_buffer()
			{
				Buffer::base = buffer;
				Buffer::size = sizeof (buffer);
			}
		};

		Key_io_job_buffer      _key_io_job_buffer      { };
		Passphrase_hash_buffer _passphrase_hash_buffer { };

		bool _check_key_file(Path const &path)
		{
			Path file_path = path;
			file_path.append_element(key_file_name.string());

			using Stat_result = Vfs::Directory_service::Stat_result;

			Vfs::Directory_service::Stat out_stat { };
			Stat_result const stat_res =
				_vfs_env.root_dir().stat(file_path.string(), out_stat);

			if (stat_res == Stat_result::STAT_OK) {

				_state = State::INITIALIZED;
				return true;
			}
			_state = State::UNINITIALIZED;
			return false;
		}

		bool _open_private_key_file_and_queue_read()
		{
			Path file_path = "/dev/jitterentropy";
			using Result = Vfs::Directory_service::Open_result;

			Result const res =
				_vfs_env.root_dir().open(file_path.string(),
				                         Vfs::Directory_service::OPEN_MODE_RDONLY,
				                         (Vfs::Vfs_handle **)&_private_key_handle,
				                         _vfs_env.alloc());
			if (res != Result::OPEN_OK) {
				Genode::error("could not open '", file_path.string(), "'");
				return false;
			}

			_private_key_io_job.construct(*_private_key_handle, Util::Io_job::Operation::READ,
			                      _private_key_io_job_buffer, 0,
			                      Util::Io_job::Partial_result::ALLOW);

			if (_private_key_io_job->execute() && _private_key_io_job->completed()) {
				_close_handle(&_private_key_handle);
				_private_key_io_job_buffer.size = _private_key_io_job->current_offset();
				_private_key_io_job.destruct();
				return true;
			}
			return true;
		}

		bool _open_jitterentropy_file_and_queue_read()
		{
			Path file_path = "/dev/jitterentropy";
			using Result = Vfs::Directory_service::Open_result;

			Result const res =
				_vfs_env.root_dir().open(file_path.string(),
				                         Vfs::Directory_service::OPEN_MODE_RDONLY,
				                         (Vfs::Vfs_handle **)&_jitterentropy_handle,
				                         _vfs_env.alloc());
			if (res != Result::OPEN_OK) {
				Genode::error("could not open '", file_path.string(), "'");
				return false;
			}

			_jitterentropy_io_job.construct(*_jitterentropy_handle, Util::Io_job::Operation::READ,
			                      _jitterentropy_io_job_buffer, 0,
			                      Util::Io_job::Partial_result::ALLOW);

			if (_jitterentropy_io_job->execute() && _jitterentropy_io_job->completed()) {
				_close_handle(&_jitterentropy_handle);
				_jitterentropy_io_job_buffer.size = _jitterentropy_io_job->current_offset();
				_jitterentropy_io_job.destruct();
				return true;
			}
			return true;
		}

		bool _open_key_file_and_queue_read(Path const &path)
		{
			Path file_path = path;
			file_path.append_element(key_file_name.string());

			using Result = Vfs::Directory_service::Open_result;

			Result const res =
				_vfs_env.root_dir().open(file_path.string(),
				                         Vfs::Directory_service::OPEN_MODE_RDONLY,
				                         (Vfs::Vfs_handle **)&_key_handle,
				                         _vfs_env.alloc());
			if (res != Result::OPEN_OK) {
				Genode::error("could not open '", file_path.string(), "'");
				return false;
			}

			_key_io_job.construct(*_key_handle, Util::Io_job::Operation::READ,
			                      _key_io_job_buffer, 0,
			                      Util::Io_job::Partial_result::ALLOW);
			if (_key_io_job->execute() && _key_io_job->completed()) {
				_state = State::INITIALIZED;
				_close_handle(&_key_handle);
				return true;
			}
			return true;
		}

		bool _read_private_key_file_finished()
		{
			if (!_private_key_io_job.constructed()) {
				return true;
			}

			// XXX trigger sync

			bool const progress  = _private_key_io_job->execute();
			bool const completed = _private_key_io_job->completed();
			if (completed) {
				_close_handle(&_private_key_handle);
				_private_key_io_job_buffer.size = _private_key_io_job->current_offset();
				_private_key_io_job.destruct();
			}

			return progress && completed;
		}

		bool _read_jitterentropy_file_finished()
		{
			if (!_jitterentropy_io_job.constructed()) {
				return true;
			}

			// XXX trigger sync

			bool const progress  = _jitterentropy_io_job->execute();
			bool const completed = _jitterentropy_io_job->completed();
			if (completed) {
				_close_handle(&_jitterentropy_handle);
				_jitterentropy_io_job_buffer.size = _jitterentropy_io_job->current_offset();
				_jitterentropy_io_job.destruct();
			}

			return progress && completed;
		}

		bool _read_key_file_finished()
		{
			if (!_key_io_job.constructed()) {
				return true;
			}

			// XXX trigger sync

			bool const progress  = _key_io_job->execute();
			bool const completed = _key_io_job->completed();
			if (completed) {
				_state = State::INITIALIZED;
				_close_handle(&_key_handle);
				_key_io_job_buffer.size = _key_io_job->current_offset();
				_key_io_job.destruct();
			}

			return progress && completed;
		}

		bool _open_key_file_and_write(Path const &path)
		{
			using Result = Vfs::Directory_service::Open_result;

			Path file_path = path;
			file_path.append_element(key_file_name.string());

			unsigned const mode =
				Vfs::Directory_service::OPEN_MODE_WRONLY | Vfs::Directory_service::OPEN_MODE_CREATE;

			Result const res =
				_vfs_env.root_dir().open(file_path.string(), mode,
				                         (Vfs::Vfs_handle **)&_key_handle,
				                         _vfs_env.alloc());
			if (res != Result::OPEN_OK) {
				return false;
			}

			_key_io_job.construct(*_key_handle, Util::Io_job::Operation::WRITE,
			                      _key_io_job_buffer, 0);
			if (_key_io_job->execute() && _key_io_job->completed()) {
				_start_sync_at_key_io_job();
			}
			return true;
		}


		/* hash */

		Vfs::Vfs_handle *_hash_handle { nullptr };

		Genode::Constructible<Util::Io_job> _hash_io_job { };

		struct Hash_io_job_buffer : Util::Io_job::Buffer
		{
			char buffer[64] { };

			Hash_io_job_buffer()
			{
				Buffer::base = buffer;
				Buffer::size = sizeof (buffer);
			}
		};

		Hash_io_job_buffer _hash_io_job_buffer { };

		bool _open_hash_file_and_queue_read(Path const &path)
		{
			using Result = Vfs::Directory_service::Open_result;

			Path file_path = path;
			file_path.append_element(hash_file_name.string());

			Result const res =
				_vfs_env.root_dir().open(file_path.string(),
				                         Vfs::Directory_service::OPEN_MODE_RDONLY,
				                         (Vfs::Vfs_handle **)&_hash_handle,
				                         _vfs_env.alloc());
			if (res != Result::OPEN_OK) {
				return false;
			}

			_hash_io_job.construct(*_hash_handle, Util::Io_job::Operation::READ,
			                       _hash_io_job_buffer, 0,
			                       Util::Io_job::Partial_result::ALLOW);
			if (_hash_io_job->execute() && _hash_io_job->completed()) {
				_close_handle(&_hash_handle);
				_hash_io_job.destruct();
				return true;
			}
			return true;
		}

		bool _read_hash_file_finished()
		{
			if (!_hash_io_job.constructed()) {
				return true;
			}
			bool const progress  = _hash_io_job->execute();
			bool const completed = _hash_io_job->completed();
			if (completed) {
				_close_handle(&_hash_handle);
				_hash_io_job.destruct();
			}

			return progress && completed;
		}

		void _start_sync_at_hash_io_job()
		{
			_hash_io_job.construct(*_hash_handle, Util::Io_job::Operation::SYNC,
			                       _hash_io_job_buffer, 0);
		}

		void _start_sync_at_key_io_job()
		{
			_key_io_job.construct(*_key_handle, Util::Io_job::Operation::SYNC,
			                      _key_io_job_buffer, 0);
		}

		bool _open_hash_file_and_write(Path const &path)
		{
			using Result = Vfs::Directory_service::Open_result;

			Path file_path = path;
			file_path.append_element(hash_file_name.string());

			using Stat_result = Vfs::Directory_service::Stat_result;

			Vfs::Directory_service::Stat out_stat { };
			Stat_result const stat_res =
				_vfs_env.root_dir().stat(file_path.string(), out_stat);

			bool const file_exists = stat_res == Stat_result::STAT_OK;

			unsigned const mode =
				Vfs::Directory_service::OPEN_MODE_WRONLY |
				(file_exists ? 0 : Vfs::Directory_service::OPEN_MODE_CREATE);

			Result const res =
				_vfs_env.root_dir().open(file_path.string(), mode,
				                         (Vfs::Vfs_handle **)&_hash_handle,
				                         _vfs_env.alloc());
			if (res != Result::OPEN_OK) {
				Genode::error("could not open '", file_path.string(), "'");
				return false;
			}

			_hash_io_job.construct(*_hash_handle, Util::Io_job::Operation::WRITE,
			                      _hash_io_job_buffer, 0);

			if (_hash_io_job->execute() && _hash_io_job->completed()) {
				_start_sync_at_hash_io_job();
			}
			return true;
		}

		bool _write_op_on_hash_file_is_in_final_sync_step()
		{
			if (_hash_io_job->op() == Util::Io_job::Operation::SYNC) {
				return true;
			}
			bool const progress  = _hash_io_job->execute();
			bool const completed = _hash_io_job->completed();
			if (completed) {
				_start_sync_at_hash_io_job();
			}
			return progress && completed;
		}

		bool _final_sync_of_write_op_on_hash_file_finished()
		{
			if (!_hash_io_job.constructed()) {
				return true;
			}
			bool const progress  = _hash_io_job->execute();
			bool const completed = _hash_io_job->completed();
			if (completed) {
				_close_handle(&_hash_handle);
				_hash_io_job.destruct();
			}
			return progress && completed;
		}

		bool _write_op_on_key_file_is_in_final_sync_step()
		{
			if (_key_io_job->op() == Util::Io_job::Operation::SYNC) {
				return true;
			}
			bool const progress  = _key_io_job->execute();
			bool const completed = _key_io_job->completed();
			if (completed) {
				_start_sync_at_key_io_job();
			}
			return progress && completed;
		}

		bool _final_sync_of_write_op_on_key_file_finished()
		{
			if (!_key_io_job.constructed()) {
				return true;
			}
			bool const progress  = _key_io_job->execute();
			bool const completed = _key_io_job->completed();
			if (completed) {
				_state = State::INITIALIZED;
				_close_handle(&_key_handle);
				_key_io_job.destruct();
			}
			return progress && completed;
		}

		Path const _base_path;

	public:

		Trust_anchor(Vfs::Env &vfs_env, Path const &path)
		:
			_vfs_env   { vfs_env },
			_base_path { path }
		{
			if (_check_key_file(_base_path)) {

				if (_open_key_file_and_queue_read(_base_path)) {

					while (!_read_key_file_finished()) {
						_vfs_env.io().commit_and_wait();
					}
				}
			}
			else {
				if (VERBOSE) {
					Genode::log("No key file found, TA not initialized");
				}
			}
		}

		bool initialized() const
		{
			return _state == State:: INITIALIZED;
		}

		bool execute()
		{
			bool result = false;

			while (_execute_one_step() == true)
				result = true;

			return result;
		}

		bool queue_initialize(Const_byte_range_ptr const &src)
		{
			if (_job != Job::NONE) {
				return false;
			}

			if (_state != State::UNINITIALIZED) {
				return false;
			}
			SHA256((unsigned char const *)src.start, src.num_bytes,
			       (unsigned char *)_passphrase_hash_buffer.base);

			_passphrase_hash_buffer.size = SHA256_DIGEST_LENGTH;

			_job       = Job::INIT;
			_job_state = Job_state::INIT_READ_JITTERENTROPY_PENDING;
			return true;
		}

		Complete_request complete_queue_initialize()
		{
			if (_job != Job::INIT || _job_state != Job_state::COMPLETE) {
				return { false, false };
			}

			_lock_state = Lock_state::UNLOCKED;

			_job       = Job::NONE;
			_job_state = Job_state::NONE;
			return { true, _job_success };
		}

		bool queue_unlock(Const_byte_range_ptr const &src)
		{
			if (_job != Job::NONE) {
				return false;
			}

			if (_state != State::INITIALIZED) {
				return false;
			}

			if (_lock_state == Lock_state::UNLOCKED) {
				_job         = Job::UNLOCK;
				_job_state   = Job_state::COMPLETE;
				_job_success = true;
				return true;
			}

			SHA256((unsigned char const *)src.start, src.num_bytes,
			       (unsigned char *)_passphrase_hash_buffer.base);

			_passphrase_hash_buffer.size = SHA256_DIGEST_LENGTH;

			_job       = Job::UNLOCK;
			_job_state = Job_state::PENDING;
			return true;
		}

		Complete_request complete_queue_unlock()
		{
			if (_job != Job::UNLOCK || _job_state != Job_state::COMPLETE) {
				return { false, false };
			}

			_lock_state = Lock_state::UNLOCKED;

			_job       = Job::NONE;
			_job_state = Job_state::NONE;
			return { true, _job_success };
		}

		bool queue_read_last_hash()
		{
			if (_job != Job::NONE) {
				return false;
			}

			if (_lock_state != Lock_state::UNLOCKED) {
				return false;
			}

			_job       = Job::READ_HASH;
			_job_state = Job_state::PENDING;
			return true;
		}

		Complete_request complete_read_last_hash(Vfs::Byte_range_ptr const &dst)
		{
			if (_job != Job::READ_HASH || _job_state != Job_state::COMPLETE) {
				return { false, false };
			}

			if (dst.num_bytes < _last_hash.length) {
				Genode::warning("truncate hash");
			}

			Genode::memcpy(dst.start, _last_hash.value, dst.num_bytes);

			_job       = Job::NONE;
			_job_state = Job_state::NONE;
			return { true, _job_success };
		}

		bool queue_update_last_hash(Vfs::Const_byte_range_ptr const &src)
		{
			if (_job != Job::NONE) {
				return false;
			}

			if (_lock_state != Lock_state::UNLOCKED) {
				return false;
			}

			if (src.num_bytes != _last_hash.length) {
				return false;
			}

			size_t const len = Genode::min(src.num_bytes, _hash_io_job_buffer.size);

			_hash_io_job_buffer.size = len;

			Genode::memcpy(_hash_io_job_buffer.buffer, src.start, len);

			Genode::memcpy(_last_hash.value, src.start, len);

			_job       = Job::UPDATE_HASH;
			_job_state = Job_state::PENDING;
			return true;
		}

		Complete_request complete_update_last_hash()
		{
			if (_job != Job::UPDATE_HASH || _job_state != Job_state::COMPLETE) {
				return { false, false };
			}

			_job       = Job::NONE;
			_job_state = Job_state::NONE;
			return { true, _job_success };
		}

		bool queue_encrypt_key(Const_byte_range_ptr const &src)
		{
			if (_job != Job::NONE) {
				return false;
			}

			if (_lock_state != Lock_state::UNLOCKED) {
				return false;
			}

			if (src.num_bytes != _encrypt_key.length) {
				Genode::error(__func__, ": key length mismatch, expected: ",
				              _encrypt_key.length, " got: ", src.num_bytes);
				return false;
			}

			Genode::memcpy(_encrypt_key.value, src.start, src.num_bytes);

			_job       = Job::ENCRYPT;
			_job_state = Job_state::PENDING;
			return true;
		}

		Complete_request complete_encrypt_key(Byte_range_ptr const &dst)
		{
			if (_job != Job::ENCRYPT || _job_state != Job_state::COMPLETE) {
				return { false, false };
			}

			if (dst.num_bytes != _encrypt_key.length) {
				Genode::error(__func__, ": key length mismatch, expected: ",
				              _encrypt_key.length, " got: ", dst.num_bytes);
				return { true, false };
			}

			Genode::memcpy(dst.start, _encrypt_key.value, _encrypt_key.length);

			_job       = Job::NONE;
			_job_state = Job_state::NONE;
			return { true, _job_success };
		}

		bool queue_decrypt_key(Const_byte_range_ptr const &src)
		{
			if (_job != Job::NONE) {
				return false;
			}

			if (_lock_state != Lock_state::UNLOCKED) {
				return false;
			}

			if (src.num_bytes != _decrypt_key.length) {
				Genode::error(__func__, ": key length mismatch, expected: ",
				              _decrypt_key.length, " got: ", src.num_bytes);
				return false;
			}

			Genode::memcpy(_decrypt_key.value, src.start, src.num_bytes);

			_job       = Job::DECRYPT;
			_job_state = Job_state::PENDING;
			return true;
		}

		Complete_request complete_decrypt_key(Byte_range_ptr const &dst)
		{
			if (_job != Job::DECRYPT || _job_state != Job_state::COMPLETE) {
				return { false, false };
			}

			if (dst.num_bytes != _decrypt_key.length) {
				Genode::error(__func__, ": key length mismatch, expected: ",
				              _decrypt_key.length, " got: ", dst.num_bytes);
				return { true, false };
			}

			Genode::memcpy(dst.start, _decrypt_key.value, _decrypt_key.length);

			_job       = Job::NONE;
			_job_state = Job_state::NONE;
			return { true, _job_success };
		}

		bool queue_generate_key()
		{
			if (_job_state != Job_state::NONE) {
				return false;
			}

			_job       = Job::GENERATE;
			_job_state = Job_state::PENDING;
			return true;
		}

		Complete_request complete_generate_key(Vfs::Byte_range_ptr const &dst)
		{
			if (_job != Job::GENERATE || _job_state != Job_state::COMPLETE) {
				return { false, false };
			}

			size_t len = dst.num_bytes;

			if (len < _generated_key.length) {
				Genode::warning("truncate generated key");
			} else {
				len = Genode::min(len, _generated_key.length);
			}

			Genode::memcpy(dst.start, _generated_key.value, len);
			Genode::memset(_generated_key.value, 0, sizeof (_generated_key.value));

			_job       = Job::NONE;
			_job_state = Job_state::NONE;
			return { true, _job_success };
		}
};


class Vfs_tresor_trust_anchor::Hashsum_file_system : public Vfs::Single_file_system
{
	private:

		Trust_anchor &_trust_anchor;

		struct Hashsum_handle : Single_vfs_handle
		{
			Trust_anchor &_trust_anchor;

			enum class State { NONE, PENDING_WRITE_ACK, PENDING_READ };
			State _state { State::NONE };

			Hashsum_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Trust_anchor      &ta)
			:
				Single_vfs_handle { ds, fs, alloc, 0 },
				_trust_anchor     { ta }
			{ }

			Read_result read(Byte_range_ptr const &src, size_t &out_count) override
			{
				_trust_anchor.execute();

				if (_state == State::NONE) {
					try {
						bool const ok =
							_trust_anchor.queue_read_last_hash();
						if (!ok) {
							return READ_ERR_IO;
						}
						_state = State::PENDING_READ;
					} catch (...) {
						return READ_ERR_INVALID;
					}

					_trust_anchor.execute();
					return READ_QUEUED;
				} else

				if (_state == State::PENDING_READ) {
					try {
						Trust_anchor::Complete_request const cr =
							_trust_anchor.complete_read_last_hash(src);
						if (!cr.valid) {
							_trust_anchor.execute();
							return READ_QUEUED;
						}

						_state = State::NONE;
						out_count = src.num_bytes;
						return cr.success ? READ_OK : READ_ERR_IO;
					} catch (...) {
						return READ_ERR_INVALID;
					}
				} else

				if (_state == State::PENDING_WRITE_ACK) {
					try {
						Trust_anchor::Complete_request const cr =
							_trust_anchor.complete_update_last_hash();
						if (!cr.valid) {
							_trust_anchor.execute();
							return READ_QUEUED;
						}

						_state = State::NONE;
						out_count = src.num_bytes;
						return cr.success ? READ_OK : READ_ERR_IO;
					} catch (...) {
						return READ_ERR_INVALID;
					}
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				_trust_anchor.execute();

				if (_state != State::NONE) {
					return WRITE_ERR_IO;
				}

				try {
					bool const ok = _trust_anchor.queue_update_last_hash(src);
					if (!ok) {
						return WRITE_ERR_IO;
					}
					_state = State::PENDING_WRITE_ACK;
				} catch (...) {
					return WRITE_ERR_INVALID;
				}

				_trust_anchor.execute();
				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Hashsum_file_system(Trust_anchor &ta)
		:
			Single_file_system { Node_type::TRANSACTIONAL_FILE, type_name(),
			                     Node_rwx::ro(), Xml_node("<hashsum/>") },
			_trust_anchor      { ta }
		{ }

		static char const *type_name() { return "hashsum"; }

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
					new (alloc) Hashsum_handle(*this, *this, alloc,
					                           _trust_anchor);
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_tresor_trust_anchor::Generate_key_file_system : public Vfs::Single_file_system
{
	private:

		Trust_anchor &_trust_anchor;

		struct Gen_key_handle : Single_vfs_handle
		{
			Trust_anchor &_trust_anchor;

			enum class State { NONE, PENDING };
			State _state { State::NONE, };

			Gen_key_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Trust_anchor      &ta)
			:
				Single_vfs_handle { ds, fs, alloc, 0 },
				_trust_anchor     { ta }
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				if (_state == State::NONE) {

					if (!_trust_anchor.queue_generate_key()) {
						return READ_QUEUED;
					}
					_state = State::PENDING;
				}

				(void)_trust_anchor.execute();

				Trust_anchor::Complete_request const cr =
					_trust_anchor.complete_generate_key(dst);
				if (!cr.valid) {
					return READ_QUEUED;
				}

				_state = State::NONE;
				out_count = dst.num_bytes;
				return cr.success ? READ_OK : READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override
			{
				return WRITE_ERR_IO;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return false; }
		};

	public:

		Generate_key_file_system(Trust_anchor &ta)
		:
			Single_file_system { Node_type::TRANSACTIONAL_FILE, type_name(),
			                     Node_rwx::ro(), Xml_node("<generate_key/>") },
			_trust_anchor      { ta }
		{ }

		static char const *type_name() { return "generate_key"; }

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
					new (alloc) Gen_key_handle(*this, *this, alloc,
					                           _trust_anchor);
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_tresor_trust_anchor::Encrypt_file_system : public Vfs::Single_file_system
{
	private:

		Trust_anchor &_trust_anchor;

		struct Encrypt_handle : Single_vfs_handle
		{
			Trust_anchor &_trust_anchor;

			enum State { NONE, PENDING };
			State _state;

			Encrypt_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Trust_anchor      &ta)
			:
				Single_vfs_handle { ds, fs, alloc, 0 },
				_trust_anchor { ta },
				_state        { State::NONE }
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				if (_state != State::PENDING) {
					return READ_ERR_IO;
				}

				_trust_anchor.execute();

				try {
					Trust_anchor::Complete_request const cr =
						_trust_anchor.complete_encrypt_key(dst);
					if (!cr.valid) {
						return READ_QUEUED;
					}

					_state = State::NONE;

					out_count = dst.num_bytes;
					return cr.success ? READ_OK : READ_ERR_IO;
				} catch (...) {
					return READ_ERR_INVALID;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				if (_state != State::NONE) {
					return WRITE_ERR_IO;
				}

				try {
					bool const ok =
						_trust_anchor.queue_encrypt_key(src);
					if (!ok) {
						return WRITE_ERR_IO;
					}
					_state = State::PENDING;
				} catch (...) {
					return WRITE_ERR_INVALID;
				}

				_trust_anchor.execute();
				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Encrypt_file_system(Trust_anchor &ta)
		:
			Single_file_system { Node_type::TRANSACTIONAL_FILE, type_name(),
			                     Node_rwx::rw(), Xml_node("<encrypt/>") },
			_trust_anchor { ta }
		{ }

		static char const *type_name() { return "encrypt"; }

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
					new (alloc) Encrypt_handle(*this, *this, alloc,
					                           _trust_anchor);
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_tresor_trust_anchor::Decrypt_file_system : public Vfs::Single_file_system
{
	private:

		Trust_anchor &_trust_anchor;

		struct Decrypt_handle : Single_vfs_handle
		{
			Trust_anchor &_trust_anchor;

			enum State { NONE, PENDING };
			State _state;

			Decrypt_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Trust_anchor      &ta)
			:
				Single_vfs_handle { ds, fs, alloc, 0 },
				_trust_anchor { ta },
				_state        { State::NONE }
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				if (_state != State::PENDING) {
					return READ_ERR_IO;
				}

				_trust_anchor.execute();

				try {
					Trust_anchor::Complete_request const cr =
						_trust_anchor.complete_decrypt_key(dst);
					if (!cr.valid) {
						return READ_QUEUED;
					}

					_state = State::NONE;

					out_count = dst.num_bytes;
					return cr.success ? READ_OK : READ_ERR_IO;
				} catch (...) {
					return READ_ERR_INVALID;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				if (_state != State::NONE) {
					return WRITE_ERR_IO;
				}

				try {
					bool const ok =
						_trust_anchor.queue_decrypt_key(src);
					if (!ok) {
						return WRITE_ERR_IO;
					}
					_state = State::PENDING;
				} catch (...) {
					return WRITE_ERR_INVALID;
				}

				_trust_anchor.execute();
				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Decrypt_file_system(Trust_anchor &ta)
		:
			Single_file_system { Node_type::TRANSACTIONAL_FILE, type_name(),
			                     Node_rwx::rw(), Xml_node("<decrypt/>") },
			_trust_anchor { ta }
		{ }

		static char const *type_name() { return "decrypt"; }

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
					new (alloc) Decrypt_handle(*this, *this, alloc,
					                           _trust_anchor);
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_tresor_trust_anchor::Initialize_file_system : public Vfs::Single_file_system
{
	private:

		Trust_anchor &_trust_anchor;

		struct Initialize_handle : Single_vfs_handle
		{
			Trust_anchor &_trust_anchor;

			enum class State { NONE, PENDING };
			State _state { State::NONE };

			bool _init_pending { false };

			Initialize_handle(Directory_service &ds,
			                  File_io_service   &fs,
			                  Genode::Allocator &alloc,
			                  Trust_anchor      &ta)
			:
				Single_vfs_handle { ds, fs, alloc, 0 },
				_trust_anchor     { ta }
			{ }

			Read_result read(Byte_range_ptr const &buf,
			                 size_t               &nr_of_read_bytes) override
			{
				if (_state != State::PENDING) {
					return READ_ERR_INVALID;
				}

				(void)_trust_anchor.execute();

				Trust_anchor::Complete_request const cr =
					_init_pending ? _trust_anchor.complete_queue_unlock()
					              : _trust_anchor.complete_queue_initialize();
				if (!cr.valid) {
					return READ_QUEUED;
				}

				_state        = State::NONE;
				_init_pending = false;

				if (cr.success) {

					char const *str { "ok" };
					if (buf.num_bytes < 3) {
						Genode::error("read buffer too small");
						return READ_ERR_IO;
					}
					memcpy(buf.start, str, 3);
					nr_of_read_bytes = buf.num_bytes;

				} else {

					char const *str { "failed" };
					if (buf.num_bytes < 7) {
						Genode::error("read buffer too small");
						return READ_ERR_IO;
					}
					memcpy(buf.start, str, 7);
					nr_of_read_bytes = buf.num_bytes;
				}
				return READ_OK;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				if (_state != State::NONE) {
					return WRITE_ERR_INVALID;
				}

				_init_pending = _trust_anchor.initialized();

				bool const res = _init_pending ? _trust_anchor.queue_unlock(src)
				                               : _trust_anchor.queue_initialize(src);

				if (!res) {
					return WRITE_ERR_IO;
				}
				_state = State::PENDING;

				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Initialize_file_system(Trust_anchor &ta)
		:
			Single_file_system { Node_type::TRANSACTIONAL_FILE, type_name(),
			                     Node_rwx::rw(), Xml_node("<initialize/>") },
			_trust_anchor      { ta }
		{ }

		static char const *type_name() { return "initialize"; }

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
					new (alloc) Initialize_handle(*this, *this, alloc,
					                              _trust_anchor);
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs_tresor_trust_anchor::Local_factory : File_system_factory
{
	Trust_anchor _trust_anchor;

	Decrypt_file_system      _decrypt_fs;
	Encrypt_file_system      _encrypt_fs;
	Generate_key_file_system _gen_key_fs;
	Hashsum_file_system      _hash_fs;
	Initialize_file_system   _init_fs;

	using Storage_path = String<256>;
	static Storage_path _storage_path(Xml_node const &node)
	{
		if (!node.has_attribute("storage_dir")) {
			error("mandatory 'storage_dir' attribute missing");
			struct Missing_storage_dir_attribute { };
			throw Missing_storage_dir_attribute();
		}
		return node.attribute_value("storage_dir", Storage_path());
	}

	Local_factory(Vfs::Env &vfs_env, Xml_node config)
	:
		_trust_anchor { vfs_env, _storage_path(config).string() },
		_decrypt_fs   { _trust_anchor },
		_encrypt_fs   { _trust_anchor },
		_gen_key_fs   { _trust_anchor },
		_hash_fs      { _trust_anchor },
		_init_fs      { _trust_anchor }
	{ }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Decrypt_file_system::type_name())) {
			return &_decrypt_fs;
		}

		if (node.has_type(Encrypt_file_system::type_name())) {
			return &_encrypt_fs;
		}

		if (node.has_type(Generate_key_file_system::type_name())) {
			return &_gen_key_fs;
		}

		if (node.has_type(Hashsum_file_system::type_name())) {
			return &_hash_fs;
		}

		if (node.has_type(Initialize_file_system::type_name())) {
			return &_init_fs;
		}

		return nullptr;
	}
};


class Vfs_tresor_trust_anchor::File_system : private Local_factory,
                                          public Vfs::Dir_file_system
{
	private:

		using Config = String<128>;

		static Config _config(Xml_node const &node)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof (buf), "dir", [&] () {
				xml.attribute("name", node.attribute_value("name", String<32>("")));

				xml.node("decrypt",      [&] () { });
				xml.node("encrypt",      [&] () { });
				xml.node("generate_key", [&] () { });
				xml.node("hashsum",      [&] () { });
				xml.node("initialize",   [&] () { });
			});

			return Config(Cstring(buf));
		}

	public:

		File_system(Vfs::Env &vfs_env, Genode::Xml_node node)
		:
			Local_factory        { vfs_env, node },
			Vfs::Dir_file_system { vfs_env, Xml_node(_config(node).string()), *this }
		{ }

		~File_system() { }
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
				return new (vfs_env.alloc())
					Vfs_tresor_trust_anchor::File_system(vfs_env, node);

			} catch (...) {
				Genode::error("could not create 'tresor_trust_anchor'");
			}
			return nullptr;
		}
	};

	static Factory factory;
	return &factory;
}
