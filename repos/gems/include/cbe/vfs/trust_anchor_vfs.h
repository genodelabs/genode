/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
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

#ifndef _CBE__VFS__TRUST_ANCHOR_VFS_H_
#define _CBE__VFS__TRUST_ANCHOR_VFS_H_

/* local includes */
#include <cbe/vfs/io_job.h>

namespace Util {

	using namespace Genode;

	struct Trust_anchor_vfs;

}; /* namespace Util */


struct Util::Trust_anchor_vfs
{
	struct Invalid_request : Genode::Exception { };

	using Path = Genode::Path<256>;

	Vfs::File_system &_vfs;

	struct Io_response_handler : Vfs::Io_response_handler
	{
		Genode::Signal_context_capability _io_sigh;

		Io_response_handler(Genode::Signal_context_capability io_sigh)
		: _io_sigh(io_sigh) { }

		void read_ready_response() override { }

		void io_progress_response() override
		{
			if (_io_sigh.valid()) {
				Genode::Signal_transmitter(_io_sigh).submit();
			}
		}
	};

	Io_response_handler _io_response_handler;

	struct File
	{
		struct Could_not_open_file              : Genode::Exception { };
		struct Io_job_already_constructed       : Genode::Exception { };
		struct Cannot_drop_unconstructed_io_job : Genode::Exception { };

		struct Completed_io_job
		{
			bool completed;
			bool success;
		};

		File(File const &) = delete;
		File &operator=(File const&) = delete;

		Vfs::File_system &_vfs;
		Vfs::Vfs_handle  *_vfs_handle;

		Genode::Constructible<Util::Io_job> _io_job { };

		File(Path          const &base_path,
		     char          const *name,
		     Vfs::File_system    &vfs,
		     Genode::Allocator   &alloc,
		     Io_response_handler &io_response_handler)
		:
			_vfs        { vfs },
			_vfs_handle { nullptr }
		{
			using Result = Vfs::Directory_service::Open_result;

			Path file_path = base_path;
			file_path.append_element(name);

			Result const res =
				_vfs.open(file_path.string(),
				          Vfs::Directory_service::OPEN_MODE_RDWR,
				          (Vfs::Vfs_handle **)&_vfs_handle, alloc);
			if (res != Result::OPEN_OK) {
				error("could not open '", file_path.string(), "'");
				throw Could_not_open_file();
			}

			_vfs_handle->handler(&io_response_handler);
		}

		~File()
		{
			_vfs.close(_vfs_handle);
		}

		bool submit_io_job(Util::Io_job::Buffer &buffer,
		                   Util::Io_job::Operation op)
		{
			if (_io_job.constructed()) {
				// throw Io_job_already_constructed();
				return false;
			}

			_io_job.construct(*_vfs_handle, op, buffer, 0);
			return true;
		}

		bool execute_io_job()
		{
			if (!_io_job.constructed()) {
				return false;
			}
			return _io_job->execute();
		}

		void drop_io_job()
		{
			if (!_io_job.constructed()) {
				throw Cannot_drop_unconstructed_io_job();
			}
			_io_job.destruct();
		}

		Completed_io_job completed_io_job()
		{
			if (!_io_job.constructed()) {
				return { false, false };
			}

			return { _io_job->completed(), _io_job->succeeded() };
		}
	};

	Util::Io_job::Buffer        _init_io_buffer { };
	Genode::Constructible<File> _init_file { };

	Util::Io_job::Buffer        _encrypt_io_buffer { };
	Genode::Constructible<File> _encrypt_file { };

	Util::Io_job::Buffer        _decrypt_io_buffer { };
	Genode::Constructible<File> _decrypt_file { };

	Util::Io_job::Buffer        _generate_key_io_buffer { };
	Genode::Constructible<File> _generate_key_file { };

	Util::Io_job::Buffer        _last_hash_io_buffer { };
	Genode::Constructible<File> _last_hash_file { };

	Path _ta_dir { };

	struct Job
	{
		enum class Type {
			NONE,
			DECRYPT_WRITE,
			DECRYPT_READ,
			ENCRYPT_WRITE,
			ENCRYPT_READ,
			GENERATE,
			INIT_WRITE,
			INIT_READ,
			HASH_READ,
			HASH_UPDATE_WRITE,
			HASH_UPDATE_READ,
		};
		Type type { Type::NONE };

