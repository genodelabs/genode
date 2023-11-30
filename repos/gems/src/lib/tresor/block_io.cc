/*
 * \brief  Module for accessing the back-end block device
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
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

Block_io_request::Block_io_request(Module_id src_module_id, Module_channel_id src_chan_id, Type type,
                                   Request_offset client_req_offset, Request_tag client_req_tag, Key_id key_id,
                                   Physical_block_address pba, Virtual_block_address vba, Block &blk, Hash &hash,
                                   bool &success)
:
	Module_request { src_module_id, src_chan_id, BLOCK_IO }, _type { type },
	_client_req_offset { client_req_offset }, _client_req_tag { client_req_tag },
	_key_id { key_id }, _pba { pba }, _vba { vba }, _blk { blk }, _hash { hash }, _success { success }
{ }


char const *Block_io_request::type_to_string(Type type)
{
	switch (type) {
	case READ: return "read";
	case WRITE: return "write";
	case SYNC: return "sync";
	case READ_CLIENT_DATA: return "read_client_data";
	case WRITE_CLIENT_DATA: return "write_client_data";
	}
	ASSERT_NEVER_REACHED;
}


void Block_io_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("block io: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Block_io_channel::_mark_req_failed(bool &progress, Error_string str)
{
	error("request failed: failed to ", str);
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Block_io_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
	if (VERBOSE_BLOCK_IO && (!VERBOSE_BLOCK_IO_PBA_FILTER || VERBOSE_BLOCK_IO_PBA == req._pba)) {
		switch (req._type) {
		case Request::READ:
		case Request::WRITE:
			log("block_io: ", req.type_to_string(req._type), " pba ", req._pba, " hash ", hash(req._blk));
			break;
		default: break;
		}
	}
}


void Block_io_channel::_read(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _file.read(READ_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Block_io_channel::_read_client_data(bool &progress)
{

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _file.read(READ_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&_blk, BLOCK_SIZE }, progress); break;
	case READ_OK:

		calc_hash(_blk, req._hash);
		_generate_req<Crypto_request>(
			PLAINTEXT_BLK_SUPPLIED, progress, Crypto_request::DECRYPT_CLIENT_DATA, req._client_req_offset,
			req._client_req_tag, req._key_id, *(Key_value *)0, req._pba, req._vba, _blk);
		return;

	case PLAINTEXT_BLK_SUPPLIED: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Block_io_channel::_write_client_data(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_generate_req<Crypto_request>(
			CIPHERTEXT_BLK_OBTAINED, progress, Crypto_request::ENCRYPT_CLIENT_DATA, req._client_req_offset,
			req._client_req_tag, req._key_id, *(Key_value *)0, req._pba, req._vba, _blk);
		break;

	case CIPHERTEXT_BLK_OBTAINED:

		calc_hash(_blk, req._hash);
		_file.write(WRITE_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&_blk, BLOCK_SIZE }, progress); break;
		break;

	case WRITE_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Block_io_channel::_write(bool &progress)
{

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _file.write(WRITE_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress); break;
	case WRITE_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Block_io_channel::_sync(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED: _file.sync(SYNC_OK, FILE_ERR, progress); break;
	case SYNC_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Block_io_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_type) {
	case Request::READ: _read(progress); break;
	case Request::WRITE: _write(progress); break;
	case Request::SYNC: _sync(progress); break;
	case Request::READ_CLIENT_DATA: _read_client_data(progress); break;
	case Request::WRITE_CLIENT_DATA: _write_client_data(progress); break;
	}
}


void Block_io::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


void Block_io_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Block_io_channel::Block_io_channel(Module_channel_id id, Vfs::Env &vfs_env, Xml_node const &xml_node)
:
	Module_channel { BLOCK_IO, id }, _vfs_env { vfs_env }, _path { xml_node.attribute_value("path", Tresor::Path()) }
{ }


Block_io::Block_io(Vfs::Env &vfs_env, Xml_node const &xml_node)
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++, vfs_env, xml_node);
		add_channel(*chan);
	}
}
