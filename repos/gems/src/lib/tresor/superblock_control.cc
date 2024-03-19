/*
 * \brief  Module for management of the superblocks
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
#include <tresor/superblock_control.h>
#include <tresor/crypto.h>
#include <tresor/free_tree.h>
#include <tresor/hash.h>

using namespace Tresor;

void Superblock_control::Write_vbas::_start_write_vba(Execute_attr const &attr, bool &progress)
{
	Virtual_block_address vba = _attr.in_first_vba + _num_written_vbas;
	if (attr.sb.curr_snap().gen != attr.curr_gen) {
		Snapshot &snap { attr.sb.curr_snap() };
		attr.sb.curr_snap_idx = attr.sb.snapshots.alloc_idx(attr.curr_gen, attr.sb.last_secured_generation);
		attr.sb.curr_snap() = snap;
		attr.sb.curr_snap().keep = false;
	}
	Key_id key_id { attr.sb.state == Superblock::REKEYING && vba >= attr.sb.rekeying_vba ?
		attr.sb.previous_key.id : attr.sb.current_key.id };

	_ft.construct(attr.sb.free_number, attr.sb.free_gen, attr.sb.free_hash, attr.sb.free_max_level, attr.sb.free_degree, attr.sb.free_leaves);
	_mt.construct(attr.sb.meta_number, attr.sb.meta_gen, attr.sb.meta_hash, attr.sb.meta_max_level, attr.sb.meta_degree, attr.sb.meta_leaves);
	_write_vba.generate(
		_helper, WRITE_VBA, WRITE_VBA_SUCCEEDED, progress, attr.sb.snapshots.items[attr.sb.curr_snap_idx], attr.sb.snapshots,
		*_ft, *_mt, vba, key_id, attr.sb.previous_key.id, attr.sb.degree, attr.sb.max_vba(), _attr.in_client_req_offset,
		_attr.in_client_req_tag, attr.curr_gen, attr.sb.last_secured_generation, attr.sb.state == Superblock::REKEYING,
		attr.sb.rekeying_vba);

	if (VERBOSE_WRITE_VBA)
		log("write vba ", vba, ": snap ", attr.sb.curr_snap_idx, " key ", key_id, " gen ", attr.curr_gen);
}


bool Superblock_control::Write_vbas::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		if (!_attr.in_num_vbas) {
			_helper.mark_failed(progress, "number of blocks is 0");
			break;
		}
		if (_attr.in_first_vba + _attr.in_num_vbas - 1 > attr.sb.max_vba()) {
			_helper.mark_failed(progress, "invalid VBA range");
			break;
		}
		_start_write_vba(attr, progress);
		break;

	case WRITE_VBA: progress |= _write_vba.execute(attr.vbd, attr.client_data, attr.block_io, attr.free_tree, attr.meta_tree, attr.crypto); break;
	case WRITE_VBA_SUCCEEDED:

		if (++_num_written_vbas < _attr.in_num_vbas)
			_start_write_vba(attr, progress);
		else
			_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


void Superblock_control::Read_vbas::_start_read_vba(Execute_attr const &attr, bool &progress)
{
	Virtual_block_address vba = _attr.in_first_vba + _num_read_vbas;
	Key_id key_id { attr.sb.state == Superblock::REKEYING && vba >= attr.sb.rekeying_vba ?
		attr.sb.previous_key.id : attr.sb.current_key.id };

	_read_vba.generate(
		_helper, READ_VBA, READ_VBA_SUCCEEDED, progress, attr.sb.snapshots.items[attr.sb.curr_snap_idx], vba, key_id,
		attr.sb.degree, _attr.in_client_req_offset, _attr.in_client_req_tag);

	if (VERBOSE_READ_VBA)
		log("read vba ", vba, ": snap ", attr.sb.curr_snap_idx, " key ", key_id, " gen ", attr.curr_gen);
}


bool Superblock_control::Read_vbas::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		if (!_attr.in_num_vbas) {
			_helper.mark_failed(progress, "number of blocks is 0");
			break;
		}
		if (_attr.in_first_vba + _attr.in_num_vbas - 1 > attr.sb.max_vba()) {
			_helper.mark_failed(progress, "invalid VBA range");
			break;
		}
		_start_read_vba(attr, progress);
		break;

	case READ_VBA: progress |= _read_vba.execute(attr.vbd, attr.client_data, attr.block_io, attr.crypto); break;
	case READ_VBA_SUCCEEDED:

		if (++_num_read_vbas < _attr.in_num_vbas)
			_start_read_vba(attr, progress);
		else
			_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


bool Superblock_control::Extend_free_tree::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		_num_pbas = _attr.in_num_pbas;
		Physical_block_address last_used_pba { attr.sb.first_pba + (attr.sb.nr_of_pbas - 1) };
		Number_of_blocks nr_of_unused_pbas { MAX_PBA - last_used_pba };

		if (_num_pbas > nr_of_unused_pbas) {
			_helper.mark_failed(progress, "check number of unused blocks");
			break;
		}
		if (attr.sb.state == Superblock::NORMAL) {

			_attr.out_extension_finished = false;
			attr.sb.state = Superblock::EXTENDING_FT;
			attr.sb.resizing_nr_of_pbas = _num_pbas;
			attr.sb.resizing_nr_of_leaves = 0;
			_pba = last_used_pba + 1;
			if (VERBOSE_FT_EXTENSION)
				log("free_tree ext init: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)attr.sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)attr.sb.resizing_nr_of_leaves);

			_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
			break;

		} else if (attr.sb.state == Superblock::EXTENDING_FT) {

			_pba = last_used_pba + 1;
			_num_pbas = attr.sb.resizing_nr_of_pbas;

			if (VERBOSE_FT_EXTENSION)
				log("free_tree ext step: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)attr.sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)attr.sb.resizing_nr_of_leaves);

			_num_pbas = attr.sb.resizing_nr_of_pbas;
			_pba = attr.sb.first_pba + attr.sb.nr_of_pbas;

			_ft.construct(attr.sb.free_number, attr.sb.free_gen, attr.sb.free_hash, attr.sb.free_max_level, attr.sb.free_degree, attr.sb.free_leaves);
			_mt.construct(attr.sb.meta_number, attr.sb.meta_gen, attr.sb.meta_hash, attr.sb.meta_max_level, attr.sb.meta_degree, attr.sb.meta_leaves);
			_extend_free_tree.generate(_helper, EXTEND_FREE_TREE, EXTEND_FREE_TREE_SUCCEEDED, progress, attr.curr_gen, *_ft, *_mt, _pba, _num_pbas);

		} else
			_helper.mark_failed(progress, "check superblock state");

		break;
	}
	case EXTEND_FREE_TREE: progress |= _extend_free_tree.execute(attr.free_tree, attr.block_io, attr.meta_tree); break;
	case EXTEND_FREE_TREE_SUCCEEDED:
	{
		if (_num_pbas >= attr.sb.resizing_nr_of_pbas) {
			_helper.mark_failed(progress, "check number of pbas");
			break;
		}
		Number_of_blocks const nr_of_added_pbas { attr.sb.resizing_nr_of_pbas - _num_pbas };
		Physical_block_address const new_first_unused_pba { attr.sb.first_pba + (attr.sb.nr_of_pbas + nr_of_added_pbas) };
		if (_pba != new_first_unused_pba) {
			_helper.mark_failed(progress, "check new first unused pba");
			break;
		}
		attr.sb.nr_of_pbas = attr.sb.nr_of_pbas + nr_of_added_pbas;
		attr.sb.resizing_nr_of_pbas = _num_pbas;
		attr.sb.resizing_nr_of_leaves += _nr_of_leaves;
		attr.sb.curr_snap_idx = attr.sb.snapshots.newest_snap_idx();

		if (!_num_pbas) {
			attr.sb.state = Superblock::NORMAL;
			_attr.out_extension_finished = true;
		}
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;
	}
	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


bool Superblock_control::Extend_vbd::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		_num_pbas = _attr.in_num_pbas;
		Physical_block_address last_used_pba { attr.sb.first_pba + (attr.sb.nr_of_pbas - 1) };
		Number_of_blocks nr_of_unused_pbas { MAX_PBA - last_used_pba };

		if (_num_pbas > nr_of_unused_pbas) {
			_helper.mark_failed(progress, "check number of unused blocks");
			break;
		}
		if (attr.sb.state == Superblock::NORMAL) {

			_attr.out_extension_finished = false;
			attr.sb.state = Superblock::EXTENDING_VBD;
			attr.sb.resizing_nr_of_pbas = _num_pbas;
			attr.sb.resizing_nr_of_leaves = 0;
			_pba = last_used_pba + 1;
			if (VERBOSE_VBD_EXTENSION)
				log("vbd ext init: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)attr.sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)attr.sb.resizing_nr_of_leaves);

			_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
			break;

		} else if (attr.sb.state == Superblock::EXTENDING_VBD) {

			_pba = last_used_pba + 1;
			_num_pbas = attr.sb.resizing_nr_of_pbas;

			if (VERBOSE_VBD_EXTENSION)
				log("vbd ext step: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)attr.sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)attr.sb.resizing_nr_of_leaves);

			_num_pbas = attr.sb.resizing_nr_of_pbas;
			_pba = attr.sb.first_pba + attr.sb.nr_of_pbas;

			_ft.construct(attr.sb.free_number, attr.sb.free_gen, attr.sb.free_hash, attr.sb.free_max_level, attr.sb.free_degree, attr.sb.free_leaves);
			_mt.construct(attr.sb.meta_number, attr.sb.meta_gen, attr.sb.meta_hash, attr.sb.meta_max_level, attr.sb.meta_degree, attr.sb.meta_leaves);
			_extend_vbd.generate(
				_helper, EXTEND_VBD, EXTEND_VBD_SUCCEEDED, progress, _nr_of_leaves, attr.sb.snapshots,
				attr.sb.degree, attr.curr_gen, attr.sb.last_secured_generation, _pba, _num_pbas, *_ft,
				*_mt, attr.sb.degree, attr.sb.max_vba(), attr.sb.previous_key.id, attr.sb.current_key.id,
				attr.sb.state == Superblock::REKEYING, attr.sb.rekeying_vba);

		} else
			_helper.mark_failed(progress, "check superblock state");

		break;
	}
	case EXTEND_VBD: progress |= _extend_vbd.execute(attr.vbd, attr.block_io, attr.free_tree, attr.meta_tree); break;
	case EXTEND_VBD_SUCCEEDED:
	{
		if (_num_pbas >= attr.sb.resizing_nr_of_pbas) {
			_helper.mark_failed(progress, "check number of pbas");
			break;
		}
		Number_of_blocks const nr_of_added_pbas { attr.sb.resizing_nr_of_pbas - _num_pbas };
		Physical_block_address const new_first_unused_pba { attr.sb.first_pba + (attr.sb.nr_of_pbas + nr_of_added_pbas) };
		if (_pba != new_first_unused_pba) {
			_helper.mark_failed(progress, "check new first unused pba");
			break;
		}
		attr.sb.nr_of_pbas = attr.sb.nr_of_pbas + nr_of_added_pbas;
		attr.sb.resizing_nr_of_pbas = _num_pbas;
		attr.sb.resizing_nr_of_leaves += _nr_of_leaves;
		attr.sb.curr_snap_idx = attr.sb.snapshots.newest_snap_idx();

		if (!_num_pbas) {
			attr.sb.state = Superblock::NORMAL;
			_attr.out_extension_finished = true;
		}
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;
	}
	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


bool Superblock_control::Rekey::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_attr.out_rekeying_finished = false;
		switch (attr.sb.state) {
		case Superblock::NORMAL:

			attr.sb.state = Superblock::REKEYING;
			attr.sb.rekeying_vba = 0;
			attr.sb.previous_key = attr.sb.current_key;
			attr.sb.current_key.id++;
			_generate_key.generate(_helper, GENERATE_KEY, GENERATE_KEY_SUCCEEDED, progress, attr.sb.current_key.value);
			break;

		case Superblock::REKEYING:

			_ft.construct(attr.sb.free_number, attr.sb.free_gen, attr.sb.free_hash, attr.sb.free_max_level, attr.sb.free_degree, attr.sb.free_leaves);
			_mt.construct(attr.sb.meta_number, attr.sb.meta_gen, attr.sb.meta_hash, attr.sb.meta_max_level, attr.sb.meta_degree, attr.sb.meta_leaves);
			_rekey_vba.generate(
				_helper, REKEY_VBA, REKEY_VBA_SUCCEEDED, progress, attr.sb.snapshots, *_ft, *_mt, attr.sb.rekeying_vba,
				attr.curr_gen, attr.sb.last_secured_generation, attr.sb.current_key.id, attr.sb.previous_key.id, attr.sb.degree, attr.sb.max_vba());

			if (VERBOSE_REKEYING)
				log("rekey vba ", attr.sb.rekeying_vba, ":\n  update vbd: keys ", attr.sb.previous_key.id, ",", attr.sb.current_key.id,
				    " generations ", attr.sb.last_secured_generation, ",", attr.curr_gen);
			break;

		default:

			_helper.mark_failed(progress, "bad superblock state");
			break;
		}
		break;

	case GENERATE_KEY: progress |= _generate_key.execute(attr.trust_anchor); break;
	case GENERATE_KEY_SUCCEEDED:

		_add_key.generate(_helper, ADD_KEY, ADD_KEY_SUCCEEDED, progress, attr.sb.current_key);
		if (VERBOSE_REKEYING)
			log("start rekeying:\n  update sb: keys ", attr.sb.previous_key.id, ",", attr.sb.current_key.id);
		break;

	case ADD_KEY: progress |= _add_key.execute(attr.crypto); break;
	case ADD_KEY_SUCCEEDED:

		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", attr.curr_gen);
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;

	case REKEY_VBA: progress |= _rekey_vba.execute(attr.vbd, attr.block_io, attr.crypto, attr.free_tree, attr.meta_tree); break;
	case REKEY_VBA_SUCCEEDED:
	{
		Number_of_leaves max_nr_of_leaves { 0 };
		for (Snapshot const &snap : attr.sb.snapshots.items) {
			if (snap.valid && max_nr_of_leaves < snap.nr_of_leaves)
				max_nr_of_leaves = snap.nr_of_leaves;
		}
		if (attr.sb.rekeying_vba < max_nr_of_leaves - 1) {
			attr.sb.rekeying_vba++;
			_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
			if (VERBOSE_REKEYING)
				log("  secure sb: gen ", attr.curr_gen);
		} else {
			_remove_key.generate(_helper, REMOVE_KEY, REMOVE_KEY_SUCCEEDED, progress, attr.sb.previous_key.id);
			if (VERBOSE_REKEYING)
				log("  remove key ", attr.sb.previous_key.id);
		}
		break;
	}
	case REMOVE_KEY: progress |= _remove_key.execute(attr.crypto); break;
	case REMOVE_KEY_SUCCEEDED:

		attr.sb.previous_key = { };
		attr.sb.state = Superblock::NORMAL;
		_attr.out_rekeying_finished = true;
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", attr.curr_gen);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


bool Superblock_control::Secure_superblock::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		attr.sb.curr_snap().gen = attr.curr_gen;
		attr.sb.discard_disposable_snapshots();
		_sb_ciphertext.copy_all_but_key_values_from(attr.sb);
		_encrypt_key.generate(
			_helper, ENCRYPT_KEY, ENCRYPT_CURR_KEY_SUCCEEDED, progress, _sb_ciphertext.current_key.value, attr.sb.current_key.value);
		break;

	case ENCRYPT_KEY: progress |= _encrypt_key.execute(attr.trust_anchor); break;
	case ENCRYPT_CURR_KEY_SUCCEEDED:

		if (attr.sb.state == Superblock::REKEYING)
			_encrypt_key.generate(
				_helper, ENCRYPT_KEY, ENCRYPT_PREV_KEY_SUCCEEDED, progress, attr.sb.previous_key.value, _sb_ciphertext.previous_key.value);
		else {
			_sb_ciphertext.encode_to_blk(_blk);
			_write_block.generate(_helper, WRITE_BLOCK, WRITE_BLOCK_SUCCEEDED, progress, attr.sb_idx, _blk);
		}
		break;

	case ENCRYPT_PREV_KEY_SUCCEEDED:

		_sb_ciphertext.encode_to_blk(_blk);
		_write_block.generate(_helper, WRITE_BLOCK, WRITE_BLOCK_SUCCEEDED, progress, attr.sb_idx, _blk);
		break;

	case WRITE_BLOCK: progress |= _write_block.execute(attr.block_io); break;
	case WRITE_BLOCK_SUCCEEDED: _sync_block_io.generate(_helper, SYNC_BLOCK_IO, SYNC_BLOCK_IO_SUCCEEDED, progress); break;
	case SYNC_BLOCK_IO: progress |= _sync_block_io.execute(attr.block_io); break;
	case SYNC_BLOCK_IO_SUCCEEDED:
	{
		_sb_ciphertext.encode_to_blk(_blk);
		calc_hash(_blk, _hash);
		_write_sb_hash.generate(_helper, WRITE_SB_HASH, WRITE_SB_HASH_SUCCEEDED, progress, _hash);
		if (attr.sb_idx < MAX_SUPERBLOCK_INDEX)
			attr.sb_idx++;
		else
			attr.sb_idx = 0;

		_gen = attr.curr_gen;
		attr.curr_gen++;
		break;
	}
	case WRITE_SB_HASH: progress |= _write_sb_hash.execute(attr.trust_anchor); break;
	case WRITE_SB_HASH_SUCCEEDED:

		attr.sb.last_secured_generation = _gen;
		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


bool Superblock_control::Discard_snapshot::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		for (Snapshot &snap : attr.sb.snapshots.items)
			if (snap.valid && snap.gen == _attr.in_gen && snap.keep)
				snap.keep = false;

		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


bool Superblock_control::Create_snapshot::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		Snapshot &snap = attr.sb.curr_snap();
		if (snap.keep) {
			_attr.out_gen = snap.gen;
			_helper.mark_succeeded(progress);
		} else {
			snap.keep = true;
			_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		}
		break;
	}
	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED:

		_attr.out_gen = attr.sb.curr_snap().gen;
		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


bool Superblock_control::Synchronize::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		attr.sb.last_secured_generation = attr.curr_gen;
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


bool Superblock_control::Initialize::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT: _read_sb_hash.generate(_helper, READ_SB_HASH, READ_SB_HASH_SUCCEEDED, progress, _hash); break;
	case READ_SB_HASH: progress |= _read_sb_hash.execute(attr.trust_anchor); break;
	case READ_SB_HASH_SUCCEEDED:

		attr.sb_idx = 0;
		_read_block.generate(_helper, READ_BLOCK, READ_BLOCK_SUCCEEDED, progress, attr.sb_idx, _blk);
		break;

	case READ_BLOCK: progress |= _read_block.execute(attr.block_io); break;
	case READ_BLOCK_SUCCEEDED:

		if (check_hash(_blk, _hash)) {
			_sb_ciphertext.decode_from_blk(_blk);
			_gen = _sb_ciphertext.snapshots.items[_sb_ciphertext.snapshots.newest_snap_idx()].gen;
			attr.sb.copy_all_but_key_values_from(_sb_ciphertext);
			_decrypt_key.generate(_helper, DECRYPT_KEY, DECRYPT_CURR_KEY_SUCCEEDED, progress, attr.sb.current_key.value, _sb_ciphertext.current_key.value);
		} else
			if (attr.sb_idx < MAX_SUPERBLOCK_INDEX) {
				attr.sb_idx++;
				_read_block.generate(_helper, READ_BLOCK, READ_BLOCK_SUCCEEDED, progress, attr.sb_idx, _blk);
			} else
				_helper.mark_failed(progress, "superblock not found");
		break;

	case DECRYPT_KEY: progress |= _decrypt_key.execute(attr.trust_anchor); break;
	case DECRYPT_CURR_KEY_SUCCEEDED: _add_key.generate(_helper, ADD_KEY, ADD_CURR_KEY_SUCCEEDED, progress, attr.sb.current_key); break;
	case ADD_KEY: progress |= _add_key.execute(attr.crypto); break;
	case ADD_CURR_KEY_SUCCEEDED:

		if (_sb_ciphertext.state == Superblock::REKEYING)
			_decrypt_key.generate(_helper, DECRYPT_KEY, DECRYPT_PREV_KEY_SUCCEEDED, progress, attr.sb.previous_key.value, _sb_ciphertext.previous_key.value);
		else {
			attr.curr_gen = _gen + 1;
			_attr.out_sb_state = attr.sb.state;
			_helper.mark_succeeded(progress);
		}
		break;

	case DECRYPT_PREV_KEY_SUCCEEDED: _add_key.generate(_helper, ADD_KEY, ADD_PREV_KEY_SUCCEEDED, progress, attr.sb.previous_key); break;
	case ADD_PREV_KEY_SUCCEEDED:

		attr.curr_gen = _gen + 1;
		_attr.out_sb_state = attr.sb.state;
		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


bool Superblock_control::Deinitialize::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		attr.sb.last_secured_generation = attr.curr_gen;
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _remove_key.generate(_helper, REMOVE_KEY, REMOVE_CURR_KEY_SUCCEEDED, progress, attr.sb.current_key.id); break;
	case REMOVE_CURR_KEY_SUCCEEDED:

		if (attr.sb.state == Superblock::REKEYING)
			_remove_key.generate(_helper, REMOVE_KEY, REMOVE_PREV_KEY_SUCCEEDED, progress, attr.sb.previous_key.id);
		else
			_helper.mark_succeeded(progress);
		break;

	case REMOVE_KEY: progress |= _remove_key.execute(attr.crypto); break;
	case REMOVE_PREV_KEY_SUCCEEDED:

		attr.sb.state = Superblock::INVALID;
		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


Snapshots_info Superblock_control::snapshots_info() const
{
	Snapshots_info info { };
	if (_sb.valid()) {
		for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
			Snapshot const &snap { _sb.snapshots.items[idx] };
			if (snap.valid && snap.keep)
				info.generations[idx] = snap.gen;
		}
	}
	return info;
}


Superblock_info Superblock_control::sb_info() const
{
	if (!_sb.valid())
		return Superblock_info { };

	return Superblock_info {
		true, _sb.state == Superblock::REKEYING, _sb.state == Superblock::EXTENDING_FT,
		_sb.state == Superblock::EXTENDING_VBD };
}
