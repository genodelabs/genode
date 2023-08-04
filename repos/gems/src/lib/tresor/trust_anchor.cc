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

/* tresor includes */
#include <tresor/trust_anchor.h>

using namespace Tresor;

Trust_anchor_request::Trust_anchor_request(Module_id src_module_id, Module_channel_id src_chan_id,
                                           Type type, Key_value &key_plaintext, Key_value &key_ciphertext,
                                           Hash &hash, Passphrase passphrase, bool &success)
:
	Module_request { src_module_id, src_chan_id, TRUST_ANCHOR }, _type { type }, _key_plaintext { key_plaintext },
	_key_ciphertext { key_ciphertext }, _hash { hash }, _pass { passphrase }, _success { success }
{ }


char const *Trust_anchor_request::type_to_string(Type type)
{
	switch (type) {
	case CREATE_KEY: return "create_key";
	case ENCRYPT_KEY: return "encrypt_key";
	case DECRYPT_KEY: return "decrypt_key";
	case WRITE_HASH: return "write_hash";
	case READ_HASH: return "read_hash";
	case INITIALIZE: return "initialize";
	}
	ASSERT_NEVER_REACHED;
}


void Trust_anchor_channel::_mark_req_failed(bool &progress, Error_string str)
{
	error("trust_anchor: request (", *_req_ptr, ") failed: ", str);
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Trust_anchor_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Trust_anchor_channel::_read_hash(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _hashsum_file.read(READ_OK, FILE_ERR, 0, { (char *)&req._hash, HASH_SIZE }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::_create_key(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _generate_key_file.read(READ_OK, FILE_ERR, 0, { (char *)&req._key_plaintext, KEY_SIZE }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::_initialize(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _initialize_file.write(WRITE_OK, FILE_ERR, 0, { req._pass.string(), req._pass.length() - 1 }, progress); break;
	case WRITE_OK: _initialize_file.read(READ_OK, FILE_ERR, 0, { _result_buf, sizeof(_result_buf) }, progress); break;
	case READ_OK:

		if (strcmp(_result_buf, "ok", sizeof(_result_buf)))
			_mark_req_failed(progress, { "trust anchor did not return \"ok\""});
		else
			_mark_req_successful(progress);
		break;

	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::_write_hash(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _hashsum_file.write(WRITE_OK, FILE_ERR, 0, { (char *)&req._hash, HASH_SIZE }, progress); break;
	case WRITE_OK: _hashsum_file.read(READ_OK, FILE_ERR, 0, { _result_buf, 0 }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::_encrypt_key(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _encrypt_file.write(WRITE_OK, FILE_ERR, 0, { (char *)&req._key_plaintext, KEY_SIZE }, progress); break;
	case WRITE_OK: _encrypt_file.read(READ_OK, FILE_ERR, 0, { (char *)&req._key_ciphertext, KEY_SIZE }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::_decrypt_key(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _decrypt_file.write(WRITE_OK, FILE_ERR, 0, { (char *)&req._key_ciphertext, KEY_SIZE }, progress); break;
	case WRITE_OK: _decrypt_file.read(READ_OK, FILE_ERR, 0, { (char *)&req._key_plaintext, KEY_SIZE }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (req._type) {
	case Request::INITIALIZE: _initialize(progress); break;
	case Request::WRITE_HASH: _write_hash(progress); break;
	case Request::READ_HASH: _read_hash(progress); break;
	case Request::CREATE_KEY: _create_key(progress); break;
	case Request::ENCRYPT_KEY: _encrypt_key(progress); break;
	case Request::DECRYPT_KEY: _decrypt_key(progress); break;
	}
}


Trust_anchor_channel::Trust_anchor_channel(Module_channel_id id, Vfs::Env &vfs_env, Xml_node const &xml_node)
:
	Module_channel { TRUST_ANCHOR, id }, _vfs_env { vfs_env }, _path { xml_node.attribute_value("path", Tresor::Path()) }
{ }


Trust_anchor::Trust_anchor(Vfs::Env &vfs_env, Xml_node const &xml_node)
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++, vfs_env, xml_node);
		add_channel(*chan);
	}
}


void Trust_anchor_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


void Trust_anchor::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
