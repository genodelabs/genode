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

Superblock_control_request::
Superblock_control_request(Module_id src_module_id, Module_channel_id src_channel_id,
                           Type type, Request_offset client_req_offset,
                           Request_tag client_req_tag, Number_of_blocks nr_of_blks,
                           Virtual_block_address vba, bool &success,
                           bool &client_req_finished, Superblock::State &sb_state,
                           Generation &gen)
:
	Module_request { src_module_id, src_channel_id, SUPERBLOCK_CONTROL }, _type { type },
	_client_req_offset { client_req_offset }, _client_req_tag { client_req_tag },
	_nr_of_blks { nr_of_blks }, _vba { vba }, _success { success },
	_client_req_finished { client_req_finished }, _sb_state { sb_state }, _gen { gen }
{ }


char const *Superblock_control_request::type_to_string(Type type)
{
	switch (type) {
	case READ_VBA: return "read_vba";
	case WRITE_VBA: return "write_vba";
	case SYNC: return "sync";
	case INITIALIZE: return "initialize";
	case DEINITIALIZE: return "deinitialize";
	case VBD_EXTENSION_STEP: return "vbd_ext_step";
	case FT_EXTENSION_STEP: return "ft_ext_step";
	case CREATE_SNAPSHOT: return "create_snap";
	case DISCARD_SNAPSHOT: return "discard_snap";
	case INITIALIZE_REKEYING: return "init_rekeying";
	case REKEY_VBA: return "rekey_vba";
	}
	ASSERT_NEVER_REACHED;
}


