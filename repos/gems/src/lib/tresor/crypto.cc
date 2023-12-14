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
#include <tresor/hash.h>

using namespace Tresor;

bool Crypto::Add_key::execute(Crypto::Attr const &crypto_attr)
{
	bool progress { false };
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, crypto_attr.add_key_file);
		memcpy(_write_buf, &_attr.in_key.id, sizeof(Key_id));
		memcpy(_write_buf + sizeof(Key_id), &_attr.in_key.value, sizeof(Key_value));
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, 0, { _write_buf, sizeof(_write_buf) }, progress); break;
	case WRITE_OK:

		crypto_attr.key_files.add_crypto_key(_attr.in_key.id);
		_helper.mark_succeeded(progress);
		break;

	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Crypto::Remove_key::execute(Crypto::Attr const &crypto_attr)
{
	bool progress { false };
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, crypto_attr.remove_key_file);
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, 0, { (char *)&_attr.in_key_id, sizeof(Key_id) }, progress); break;
	case WRITE_OK:

		crypto_attr.key_files.remove_crypto_key(_attr.in_key_id);
		_helper.mark_succeeded(progress);
		break;

	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Crypto::Encrypt::execute(Crypto::Attr const &crypto_attr)
{
	bool progress { false };
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, crypto_attr.key_files.encrypt_file(_attr.in_key_id));
		_offset = _attr.in_pba * BLOCK_SIZE;
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, _offset, { (char *)&_attr.in_out_blk, BLOCK_SIZE }, progress); break;
	case WRITE_OK: _file->read(READ_OK, FILE_ERR, _offset, { (char *)&_attr.in_out_blk, BLOCK_SIZE }, progress); break;
	case READ_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file-operation error"); break;
	default: break;
	}
	return progress;
}


bool Crypto::Decrypt::execute(Crypto::Attr const &crypto_attr)
{
	bool progress { false };
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, crypto_attr.key_files.decrypt_file(_attr.in_key_id));
		_offset = _attr.in_pba * BLOCK_SIZE;
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, _offset, { (char *)&_attr.in_out_blk, BLOCK_SIZE }, progress); break;
	case WRITE_OK: _file->read(READ_OK, FILE_ERR, _offset, { (char *)&_attr.in_out_blk, BLOCK_SIZE }, progress); break;
	case READ_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file-operation error"); break;
	default: break;
	}
	return progress;
}
