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

bool Trust_anchor::Encrypt_key::execute(Trust_anchor::Attr const &ta_attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, ta_attr.encrypt_file);
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, 0, { (char *)&_attr.in_key_plaintext, KEY_SIZE }, progress); break;
	case WRITE_OK: _file->read(READ_OK, FILE_ERR, 0, { (char *)&_attr.out_key_ciphertext, KEY_SIZE }, progress); break;
	case READ_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Trust_anchor::Decrypt_key::execute(Trust_anchor::Attr const &ta_attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, ta_attr.decrypt_file);
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, 0, { (char *)&_attr.in_key_ciphertext, KEY_SIZE }, progress); break;
	case WRITE_OK: _file->read(READ_OK, FILE_ERR, 0, { (char *)&_attr.out_key_plaintext, KEY_SIZE }, progress); break;
	case READ_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Trust_anchor::Write_hash::execute(Trust_anchor::Attr const &ta_attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, ta_attr.hash_file);
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, 0, { (char *)&_attr.in_hash, HASH_SIZE }, progress); break;
	case WRITE_OK: _file->read(READ_OK, FILE_ERR, 0, { _result_buf, sizeof(_result_buf) }, progress); break;
	case READ_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Trust_anchor::Read_hash::execute(Trust_anchor::Attr const &ta_attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, ta_attr.hash_file);
		_helper.state = READ;
		progress = true;
		break;

	case READ: _file->read(READ_OK, FILE_ERR, 0, { (char *)&_attr.out_hash, HASH_SIZE }, progress); break;
	case READ_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Trust_anchor::Generate_key::execute(Trust_anchor::Attr const &ta_attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, ta_attr.generate_key_file);
		_helper.state = READ;
		progress = true;
		break;

	case READ: _file->read(READ_OK, FILE_ERR, 0, { (char *)&_attr.out_key_plaintext, KEY_SIZE }, progress); break;
	case READ_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Trust_anchor::Initialize::execute(Trust_anchor::Attr const &ta_attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, ta_attr.initialize_file);
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, 0, { _attr.in_passphrase.string(), _attr.in_passphrase.length() - 1 }, progress); break;
	case WRITE_OK: _file->read(READ_OK, FILE_ERR, 0, { _result_buf, sizeof(_result_buf) }, progress); break;
	case READ_OK:

		if (strcmp(_result_buf, "ok", sizeof(_result_buf)))
			_helper.mark_failed(progress, {"trust anchor did not return \"ok\""});
		else
			_helper.mark_succeeded(progress);
		break;

	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}