		static char const *to_string(Type const type)
		{
			switch (type) {
			case Type::NONE:              return "NONE";
			case Type::DECRYPT_WRITE:     return "DECRYPT_WRITE";
			case Type::DECRYPT_READ:      return "DECRYPT_READ";
			case Type::ENCRYPT_WRITE:     return "ENCRYPT_WRITE";
			case Type::ENCRYPT_READ:      return "ENCRYPT_READ";
			case Type::GENERATE:          return "GENERATE";
			case Type::INIT_WRITE:        return "INIT_WRITE";
			case Type::INIT_READ:         return "INIT_READ";
			case Type::HASH_READ:         return "HASH_READ";
			case Type::HASH_UPDATE_WRITE: return "HASH_UPDATE_WRITE";
			case Type::HASH_UPDATE_READ:  return "HASH_UPDATE_READ";
			}
			return nullptr;
		}

		enum class State {
			NONE,
			PENDING,
			IN_PROGRESS,
			COMPLETE
		};
		State state { State::NONE };

		static char const *to_string(State const state)
		{
			switch (state) {
			case State::NONE:        return "NONE";
			case State::PENDING:     return "PENDING";
			case State::IN_PROGRESS: return "IN_PROGRESS";
			case State::COMPLETE:    return "COMPLETE";
			}
			return nullptr;
		}

		Cbe::Hash                 hash;
		Cbe::Key_plaintext_value  plain;
		Cbe::Key_ciphertext_value cipher;

		Cbe::Trust_anchor_request request { };
		bool success { false };

		void reset()
		{
			type    = Type::NONE;
			state   = State::NONE;
			request = Cbe::Trust_anchor_request();
		}

		bool valid() const { return state != State::NONE; }

		bool completed() const { return state == State::COMPLETE; }

		bool equals(Cbe::Trust_anchor_request const &other) const
		{
			return request.operation() == other.operation()
			    && request.tag()       == other.tag();
		}

		void print(Genode::Output &out) const
		{
			if (!valid()) {
				Genode::print(out, "<invalid>");
				return;
			}

			Genode::print(out, "type: ", to_string(type));
			Genode::print(out, " state: ", to_string(state));
			Genode::print(out, " request: ", request);
		}
	};

	Job _job { };

	bool _execute_decrypt(Job &job, bool write)
	{
		bool progress = false;
		File::Completed_io_job completed_io_job { false, false };

		switch (job.state) {
		case Job::State::PENDING:
		{
			using Op = Util::Io_job::Operation;

			Op const op = write ? Op::WRITE : Op::READ;
			if (!_decrypt_file->submit_io_job(_decrypt_io_buffer, op)) {
				break;
			}
			job.state = Job::State::IN_PROGRESS;
			progress |= true;
		}
		[[fallthrough]];
		case Job::State::IN_PROGRESS:
			if (!_decrypt_file->execute_io_job()) {
				break;
			}

			progress |= true;

			completed_io_job = _decrypt_file->completed_io_job();
			if (!completed_io_job.completed) {
				break;
			}
			_decrypt_file->drop_io_job();

			/* setup second phase */
			if (write) {

				/*
				 * In case the write request was not successful it
				 * is not needed to try to read the result.
				 */
				if (!completed_io_job.success) {
					job.state = Job::State::COMPLETE;
					job.request.success(false);
					break;
				}

				_decrypt_io_buffer = {
					.base = _job.plain.value,
					.size = sizeof (_job.plain)
				};

				job.type  = Job::Type::DECRYPT_READ;
				job.state = Job::State::PENDING;
				break;
			}

			job.state   = Job::State::COMPLETE;
			job.success = completed_io_job.success;
			job.request.success(job.success);
		[[fallthrough]];
		case Job::State::COMPLETE: break;
		case Job::State::NONE:     break;
		}
		return progress;
	}