void Superblock_control_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("sb control: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Superblock_control_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Superblock_control_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_gen_req_success) {
		error("superblock control: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	if (_state == SECURE_SB)
		_secure_sb_state = (Secure_sb_state)state_uint;
	else
		_state = (State)state_uint;
}


void Superblock_control_channel::
_generate_vbd_req(Virtual_block_device_request::Type type, State_uint complete_state, bool
                  &progress, Key_id key_id, Virtual_block_address vba = INVALID_VBA)
{
	if (_state == SECURE_SB)
		_secure_sb_state = SECURE_SB_REQ_GENERATED;
	else
		_state = REQ_GENERATED;

	_state = REQ_GENERATED;
	_pba = _sb.first_pba + _sb.nr_of_pbas;
	_ft.construct(_sb.free_number, _sb.free_gen, _sb.free_hash, _sb.free_max_level, _sb.free_degree, _sb.free_leaves);
	_mt.construct(_sb.meta_number, _sb.meta_gen, _sb.meta_hash, _sb.meta_max_level, _sb.meta_degree, _sb.meta_leaves);
	generate_req<Virtual_block_device_request>(
		complete_state, progress, type, _req_ptr->_client_req_offset, _req_ptr->_client_req_tag,
		_sb.last_secured_generation, *_ft, *_mt, _sb.degree, _sb.max_vba(), _sb.state == Superblock::REKEYING,
		vba, _sb.curr_snap_idx, _sb.snapshots, _sb.degree, _sb.previous_key.id, key_id,
		_curr_gen, _pba, _gen_req_success, _nr_of_leaves, _req_ptr->_nr_of_blks, _sb.rekeying_vba);
}


void Superblock_control_channel::_access_vba(Virtual_block_device_request::Type type, bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:
	{
		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (req._vba > _sb.max_vba()) {
			_mark_req_failed(progress, "VBA greater than max VBA");
			break;
		}
		if (type == Virtual_block_device_request::WRITE_VBA && _sb.curr_snap().gen != _curr_gen) {
			Snapshot &snap { _sb.curr_snap() };
			_sb.curr_snap_idx = _sb.snapshots.alloc_idx(_curr_gen, _sb.last_secured_generation);
			_sb.curr_snap() = snap;
			_sb.curr_snap().keep = false;
		}
		Key_id key_id { _sb.state == Superblock::REKEYING && req._vba >= _sb.rekeying_vba ?
			_sb.previous_key.id : _sb.current_key.id };

		_generate_vbd_req(type, ACCESS_VBA_AT_VBD_SUCCEEDED, progress, key_id, req._vba);
		if (VERBOSE_READ_VBA)
			log("read vba ", req._vba, ": snap ", _sb.curr_snap_idx, " key ", key_id, " gen ", _curr_gen);

		break;
	}
	case ACCESS_VBA_AT_VBD_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control_channel::_tree_ext_step(Superblock::State sb_state, bool verbose, String<4> tree_name, bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:
	{
		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		Physical_block_address const last_used_pba { _sb.first_pba + (_sb.nr_of_pbas - 1) };
		Number_of_blocks const nr_of_unused_pbas { MAX_PBA - last_used_pba };

		if (req._nr_of_blks > nr_of_unused_pbas) {
			_mark_req_failed(progress, "check number of unused blocks");
			break;
		}
		if (_sb.state == Superblock::NORMAL) {

			req._client_req_finished = false;
			_sb.state = sb_state;
			_sb.resizing_nr_of_pbas = req._nr_of_blks;
			_sb.resizing_nr_of_leaves = 0;
			_pba = last_used_pba + 1;
			if (verbose)
				log(tree_name, " ext init: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)_sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)_sb.resizing_nr_of_leaves);

			_start_secure_sb(progress);
			break;

		} else if (_sb.state == sb_state) {

			_pba = last_used_pba + 1;
			req._nr_of_blks = _sb.resizing_nr_of_pbas;

			if (verbose)
				log(tree_name, " ext step: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)_sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)_sb.resizing_nr_of_leaves);

			req._nr_of_blks = _sb.resizing_nr_of_pbas;
			_pba = _sb.first_pba + _sb.nr_of_pbas;
			if (tree_name == "vbd") {

				_generate_vbd_req(
					Virtual_block_device_request::EXTENSION_STEP,
					TREE_EXT_STEP_IN_TREE_SUCCEEDED, progress, _sb.current_key.id);

			} else if (tree_name == "ft") {

				_mt.construct(
					_sb.free_number, _sb.free_gen, _sb.free_hash, _sb.free_max_level, _sb.free_degree,
					_sb.free_leaves);

				_mt.construct(
					_sb.meta_number, _sb.meta_gen, _sb.meta_hash, _sb.meta_max_level, _sb.meta_degree,
					_sb.meta_leaves);

				_generate_req<Free_tree::Extension_step>(
					TREE_EXT_STEP_IN_TREE_SUCCEEDED, progress, _curr_gen, *_ft, *_mt, _pba, req._nr_of_blks);
			}
		} else
			_mark_req_failed(progress, "check superblock state");

		break;
	}
	case TREE_EXT_STEP_IN_TREE_SUCCEEDED:
	{
		if (req._nr_of_blks >= _sb.resizing_nr_of_pbas) {
			_mark_req_failed(progress, "check number of pbas");
			break;
		}
		Number_of_blocks const nr_of_added_pbas { _sb.resizing_nr_of_pbas - req._nr_of_blks };
		Physical_block_address const new_first_unused_pba { _sb.first_pba + (_sb.nr_of_pbas + nr_of_added_pbas) };
		if (_pba != new_first_unused_pba) {
			_mark_req_failed(progress, "check new first unused pba");
			break;
		}
		_sb.nr_of_pbas = _sb.nr_of_pbas + nr_of_added_pbas;
		_sb.resizing_nr_of_pbas = req._nr_of_blks;
		_sb.resizing_nr_of_leaves += _nr_of_leaves;

		if (tree_name == "vbd")
			_sb.curr_snap_idx = _sb.snapshots.newest_snap_idx();

		if (!req._nr_of_blks) {
			_sb.state = Superblock::NORMAL;
			req._client_req_finished = true;
		}
		_start_secure_sb(progress);
		break;
	}
	case SECURE_SB: _secure_sb(progress); break;
	case SECURE_SB_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control_channel::_rekey_vba(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (_sb.state != Superblock::REKEYING) {
			_mark_req_failed(progress, "check superblock state");
			break;
		}
		_generate_vbd_req(
			Virtual_block_device_request::REKEY_VBA, REKEY_VBA_AT_VBD_SUCCEEDED,
			progress, _sb.current_key.id, _sb.rekeying_vba);

		_state = REQ_GENERATED;
		if (VERBOSE_REKEYING) {
			log("rekey vba ", _sb.rekeying_vba, ":");
			log("  update vbd: keys ", _sb.previous_key.id, ",", _sb.current_key.id, " generations ", _sb.last_secured_generation, ",", _curr_gen);
		}
		break;

	case REKEY_VBA_AT_VBD_SUCCEEDED:
	{
		Number_of_leaves max_nr_of_leaves { 0 };
		for (Snapshot const &snap : _sb.snapshots.items) {
			if (snap.valid && max_nr_of_leaves < snap.nr_of_leaves)
				max_nr_of_leaves = snap.nr_of_leaves;
		}
		if (_sb.rekeying_vba < max_nr_of_leaves - 1) {
			_sb.rekeying_vba++;
			_req_ptr->_client_req_finished = false;
			_start_secure_sb(progress);
			if (VERBOSE_REKEYING)
				log("  secure sb: gen ", _curr_gen);
		} else {
			_generate_req<Crypto::Remove_key>(REMOVE_PREV_KEY_SUCCEEDED, progress, _sb.previous_key.id);
			if (VERBOSE_REKEYING)
				log("  remove key ", _sb.previous_key.id);
		}
		break;
	}
	case REMOVE_PREV_KEY_SUCCEEDED:

		_sb.previous_key = { };
		_sb.state = Superblock::NORMAL;
		_req_ptr->_client_req_finished = true;
		_start_secure_sb(progress);
		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);
		break;

	case SECURE_SB: _secure_sb(progress); break;
	case SECURE_SB_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control_channel::_start_secure_sb(bool &progress)
{
	_state = SECURE_SB;
	_secure_sb_state = STARTED;
	progress = true;
}


void Superblock_control_channel::_secure_sb(bool &progress)
{
	switch (_secure_sb_state) {
	case STARTED:

		_sb.curr_snap().gen = _curr_gen;
		_sb_ciphertext.copy_all_but_key_values_from(_sb);
		_generate_req<Trust_anchor::Encrypt_key>(
			ENCRYPT_CURR_KEY_SUCCEEDED, progress, _sb.current_key.value, _sb_ciphertext.current_key.value);
		break;

	case ENCRYPT_CURR_KEY_SUCCEEDED:

		if (_sb.state == Superblock::REKEYING)
			_generate_req<Trust_anchor::Encrypt_key>(
				ENCRYPT_PREV_KEY_SUCCEEDED, progress, _sb.previous_key.value, _sb_ciphertext.previous_key.value);
		else
			_generate_req<Block_io::Sync>(SYNC_CACHE_SUCCEEDED, progress);
		break;

	case ENCRYPT_PREV_KEY_SUCCEEDED: _generate_req<Block_io::Sync>(SYNC_CACHE_SUCCEEDED, progress); break;
	case SYNC_CACHE_SUCCEEDED:

		_sb_ciphertext.encode_to_blk(_blk);
		_generate_req<Block_io::Write>(WRITE_SB_SUCCEEDED, progress, _sb_idx, _blk);
		break;

	case WRITE_SB_SUCCEEDED: _generate_req<Block_io::Sync>(SYNC_BLK_IO_SUCCEEDED, progress); break;
	case SYNC_BLK_IO_SUCCEEDED:
	{
		_sb_ciphertext.encode_to_blk(_blk);
		calc_hash(_blk, _hash);
		_generate_req<Trust_anchor::Write_hash>(WRITE_SB_HASH_SUCCEEDED, progress, _hash);
		if (_sb_idx < MAX_SUPERBLOCK_INDEX)
			_sb_idx++;
		else
			_sb_idx = 0;

		_gen = _curr_gen;
		_curr_gen++;
		break;
	}
	case WRITE_SB_HASH_SUCCEEDED:

		_sb.last_secured_generation = _gen;
		_state = SECURE_SB_SUCCEEDED;
		break;

	default: break;
	}
}


void Superblock_control_channel::_init_rekeying(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (_sb.state != Superblock::NORMAL) {
			_mark_req_failed(progress, "check superblock state");
			break;
		}
		_sb.state = Superblock::REKEYING;
		_sb.rekeying_vba = 0;
		_sb.previous_key = _sb.current_key;
		_sb.current_key.id++;
		_generate_req<Trust_anchor::Create_key>(CREATE_KEY_SUCCEEDED, progress, _sb.current_key.value);
		break;

	case CREATE_KEY_SUCCEEDED:

		_generate_req<Crypto::Add_key>(ADD_CURR_KEY_SUCCEEDED, progress, _sb.current_key);
		if (VERBOSE_REKEYING)
			log("start rekeying:\n  update sb: keys ", _sb.previous_key.id, ",", _sb.current_key.id);
		break;

	case ADD_CURR_KEY_SUCCEEDED:

		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);
		_start_secure_sb(progress);
		break;

	case SECURE_SB: _secure_sb(progress); break;
	case SECURE_SB_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control_channel::_discard_snap(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		for (Snapshot &snap : _sb.snapshots.items)
			if (snap.valid && snap.gen == _req_ptr->_gen && snap.keep)
				snap.keep = false;

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_start_secure_sb(progress);
		break;

	case SECURE_SB: _secure_sb(progress); break;
	case SECURE_SB_SUCCEEDED:

		_req_ptr->_gen = _gen;
		_mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control_channel::_create_snap(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		if (_sb.curr_snap().keep) {
			_req_ptr->_gen = _sb.curr_snap().gen;
			_mark_req_successful(progress);
		} else {
			_sb.curr_snap().keep = true;
			_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
			_start_secure_sb(progress);
		}
		break;

	case SECURE_SB: _secure_sb(progress); break;
	case SECURE_SB_SUCCEEDED:

		_req_ptr->_gen = _gen;
		_mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control_channel::_sync(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_sb.last_secured_generation = _curr_gen;
		_start_secure_sb(progress);
		break;

	case SECURE_SB: _secure_sb(progress); break;
	case SECURE_SB_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control_request::print(Output &out) const
{
	Genode::print(out, type_to_string(_type));
	switch (_type) {
	case REKEY_VBA:
	case READ_VBA:
	case WRITE_VBA: Genode::print(out, " ", _vba); break;
	default: break;
	}
}


void Superblock_control_channel::_initialize(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_generate_req<Trust_anchor::Read_hash>(READ_SB_HASH_SUCCEEDED, progress, _hash);
		break;

	case READ_SB_HASH_SUCCEEDED:

		_sb_idx = 0;
		_generate_req<Block_io::Read>(READ_SB_SUCCEEDED, progress, _sb_idx, _blk);
		break;

	case READ_SB_SUCCEEDED:

		_sb_ciphertext.decode_from_blk(_blk);
		if (check_hash(_blk, _hash)) {
			_gen = _sb_ciphertext.snapshots.items[_sb_ciphertext.snapshots.newest_snap_idx()].gen;
			_sb.copy_all_but_key_values_from(_sb_ciphertext);
			_generate_req<Trust_anchor::Decrypt_key>(
				DECRYPT_CURR_KEY_SUCCEEDED, progress, _sb.current_key.value, _sb_ciphertext.current_key.value);
		} else
			if (_sb_idx < MAX_SUPERBLOCK_INDEX) {
				_sb_idx++;
				_generate_req<Block_io::Read>(READ_SB_SUCCEEDED, progress, _sb_idx, _blk);
			} else
				_mark_req_failed(progress, "superblock not found");
		break;

	case DECRYPT_CURR_KEY_SUCCEEDED:

		_generate_req<Crypto::Add_key>(ADD_CURR_KEY_SUCCEEDED, progress, _sb.current_key);
		break;

	case ADD_CURR_KEY_SUCCEEDED:

		if (_sb_ciphertext.state == Superblock::REKEYING)
			_generate_req<Trust_anchor::Decrypt_key>(
				DECRYPT_PREV_KEY_SUCCEEDED, progress, _sb.previous_key.value, _sb_ciphertext.previous_key.value);
		else {
			_curr_gen = _gen + 1;
			_req_ptr->_sb_state = _sb.state;
			_mark_req_successful(progress);
		}
		break;

	case DECRYPT_PREV_KEY_SUCCEEDED:

		_generate_req<Crypto::Add_key>(ADD_PREV_KEY_SUCCEEDED, progress, _sb.previous_key);
		break;

	case ADD_PREV_KEY_SUCCEEDED:

		_curr_gen = _gen + 1;
		_req_ptr->_sb_state = _sb.state;
		_mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control_channel::_deinitialize(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_sb.last_secured_generation = _curr_gen;
		_start_secure_sb(progress);
		break;

	case SECURE_SB: _secure_sb(progress); break;
	case SECURE_SB_SUCCEEDED: _generate_req<Crypto::Remove_key>(REMOVE_CURR_KEY_SUCCEEDED, progress, _sb.current_key.id); break;
	case REMOVE_CURR_KEY_SUCCEEDED:

		if (_sb.state == Superblock::REKEYING)
			_generate_req<Crypto::Remove_key>(REMOVE_CURR_KEY_SUCCEEDED, progress, _sb.previous_key.id);
		else
			_mark_req_successful(progress);
		break;

	case REMOVE_PREV_KEY_SUCCEEDED:

		_sb.state = Superblock::INVALID;
		_mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_type) {
	case Request::READ_VBA: _access_vba(Virtual_block_device_request::READ_VBA, progress); break;
	case Request::WRITE_VBA: _access_vba(Virtual_block_device_request::WRITE_VBA, progress); break;
	case Request::SYNC: _sync(progress); break;
	case Request::INITIALIZE_REKEYING: _init_rekeying(progress); break;
	case Request::REKEY_VBA: _rekey_vba(progress); break;
	case Request::VBD_EXTENSION_STEP: _tree_ext_step(Superblock::EXTENDING_VBD, VERBOSE_VBD_EXTENSION, "vbd", progress); break;
	case Request::FT_EXTENSION_STEP: _tree_ext_step(Superblock::EXTENDING_FT, VERBOSE_FT_EXTENSION, "ft", progress); break;
	case Request::CREATE_SNAPSHOT: _create_snap(progress); break;
	case Request::DISCARD_SNAPSHOT: _discard_snap(progress); break;
	case Request::INITIALIZE: _initialize(progress); break;
	case Request::DEINITIALIZE: _deinitialize (progress); break;
	}
}


void Superblock_control::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
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


void Superblock_control_channel::_request_submitted(Module_request &req)
{
	_req_ptr = static_cast<Request *>(&req);
	_state = REQ_SUBMITTED;
}


Superblock_control::Superblock_control()
{
	for (Module_channel_id id { 0 }; id < NUM_CHANNELS; id++) {
		_channels[id].construct(id, _sb, _sb_idx, _curr_gen);
		add_channel(*_channels[id]);
	}
}


Superblock_control_channel::
Superblock_control_channel(Module_channel_id id, Superblock &sb, Superblock_index &sb_idx, Generation &curr_gen)
:
	Module_channel { SUPERBLOCK_CONTROL, id }, _sb { sb }, _sb_idx { sb_idx }, _curr_gen { curr_gen }
{ }
