/*
 * \brief  Module for encrypting/decrypting single data blocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/crypto.h>
#include <tresor/client_data.h>
#include <tresor/hash.h>

using namespace Tresor;

Crypto_request::Crypto_request(Module_id src_module_id, Module_channel_id src_chan_id, Type type,
                               Request_offset client_req_offset, Request_tag client_req_tag, Key_id key_id,
                               Key_value const &key_plaintext, Physical_block_address pba, Virtual_block_address vba,
                               Block &blk, bool &success)
:
	Module_request { src_module_id, src_chan_id, CRYPTO }, _type { type }, _client_req_offset { client_req_offset },
	_client_req_tag { client_req_tag }, _pba { pba }, _vba { vba }, _key_id { key_id }, _key_plaintext { key_plaintext },
	_blk { blk }, _success { success }
{ }


void Crypto_request::print(Output &out) const
{
	Genode::print(out, type_to_string(_type));
	switch (_type) {
	case ADD_KEY:
	case REMOVE_KEY: Genode::print(out, " ", _key_id); break;
	case DECRYPT:
	case ENCRYPT:
	case DECRYPT_CLIENT_DATA:
	case ENCRYPT_CLIENT_DATA: Genode::print(out, " pba ", _pba); break;
	default: break;
	}
}


char const *Crypto_request::type_to_string(Type type)
{
	switch (type) {
	case ADD_KEY: return "add_key";
	case REMOVE_KEY: return "remove_key";
	case ENCRYPT_CLIENT_DATA: return "encrypt_client_data";
	case DECRYPT_CLIENT_DATA: return "decrypt_client_data";
	case ENCRYPT: return "encrypt";
	case DECRYPT: return "decrypt";
	}
	ASSERT_NEVER_REACHED;
}


void Crypto_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("crypto: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


Constructible<Crypto_channel::Key_directory> &Crypto_channel::_key_dir(Key_id key_id)
{
	for (Constructible<Key_directory> &key_dir : _key_dirs)
		if (key_dir.constructed() && key_dir->key_id == key_id)
			return key_dir;
	ASSERT_NEVER_REACHED;
}


void Crypto_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("crypto: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Crypto_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
	if (VERBOSE_WRITE_VBA && req._type == Request::ENCRYPT_CLIENT_DATA)
		log("  encrypt leaf data: plaintext ", _blk, " hash ", hash(_blk),
		    "\n  update branch:\n    ", Branch_lvl_prefix("leaf data: "), req._blk);

	if (VERBOSE_READ_VBA && req._type == Request::DECRYPT_CLIENT_DATA)
		log("    ", Branch_lvl_prefix("leaf data: "), req._blk,
		    "\n  decrypt leaf data: plaintext ", _blk, " hash ", hash(_blk));

	if (VERBOSE_CRYPTO) {
		switch (req._type) {
		case Request::DECRYPT_CLIENT_DATA:
		case Request::ENCRYPT_CLIENT_DATA:
			log("crypto: ", req.type_to_string(req._type), " pba ", req._pba, " vba ", req._vba,
			    " plain ", _blk, " cipher ", req._blk);
			break;
		default: break;
		}
	}
	if (VERBOSE_BLOCK_IO && (!VERBOSE_BLOCK_IO_PBA_FILTER || VERBOSE_BLOCK_IO_PBA == req._pba)) {
		switch (req._type) {
		case Request::DECRYPT_CLIENT_DATA:
			log("block_io: read pba ", req._pba, " hash ", hash(req._blk), " (plaintext hash ", hash(_blk), ")");
			break;
		case Request::ENCRYPT_CLIENT_DATA:
			log("block_io: write pba ", req._pba, " hash ", hash(req._blk), " (plaintext hash ", hash(_blk), ")");
			break;
		default: break;
		}
	}
}


void Crypto_channel::_add_key(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		memcpy(_add_key_buf, &req._key_id, sizeof(Key_id));
		memcpy(_add_key_buf + sizeof(Key_id), &req._key_plaintext, KEY_SIZE);
		_add_key_file.write(WRITE_OK, FILE_ERR, 0, { _add_key_buf, sizeof(_add_key_buf) }, progress);
		break;

	case WRITE_OK:
	{
		Constructible<Key_directory> *key_dir_ptr { nullptr };
		for (Constructible<Key_directory> &key_dir : _key_dirs)
			if (!key_dir.constructed())
				key_dir_ptr = &key_dir;
		if (!key_dir_ptr) {
			_mark_req_failed(progress, "find unused key dir");
			break;
		}
		key_dir_ptr->construct(*this, req._key_id);
		_mark_req_successful(progress);
		break;
	}
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Crypto_channel::_remove_key(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _remove_key_file.write(WRITE_OK, FILE_ERR, 0, { (char *)&req._key_id, sizeof(Key_id) }, progress); break;
	case WRITE_OK:

		_key_dir(req._key_id).destruct();
		_mark_req_successful(progress);
		break;

	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: return;
	}
}


void Crypto_channel::_encrypt_client_data(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_generate_req<Client_data_request>(
			PLAINTEXT_BLK_OBTAINED, progress, Client_data_request::OBTAIN_PLAINTEXT_BLK,
			req._client_req_offset, req._client_req_tag, req._pba, req._vba, _blk);;
		break;

	case PLAINTEXT_BLK_OBTAINED:

		_key_dir(req._key_id)->encrypt_file.write(
			WRITE_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&_blk, BLOCK_SIZE }, progress);
		break;

	case WRITE_OK:

		_key_dir(req._key_id)->encrypt_file.read(
			READ_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress);
		break;

	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation"); break;
	default: break;
	}
}


void Crypto_channel::_encrypt(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_key_dir(req._key_id)->encrypt_file.write(
			WRITE_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress);
		break;

	case WRITE_OK:

		_key_dir(req._key_id)->encrypt_file.read(
			READ_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress);
		break;

	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation"); break;
	default: break;
	}
}


void Crypto_channel::_decrypt(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_key_dir(req._key_id)->decrypt_file.write(
			WRITE_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress);
		break;

	case WRITE_OK:

		_key_dir(req._key_id)->decrypt_file.read(
			READ_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress);
		break;

	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation"); break;
	default: break;
	}
}


void Crypto_channel::_decrypt_client_data(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_key_dir(req._key_id)->decrypt_file.write(
			WRITE_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress);
		break;

	case WRITE_OK:

		_key_dir(req._key_id)->decrypt_file.read(
			READ_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&_blk, BLOCK_SIZE }, progress);
		break;

	case READ_OK:

		_generate_req<Client_data_request>(
			PLAINTEXT_BLK_SUPPLIED, progress, Client_data_request::SUPPLY_PLAINTEXT_BLK,
			req._client_req_offset, req._client_req_tag, req._pba, req._vba, _blk);;
		break;

	case PLAINTEXT_BLK_SUPPLIED: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Crypto_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


void Crypto_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_type) {
	case Request::ADD_KEY: _add_key(progress); break;
	case Request::REMOVE_KEY: _remove_key(progress); break;
	case Request::DECRYPT: _decrypt(progress); break;
	case Request::ENCRYPT: _encrypt(progress); break;
	case Request::DECRYPT_CLIENT_DATA: _decrypt_client_data(progress); break;
	case Request::ENCRYPT_CLIENT_DATA: _encrypt_client_data(progress); break;
	}
}


void Crypto::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


Crypto_channel::Crypto_channel(Module_channel_id id, Vfs::Env &vfs_env, Xml_node const &xml_node)
:
	Module_channel { CRYPTO, id }, _vfs_env { vfs_env }, _path { xml_node.attribute_value("path", Tresor::Path()) }
{ }


Crypto::Crypto(Vfs::Env &vfs_env, Xml_node const &xml_node)
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++, vfs_env, xml_node);
		add_channel(*chan);
	}
}