	bool _execute_encrypt(Job &job, bool write)
	{
		bool progress = false;
		File::Completed_io_job completed_io_job { false, false };

		switch (job.state) {
		case Job::State::PENDING:
		{
			using Op = Util::Io_job::Operation;

			Op const op = write ? Op::WRITE : Op::READ;
			if (!_encrypt_file->submit_io_job(_encrypt_io_buffer, op)) {
				break;
			}
			job.state = Job::State::IN_PROGRESS;
			progress |= true;
		}
		[[fallthrough]];
		case Job::State::IN_PROGRESS:
			if (!_encrypt_file->execute_io_job()) {
				break;
			}

			progress |= true;

			completed_io_job = _encrypt_file->completed_io_job();
			if (!completed_io_job.completed) {
				break;
			}
			_encrypt_file->drop_io_job();

			/* setup second phase */
			if (write) {

				/*
				 * In case the write request was not successful it
				 * is not needed to try to read the result.
				 */
				if (!completed_io_job.success) {
					job.state = Job::State::COMPLETE;
					job.request.success(false);
					break;
				}

				_encrypt_io_buffer = {
					.base = _job.cipher.value,
					.size = sizeof (_job.cipher)
				};

				job.type  = Job::Type::ENCRYPT_READ;
				job.state = Job::State::PENDING;
				break;
			}

			job.state   = Job::State::COMPLETE;
			job.success = completed_io_job.success;
			job.request.success(job.success);
		[[fallthrough]];
		case Job::State::COMPLETE: break;
		case Job::State::NONE:     break;
		}
		return progress;
	}

	bool _execute_generate(Job &job)
	{
		bool progress = false;
		File::Completed_io_job completed_io_job { false, false };

		switch (job.state) {
		case Job::State::PENDING:
			if (!_generate_key_file->submit_io_job(_generate_key_io_buffer,
			                                       Util::Io_job::Operation::READ)) {
				break;
			}
			job.state = Job::State::IN_PROGRESS;
			progress |= true;
		[[fallthrough]];
		case Job::State::IN_PROGRESS:
			if (!_generate_key_file->execute_io_job()) {
				break;
			}

			progress |= true;

			completed_io_job = _generate_key_file->completed_io_job();
			if (!completed_io_job.completed) {
				break;
			}
			_generate_key_file->drop_io_job();

			job.state   = Job::State::COMPLETE;
			job.success = completed_io_job.success;
			job.request.success(job.success);
		[[fallthrough]];
		case Job::State::COMPLETE: break;
		case Job::State::NONE:     break;
		}
		return progress;
	}

	bool _execute_init(Job &job, bool write)
	{
		bool progress = false;
		File::Completed_io_job completed_io_job { false, false };

		switch (job.state) {
		case Job::State::PENDING:
		{
			using Op = Util::Io_job::Operation;

			Op const op = write ? Op::WRITE : Op::READ;
			if (!_init_file->submit_io_job(_init_io_buffer, op)) {
				break;
			}
			job.state = Job::State::IN_PROGRESS;
			progress |= true;
		}
		[[fallthrough]];
		case Job::State::IN_PROGRESS:
			if (!_init_file->execute_io_job()) {
				break;
			}

			progress |= true;

			completed_io_job = _init_file->completed_io_job();
			if (!completed_io_job.completed) {
				break;
			}
			_init_file->drop_io_job();

			/* setup second phase */
			if (write) {

				/*
				 * In case the write request was not successful it
				 * is not needed to try to read the result.
				 */
				if (!completed_io_job.success) {
					job.state = Job::State::COMPLETE;
					job.request.success(false);
					break;
				}

				_init_io_buffer = {
					.base = _job.cipher.value,
					.size = sizeof (_job.cipher)
				};

				job.type  = Job::Type::INIT_READ;
				job.state = Job::State::PENDING;
				break;
			}

			job.state   = Job::State::COMPLETE;
			job.success = completed_io_job.success;
			job.request.success(job.success);
		[[fallthrough]];
		case Job::State::COMPLETE: break;
		case Job::State::NONE:     break;
		}
		return progress;
	}

