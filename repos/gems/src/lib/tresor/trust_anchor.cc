/*
 * \brief  Module for accessing the systems trust anchor
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>

/* tresor includes */
#include <tresor/trust_anchor.h>

using namespace Tresor;


/**************************
 ** Trust_anchor_request **
 **************************/

void Trust_anchor_request::create(void       *buf_ptr,
                                  size_t      buf_size,
                                  uint64_t    src_module_id,
                                  uint64_t    src_request_id,
                                  size_t      req_type,
                                  void       *key_plaintext_ptr,
                                  void       *key_ciphertext_ptr,
                                  char const *passphrase_ptr,
                                  void       *hash_ptr)
{
	Trust_anchor_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;
	req._passphrase_ptr = (addr_t)passphrase_ptr;
	if (key_plaintext_ptr != nullptr)
		memcpy(
			&req._key_plaintext, key_plaintext_ptr,
			sizeof(req._key_plaintext));

	if (key_ciphertext_ptr != nullptr)
		memcpy(
			&req._key_ciphertext, key_ciphertext_ptr,
			sizeof(req._key_ciphertext));

	if (hash_ptr != nullptr)
		memcpy(&req._hash, hash_ptr, sizeof(req._hash));

	if (sizeof(req) > buf_size) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Trust_anchor_request::Trust_anchor_request(Module_id         src_module_id,
                                           Module_request_id src_request_id)
:
	Module_request { src_module_id, src_request_id, TRUST_ANCHOR }
{ }


char const *Trust_anchor_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case CREATE_KEY: return "create_key";
	case ENCRYPT_KEY: return "encrypt_key";
	case DECRYPT_KEY: return "decrypt_key";
	case SECURE_SUPERBLOCK: return "secure_superblock";
	case GET_LAST_SB_HASH: return "get_last_sb_hash";
	case INITIALIZE: return "initialize";
	}
	return "?";
}


/******************
 ** Trust_anchor **
 ******************/

void Trust_anchor::_execute_write_read_operation(Vfs::Vfs_handle   &file,
                                                 String<128> const &file_path,
                                                 Channel           &channel,
                                                 char        const *write_buf,
                                                 char              *read_buf,
                                                 size_t             read_size,
                                                 bool              &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::WRITE_PENDING:

		file.seek(channel._file_offset);
		channel._state = Channel::WRITE_IN_PROGRESS;
		progress = true;
		return;

	case Channel::WRITE_IN_PROGRESS:
	{
		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr src {
			write_buf + channel._file_offset, channel._file_size };

		Write_result const result =
			file.fs().write(&file, src, nr_of_written_bytes);

		switch (result) {

		case Write_result::WRITE_ERR_WOULD_BLOCK:
			return;

		case Write_result::WRITE_OK:

			channel._file_offset += nr_of_written_bytes;
			channel._file_size -= nr_of_written_bytes;

			if (channel._file_size > 0) {

				channel._state = Channel::WRITE_PENDING;
				progress = true;
				return;
			}
			channel._state = Channel::READ_PENDING;
			channel._file_offset = 0;
			channel._file_size = read_size;
			progress = true;
			return;

		default:

			req._success = false;
			error("failed to write file ", file_path);
			channel._state = Channel::COMPLETE;
			progress = true;
			return;
		}
	}
	case Channel::READ_PENDING:

		file.seek(channel._file_offset);

		if (!file.fs().queue_read(&file, channel._file_size)) {
			return;
		}
		channel._state = Channel::READ_IN_PROGRESS;
		progress = true;
		return;

	case Channel::READ_IN_PROGRESS:
	{
		size_t nr_of_read_bytes { 0 };

		Byte_range_ptr dst {
			read_buf + channel._file_offset, channel._file_size };

		Read_result const result {
			file.fs().complete_read( &file, dst, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_OK:

			channel._file_offset += nr_of_read_bytes;
			channel._file_size -= nr_of_read_bytes;
			req._success = true;

			if (channel._file_size > 0) {

				channel._state = Channel::READ_PENDING;
				progress = true;
				return;
			}
			channel._state = Channel::COMPLETE;
			progress = true;
			return;

		default:

			req._success = false;
			error("failed to read file ", file_path);
			channel._state = Channel::COMPLETE;
			return;
		}
	}
	default:

		return;
	}
}


void Trust_anchor::_execute_write_operation(Vfs::Vfs_handle   &file,
                                            String<128> const &file_path,
                                            Channel           &channel,
                                            char        const *write_buf,
                                            bool              &progress,
                                            bool               result_via_read)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::WRITE_PENDING:

		file.seek(channel._file_offset);
		channel._state = Channel::WRITE_IN_PROGRESS;
		progress = true;
		return;

	case Channel::WRITE_IN_PROGRESS:
	{
		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr src {
			write_buf + channel._file_offset, channel._file_size };

		Write_result const result =
			file.fs().write(&file, src, nr_of_written_bytes);

		switch (result) {

		case Write_result::WRITE_ERR_WOULD_BLOCK:
			return;

		case Write_result::WRITE_OK:

			channel._file_offset += nr_of_written_bytes;
			channel._file_size -= nr_of_written_bytes;

			if (channel._file_size > 0) {

				channel._state = Channel::WRITE_PENDING;
				progress = true;
				return;
			}
			channel._state = Channel::READ_PENDING;
			channel._file_offset = 0;

			if (result_via_read)
				channel._file_size = sizeof(_read_buf);
			else
				channel._file_size = 0;

			progress = true;
			return;

		default:

			req._success = false;
			error("failed to write file ", file_path);
			channel._state = Channel::COMPLETE;
			progress = true;
			return;
		}
	}
	case Channel::READ_PENDING:

		file.seek(channel._file_offset);

		if (!file.fs().queue_read(&file, channel._file_size)) {
			return;
		}
		channel._state = Channel::READ_IN_PROGRESS;
		progress = true;
		return;

	case Channel::READ_IN_PROGRESS:
	{
		size_t nr_of_read_bytes { 0 };
		Byte_range_ptr dst {
			_read_buf + channel._file_offset, channel._file_size };

		Read_result const result {
			file.fs().complete_read(&file, dst, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_OK:

			channel._file_offset += nr_of_read_bytes;
			channel._file_size -= nr_of_read_bytes;

			if (channel._file_size > 0) {

				channel._state = Channel::READ_PENDING;
				progress = true;
				return;
			}
			if (result_via_read) {
				req._success = !strcmp(_read_buf, "ok", 3);
			} else
				req._success = true;

			channel._state = Channel::COMPLETE;
			progress = true;
			return;

		default:

			req._success = false;
			error("failed to read file ", file_path);
			channel._state = Channel::COMPLETE;
			return;
		}
	}
	default:

		return;
	}
}


