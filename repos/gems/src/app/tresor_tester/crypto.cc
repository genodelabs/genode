/*
 * \brief  Implementation of the Crypto module API using the Crypto VFS API
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <crypto.h>

using namespace Genode;
using namespace Tresor;
using namespace Vfs;


Crypto::Key_directory &Crypto::_get_unused_key_dir()
{
	for (Key_directory &key_dir : _key_dirs) {
		if (key_dir.key_id == 0) {
			return key_dir;
		}
	}
	class Failed { };
	throw Failed { };
}


Crypto::Key_directory &Crypto::_lookup_key_dir(uint32_t key_id)
{
	for (Key_directory &key_dir : _key_dirs) {
		if (key_dir.key_id == key_id) {
			return key_dir;
		}
	}
	class Failed { };
	throw Failed { };
}


Crypto::Crypto(Vfs::Env &env, Xml_node const &crypto)
:
	_env               { env },
	_path              { crypto.attribute_value("path", String<32>()) },
	_add_key_handle    { vfs_open_wo(env, { _path.string(), "/add_key" }) },
	_remove_key_handle { vfs_open_wo(env, { _path.string(), "/remove_key" }) }
{ }


bool Crypto::request_acceptable() const
{
	return _job.op == Operation::INVALID;
}


Crypto::Result Crypto::add_key(Key const &key)
{
	char buffer[sizeof (key.value) + sizeof (key.id.value)] { };
	memcpy(buffer, &key.id.value, sizeof (key.id.value));
	memcpy(buffer + sizeof (key.id.value),
	       key.value, sizeof (key.value));

	_add_key_handle.seek(0);

	size_t written_bytes = 0;

	using Write_result = Vfs::File_io_service::Write_result;

	Const_byte_range_ptr const src(buffer, sizeof(buffer));

	Write_result const result =
		_add_key_handle.fs().write(&_add_key_handle, src, written_bytes);

	if (result == Write_result::WRITE_ERR_WOULD_BLOCK)
		return Result::RETRY_LATER;

	Key_directory &key_dir { _get_unused_key_dir() };

	key_dir.encrypt_handle = &vfs_open_rw(
		_env, { _path.string(), "/keys/", key.id.value, "/encrypt" });

	key_dir.decrypt_handle = &vfs_open_rw(
		_env, { _path.string(), "/keys/", key.id.value, "/decrypt" });

	key_dir.key_id = key.id.value;
	return Result::SUCCEEDED;
}


Crypto::Result Crypto::remove_key(Tresor::Key::Id key_id)
{
	size_t written_bytes = 0;
	_remove_key_handle.seek(0);

	Const_byte_range_ptr const src((char *)&key_id.value, sizeof(key_id.value));

	using Write_result = Vfs::File_io_service::Write_result;

	Write_result const result =
		_remove_key_handle.fs().write(&_remove_key_handle, src, written_bytes);

	if (result == Write_result::WRITE_ERR_WOULD_BLOCK)
		return Result::RETRY_LATER;

	Key_directory &key_dir { _lookup_key_dir(key_id.value) };
	_env.root_dir().close(key_dir.encrypt_handle);
	key_dir.encrypt_handle = nullptr;
	_env.root_dir().close(key_dir.decrypt_handle);
	key_dir.decrypt_handle = nullptr;
	key_dir.key_id = 0;
	return Result::SUCCEEDED;
}


void Crypto::submit_request(Tresor::Request          const &request,
                    Operation                    op,
                    Crypto_plain_buffer::Index   plain_buf_idx,
                    Crypto_cipher_buffer::Index  cipher_buf_idx)
{
	switch (op) {
	case Operation::ENCRYPT_BLOCK:

		_job.request        = request;
		_job.state          = Job_state::SUBMITTED;
		_job.op             = op;
		_job.cipher_buf_idx = cipher_buf_idx;
		_job.plain_buf_idx  = plain_buf_idx;
		_job.handle         =
			_lookup_key_dir(request.key_id()).encrypt_handle;

		break;

	case Operation::DECRYPT_BLOCK:

		_job.request        = request;
		_job.state          = Job_state::SUBMITTED;
		_job.op             = op;
		_job.cipher_buf_idx = cipher_buf_idx;
		_job.plain_buf_idx  = plain_buf_idx;
		_job.handle         =
			_lookup_key_dir(request.key_id()).decrypt_handle;

		break;

	case Operation::INVALID:

		class Bad_operation { };
		throw Bad_operation { };
	}
}


Tresor::Request Crypto::peek_completed_encryption_request() const
{
	if (_job.state != Job_state::COMPLETE ||
	    _job.op != Operation::ENCRYPT_BLOCK) {

		return Tresor::Request { };
	}
	return _job.request;
}


Tresor::Request Crypto::peek_completed_decryption_request() const
{
	if (_job.state != Job_state::COMPLETE ||
	    _job.op != Operation::DECRYPT_BLOCK) {

		return Tresor::Request { };
	}
	return _job.request;
}


void Crypto::drop_completed_request()
{
	if (_job.state != Job_state::COMPLETE) {

		class Bad_state { };
		throw Bad_state { };
	}
	_job.op = Operation::INVALID;
}


void Crypto::_execute_decrypt_block(Job                  &job,
                            Crypto_plain_buffer  &plain_buf,
                            Crypto_cipher_buffer &cipher_buf,
                            bool                 &progress)
{
	switch (job.state) {
	case Job_state::SUBMITTED:
	{
		job.handle->seek(job.request.block_number() * Tresor::BLOCK_SIZE);

		size_t written_bytes = 0;

		Const_byte_range_ptr const src(
			reinterpret_cast<char const*>(&cipher_buf.item(job.cipher_buf_idx)),
			sizeof(Tresor::Block));

		job.handle->fs().write( job.handle, src, written_bytes);

		job.state = Job_state::OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case Job_state::OP_WRITTEN_TO_VFS_HANDLE:
	{
		job.handle->seek(job.request.block_number() * Tresor::BLOCK_SIZE);
		bool const success {
			job.handle->fs().queue_read(
				job.handle, sizeof (Tresor::Block)) };

		if (!success) {
			return;
		}
		job.state = Job_state::READING_VFS_HANDLE_SUCCEEDED;
		progress = true;
		break;
	}
	case Job_state::READING_VFS_HANDLE_SUCCEEDED:
	{
		size_t read_bytes = 0;

		Byte_range_ptr const dst(
			reinterpret_cast<char *>(&plain_buf.item(job.plain_buf_idx)),
			sizeof(Tresor::Block));

		Read_result const result =
			job.handle->fs().complete_read(job.handle, dst, read_bytes);

		switch (result) {
		case Read_result::READ_QUEUED:          return;
		case Read_result::READ_ERR_WOULD_BLOCK: return;
		default: break;
		}
		job.request.success(result == Read_result::READ_OK);
		job.state = Job_state::COMPLETE;
		progress = true;
		return;
	}
	default:

		return;
	}
}


void Crypto::_execute_encrypt_block(Job                  &job,
                            Crypto_plain_buffer  &plain_buf,
                            Crypto_cipher_buffer &cipher_buf,
                            bool                 &progress)
{
	switch (job.state) {
	case Job_state::SUBMITTED:
	{
		job.handle->seek(job.request.block_number() * Tresor::BLOCK_SIZE);

		size_t written_bytes = 0;

		Const_byte_range_ptr const src(
			reinterpret_cast<char const*>(
				&plain_buf.item(job.plain_buf_idx)),
			sizeof(Tresor::Block));

		job.handle->fs().write(job.handle, src, written_bytes);

		job.state = Job_state::OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case Job_state::OP_WRITTEN_TO_VFS_HANDLE:
	{
		job.handle->seek(job.request.block_number() * Tresor::BLOCK_SIZE);
		bool success {
			job.handle->fs().queue_read(
				job.handle, sizeof (Tresor::Block)) };

		if (!success) {
			return;
		}
		job.state = Job_state::READING_VFS_HANDLE_SUCCEEDED;
		progress = true;
		return;
	}
	case Job_state::READING_VFS_HANDLE_SUCCEEDED:
	{
		size_t read_bytes = 0;

		Byte_range_ptr const dst(
			reinterpret_cast<char *>(&cipher_buf.item(job.cipher_buf_idx)),
			sizeof (Tresor::Block));

		Read_result const result =
			job.handle->fs().complete_read(job.handle, dst, read_bytes);

		switch (result) {
		case Read_result::READ_QUEUED:          return;
		case Read_result::READ_ERR_WOULD_BLOCK: return;
		default: break;
		}
		job.request.success(result == Read_result::READ_OK);
		job.state = Job_state::COMPLETE;
		progress = true;
		return;
	}
	default:

		return;
	}
}


void Crypto::execute(Crypto_plain_buffer  &plain_buf,
             Crypto_cipher_buffer &cipher_buf,
             bool                 &progress)
{
	switch (_job.op) {
	case Operation::ENCRYPT_BLOCK:

		_execute_encrypt_block(_job, plain_buf, cipher_buf, progress);
		break;

	case Operation::DECRYPT_BLOCK:

		_execute_decrypt_block(_job, plain_buf, cipher_buf, progress);
		break;

	case Operation::INVALID:

		break;
	}
}