	bool _execute_read_hash(Job &job)
	{
		bool progress = false;
		File::Completed_io_job completed_io_job { false, false };

		switch (job.state) {
		case Job::State::PENDING:
			if (!_last_hash_file->submit_io_job(_last_hash_io_buffer,
			                                    Util::Io_job::Operation::READ)) {
				break;
			}
			job.state = Job::State::IN_PROGRESS;
			progress |= true;
		[[fallthrough]];
		case Job::State::IN_PROGRESS:
			if (!_last_hash_file->execute_io_job()) {
				break;
			}

			progress |= true;

			completed_io_job = _last_hash_file->completed_io_job();
			if (!completed_io_job.completed) {
				break;
			}
			_last_hash_file->drop_io_job();

			job.state   = Job::State::COMPLETE;
			job.success = completed_io_job.success;
			job.request.success(job.success);
		[[fallthrough]];
		case Job::State::COMPLETE: break;
		case Job::State::NONE:     break;
		}
		return progress;
	}

	bool _execute_update_hash(Job &job, bool write)
	{
		bool progress = false;
		File::Completed_io_job completed_io_job { false, false };

		switch (job.state) {
		case Job::State::PENDING:
		{
			using Op = Util::Io_job::Operation;

			Op const op = write ? Op::WRITE : Op::READ;
			if (!_last_hash_file->submit_io_job(_last_hash_io_buffer, op)) {
				break;
			}
			job.state = Job::State::IN_PROGRESS;
			progress |= true;
		}
		[[fallthrough]];
		case Job::State::IN_PROGRESS:
			if (!_last_hash_file->execute_io_job()) {
				break;
			}

			progress |= true;

			completed_io_job = _last_hash_file->completed_io_job();
			if (!completed_io_job.completed) {
				break;
			}
			_last_hash_file->drop_io_job();

			/* setup second phase */
			if (write) {

				/*
				 * In case the write request was not successful it
				 * is not needed to try to read the result.
				 */
				if (!completed_io_job.success) {
					job.state = Job::State::COMPLETE;
					job.request.success(false);
					break;
				}

				_last_hash_io_buffer = {
					.base = _job.hash.values,
					.size = sizeof (_job.hash)
				};

				job.type  = Job::Type::HASH_UPDATE_READ;
				job.state = Job::State::PENDING;
				break;
			}

			job.state   = Job::State::COMPLETE;
			job.success = completed_io_job.success;
			job.request.success(job.success);
		[[fallthrough]];
		case Job::State::COMPLETE: break;
		case Job::State::NONE:     break;
		}
		return progress;
	}

	Trust_anchor_vfs(Vfs::File_system  &vfs,
	                 Genode::Allocator &alloc,
	                 Path        const &path,
	                 Genode::Signal_context_capability io_sigh)
	:
		_vfs                 { vfs },
		_io_response_handler { io_sigh },
		_ta_dir              { path }
	{
		_init_file.construct(path, "initialize", _vfs, alloc, _io_response_handler);
		_encrypt_file.construct(path, "encrypt", _vfs, alloc, _io_response_handler);
		_decrypt_file.construct(path, "decrypt", _vfs, alloc, _io_response_handler);
		_generate_key_file.construct(path, "generate_key", _vfs, alloc, _io_response_handler);
		_last_hash_file.construct(path, "hashsum", _vfs, alloc, _io_response_handler);
	}

	bool request_acceptable() const
	{
		return !_job.valid();
	}

	void submit_create_key_request(Cbe::Trust_anchor_request const &request)
	{
		_job = {
			.type    = Job::Type::GENERATE,
			.state   = Job::State::PENDING,
			.hash    = Cbe::Hash(),
			.plain   = Cbe::Key_plaintext_value(),
			.cipher  = Cbe::Key_ciphertext_value(),
			.request = request,
			.success = false,
		};

		_generate_key_io_buffer = {
			.base = _job.plain.value,
			.size = sizeof (_job.plain)
		};
	}

	void submit_superblock_hash_request(Cbe::Trust_anchor_request const &request)
	{
		_job = {
			.type    = Job::Type::HASH_READ,
			.state   = Job::State::PENDING,
			.hash    = Cbe::Hash(),
			.plain   = Cbe::Key_plaintext_value(),
			.cipher  = Cbe::Key_ciphertext_value(),
			.request = request,
			.success = false,
		};

		_last_hash_io_buffer = {
			.base = _job.hash.values,
			.size = sizeof (_job.hash)
		};
	}