void Trust_anchor::_execute_read_operation(Vfs::Vfs_handle   &file,
                                           String<128> const &file_path,
                                           Channel           &channel,
                                           char              *read_buf,
                                           bool              &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::READ_PENDING:

		file.seek(channel._file_offset);

		if (!file.fs().queue_read(&file, channel._file_size)) {
			return;
		}
		channel._state = Channel::READ_IN_PROGRESS;
		progress = true;
		return;

	case Channel::READ_IN_PROGRESS:
	{
		size_t nr_of_read_bytes { 0 };
		Byte_range_ptr dst {
			read_buf + channel._file_offset, channel._file_size };

		Read_result const result {
			file.fs().complete_read(&file, dst, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_OK:

			channel._file_offset += nr_of_read_bytes;
			channel._file_size -= nr_of_read_bytes;
			req._success = true;

			if (channel._file_size > 0) {

				channel._state = Channel::READ_PENDING;
				progress = true;
				return;
			}
			channel._state = Channel::COMPLETE;
			progress = true;
			return;

		default:

			req._success = false;
			error("failed to read file ", file_path);
			channel._state = Channel::COMPLETE;
			return;
		}
	}
	default:

		return;
	}
}


void Trust_anchor::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		Request &req { channel._request };
		switch (req._type) {
		case Request::INITIALIZE:

			if (channel._state == Channel::SUBMITTED) {
				channel._state = Channel::WRITE_PENDING;
				channel._file_offset = 0;
				channel._file_size =
					strlen((char const *)req._passphrase_ptr);
			}
			_execute_write_operation(
				_initialize_file, _initialize_path, channel,
				(char const *)req._passphrase_ptr, progress, true);

			break;

		case Request::SECURE_SUPERBLOCK:

			if (channel._state == Channel::SUBMITTED) {
				channel._state = Channel::WRITE_PENDING;
				channel._file_offset = 0;
				channel._file_size = sizeof(req._hash);
			}
			_execute_write_operation(
				_hashsum_file, _hashsum_path, channel,
				(char const *)&req._hash, progress, false);

			break;

		case Request::GET_LAST_SB_HASH:

			if (channel._state == Channel::SUBMITTED) {
				channel._state = Channel::READ_PENDING;
				channel._file_offset = 0;
				channel._file_size = sizeof(req._hash);
			}
			_execute_read_operation(
				_hashsum_file, _hashsum_path, channel,
				(char *)&req._hash, progress);

			break;

		case Request::CREATE_KEY:

			if (channel._state == Channel::SUBMITTED) {
				channel._state = Channel::READ_PENDING;
				channel._file_offset = 0;
				channel._file_size = sizeof(req._key_plaintext);
			}
			_execute_read_operation(
				_generate_key_file, _generate_key_path, channel,
				(char *)req._key_plaintext, progress);

			break;

		case Request::ENCRYPT_KEY:

			if (channel._state == Channel::SUBMITTED) {
				channel._state = Channel::WRITE_PENDING;
				channel._file_offset = 0;
				channel._file_size = sizeof(req._key_plaintext);
			}
			_execute_write_read_operation(
				_encrypt_file, _encrypt_path, channel,
				(char const *)req._key_plaintext,
				(char *)req._key_ciphertext,
				sizeof(req._key_ciphertext),
				progress);

			break;

		case Request::DECRYPT_KEY:

			if (channel._state == Channel::SUBMITTED) {
				channel._state = Channel::WRITE_PENDING;
				channel._file_offset = 0;
				channel._file_size = sizeof(req._key_ciphertext);
			}
			_execute_write_read_operation(
				_decrypt_file, _decrypt_path, channel,
				(char const *)req._key_ciphertext,
				(char *)req._key_plaintext,
				sizeof(req._key_plaintext),
				progress);

			break;

		default:

			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}


Trust_anchor::Trust_anchor(Vfs::Env       &vfs_env,
                           Xml_node const &xml_node)
:
	_vfs_env { vfs_env },
	_path    { xml_node.attribute_value("path", String<128>()) }
{ }


bool Trust_anchor::_peek_completed_request(uint8_t *buf_ptr,
                                           size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(channel._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &channel._request, sizeof(channel._request));
			return true;
		}
	}
	return false;
}


void Trust_anchor::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INACTIVE;
}


bool Trust_anchor::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}

void Trust_anchor::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			req.dst_request_id(id);
			_channels[id]._request = *static_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}