	void submit_secure_superblock_request(Cbe::Trust_anchor_request const &request,
	                                      Cbe::Hash const &hash)
	{
		_job = {
			.type    = Job::Type::HASH_UPDATE_WRITE,
			.state   = Job::State::PENDING,
			.hash    = hash,
			.plain   = Cbe::Key_plaintext_value(),
			.cipher  = Cbe::Key_ciphertext_value(),
			.request = request,
			.success = false,
		};

		_last_hash_io_buffer = {
			.base = _job.hash.values,
			.size = sizeof (_job.hash)
		};
	}

	void submit_encrypt_key_request(Cbe::Trust_anchor_request const &request,
	                                Cbe::Key_plaintext_value  const &plain)
	{
		_job = {
			.type    = Job::Type::ENCRYPT_WRITE,
			.state   = Job::State::PENDING,
			.hash    = Cbe::Hash(),
			.plain   = plain,
			.cipher  = Cbe::Key_ciphertext_value(),
			.request = request,
			.success = false,
		};

		_encrypt_io_buffer = {
			.base = _job.plain.value,
			.size = sizeof (_job.plain)
		};
	}

	void submit_decrypt_key_request(Cbe::Trust_anchor_request const &request,
	                                Cbe::Key_ciphertext_value const &cipher)
	{
		_job = {
			.type    = Job::Type::DECRYPT_WRITE,
			.state   = Job::State::PENDING,
			.hash    = Cbe::Hash(),
			.plain   = Cbe::Key_plaintext_value(),
			.cipher  = cipher,
			.request = request,
			.success = false,
		};

		_decrypt_io_buffer = {
			.base = _job.cipher.value,
			.size = sizeof (_job.cipher)
		};
	}

	Cbe::Trust_anchor_request peek_completed_request()
	{
		return _job.completed() ? _job.request : Cbe::Trust_anchor_request();
	}

	void drop_completed_request(Cbe::Trust_anchor_request const &request)
	{
		if (!_job.equals(request)) {
			throw Invalid_request();
		}

		_job.reset();
	}

	Cbe::Hash peek_completed_superblock_hash(Cbe::Trust_anchor_request const &request)
	{
		if (!_job.equals(request) || !_job.completed()) {
			throw Invalid_request();
		}

		return _job.hash;
	}

	Cbe::Key_plaintext_value peek_completed_key_value_plaintext(Cbe::Trust_anchor_request const &request)
	{
		if (!_job.equals(request) || !_job.completed()) {
			throw Invalid_request();
		}

		return _job.plain;
	}

	Cbe::Key_ciphertext_value peek_completed_key_value_ciphertext(Cbe::Trust_anchor_request const &request)
	{
		if (!_job.equals(request) || !_job.completed()) {
			throw Invalid_request();
		}
		return _job.cipher;
	}

	bool execute()
	{
		bool progress = false;

		switch (_job.type) {
		case Job::Type::NONE: break;
		case Job::Type::DECRYPT_WRITE: progress |= _execute_decrypt(_job, true);  break;
		case Job::Type::DECRYPT_READ:  progress |= _execute_decrypt(_job, false); break;
		case Job::Type::ENCRYPT_WRITE: progress |= _execute_encrypt(_job, true);  break;
		case Job::Type::ENCRYPT_READ:  progress |= _execute_encrypt(_job, false); break;
		case Job::Type::GENERATE:      progress |= _execute_generate(_job);    break;
		case Job::Type::INIT_WRITE:    progress |= _execute_init(_job, true);  break;
		case Job::Type::INIT_READ:     progress |= _execute_init(_job, false); break;
		case Job::Type::HASH_READ:     progress |= _execute_read_hash(_job);   break;
		case Job::Type::HASH_UPDATE_WRITE: progress |= _execute_update_hash(_job, true); break;
		case Job::Type::HASH_UPDATE_READ:  progress |= _execute_update_hash(_job, false); break;
		}
		return progress;
	}
};

#endif /* _CBE__VFS__TRUST_ANCHOR_VFS_H_ */
