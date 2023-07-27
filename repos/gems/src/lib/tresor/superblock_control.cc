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

/* base includes */
#include <base/log.h>

/* tresor includes */
#include <tresor/superblock_control.h>
#include <tresor/crypto.h>
#include <tresor/block_io.h>
#include <tresor/trust_anchor.h>
#include <tresor/virtual_block_device.h>
#include <tresor/ft_resizing.h>
#include <tresor/sha256_4k_hash.h>

using namespace Tresor;


/********************************
 ** Superblock_control_request **
 ********************************/

void Superblock_control_request::create(void             *buf_ptr,
                                        size_t            buf_size,
                                        uint64_t          src_module_id,
                                        uint64_t          src_request_id,
                                        size_t            req_type,
                                        uint64_t          client_req_offset,
                                        uint64_t          client_req_tag,
                                        Number_of_blocks  nr_of_blks,
                                        uint64_t          vba)
{
	Superblock_control_request req { src_module_id, src_request_id };

	req._type = (Type)req_type;
	req._client_req_offset = client_req_offset;
	req._client_req_tag = client_req_tag;
	req._nr_of_blks = nr_of_blks;
	req._vba = vba;

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Superblock_control_request::
Superblock_control_request(Module_id         src_module_id,
                           Module_request_id src_request_id)
:
	Module_request { src_module_id, src_request_id, SUPERBLOCK_CONTROL }
{ }


char const *Superblock_control_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
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
	return "?";
}


/************************
 ** Superblock_control **
 ************************/

void Superblock_control::_mark_req_failed(Channel    &chan,
                                          bool       &progress,
                                          char const *str)
{
	error("sb control: request (", chan._request, ") failed at step \"", str, "\"");
	chan._request._success = false;
	chan._state = Channel::COMPLETED;
	progress = true;
}


void Superblock_control::_mark_req_successful(Channel &chan,
                                              bool    &progress)
{
	chan._request._success = true;
	chan._state = Channel::COMPLETED;
	progress = true;
}


Virtual_block_address Superblock_control::max_vba() const
{
	if (_sb.valid())
		return _sb.snapshots.items[_sb.curr_snap].nr_of_leaves - 1;
	else
		return 0;
}


Virtual_block_address Superblock_control::rekeying_vba() const
{
	return _sb.rekeying_vba;
}


Virtual_block_address Superblock_control::resizing_nr_of_pbas() const
{
	return _sb.resizing_nr_of_pbas;
}


void Superblock_control::_execute_read_vba(Channel          &channel,
                                           uint64_t   const job_idx,
                                           Superblock const &sb,
                                           bool             &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:
		switch (sb.state) {
		case Superblock::REKEYING: {
			Virtual_block_address const vba = channel._request._vba;

			if (vba < sb.rekeying_vba)
				channel._curr_key_plaintext.id = sb.current_key.id;
			else
				channel._curr_key_plaintext.id = sb.previous_key.id;

			break;
		}
		case Superblock::NORMAL:
		{
			Virtual_block_address const vba = channel._request._vba;
			if (vba > max_vba()) {
				channel._request._success = false;
				channel._state = Channel::State::COMPLETED;
				progress = true;
				return;
			}
			channel._curr_key_plaintext.id = sb.current_key.id;
			break;
		}
		case Superblock::EXTENDING_FT:
		case Superblock::EXTENDING_VBD:
			channel._curr_key_plaintext.id = sb.current_key.id;
			break;
		case Superblock::INVALID:
			class Superblock_not_valid { };
			throw Superblock_not_valid { };

			break;
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_VBD_RKG_READ_VBA,
			.blk_nr = channel._request._vba,
			.idx    = job_idx
		};

		channel._state = Channel::State::READ_VBA_AT_VBD_PENDING;
		progress = true;

		if (VERBOSE_READ_VBA)
			log("read vba ", channel._request._vba,
			    ": snap ", (Snapshot_index)_sb.curr_snap,
			    " key ", (Key_id)channel._curr_key_plaintext.id);

		break;
	case Channel::State::READ_VBA_AT_VBD_COMPLETED:
		channel._request._success = channel._generated_prim.succ;
		channel._state = Channel::State::COMPLETED;
		progress = true;

		break;
	default:
		break;
	}
}


void Superblock_control::_execute_write_vba(Channel         &channel,
                                            uint64_t   const job_idx,
                                            Superblock       &sb,
                                            Generation const &curr_gen,
                                            bool             &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:
		switch (sb.state) {
		case Superblock::REKEYING: {
			Virtual_block_address const vba = channel._request._vba;

			if (vba < sb.rekeying_vba)
				channel._curr_key_plaintext.id = sb.current_key.id;
			else
				channel._curr_key_plaintext.id = sb.previous_key.id;

			break;
		}
		case Superblock::NORMAL:
		{
			Virtual_block_address const vba = channel._request._vba;
			if (vba > max_vba()) {
				channel._request._success = false;
				channel._state = Channel::State::COMPLETED;
				progress = true;
				return;
			}
			channel._curr_key_plaintext.id = sb.current_key.id;
			break;
		}
		case Superblock::EXTENDING_FT:
		case Superblock::EXTENDING_VBD:
			channel._curr_key_plaintext.id = sb.current_key.id;

			break;
		case Superblock::INVALID:
			class Superblock_not_valid_write { };
			throw Superblock_not_valid_write { };

			break;
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::WRITE,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_VBD_RKG_WRITE_VBA,
			.blk_nr = channel._request._vba,
			.idx    = job_idx
		};

		channel._state = Channel::State::WRITE_VBA_AT_VBD_PENDING;
		progress = true;

		if (VERBOSE_WRITE_VBA)
			log("write vba ", channel._request._vba,
			    ": snap ", (Snapshot_index)_sb.curr_snap,
			    " key ", (Key_id)channel._curr_key_plaintext.id,
			    " gen ", curr_gen);

		break;
	case Channel::State::WRITE_VBA_AT_VBD_COMPLETED:
		if (sb.snapshots.items[sb.curr_snap].gen < curr_gen) {

			sb.curr_snap =
				sb.snapshots.idx_of_invalid_or_lowest_gen_evictable_snap(
					curr_gen, sb.last_secured_generation);

			sb.snapshots.items[sb.curr_snap] = channel._snapshots.items[0];
			sb.snapshots.items[sb.curr_snap].keep = false;
		} else if (sb.snapshots.items[sb.curr_snap].gen == curr_gen) {
			sb.snapshots.items[sb.curr_snap] = channel._snapshots.items[0];
		} else {
			class Superblock_write_vba_at_vbd { };
			throw Superblock_write_vba_at_vbd { };
		}

		channel._request._success = channel._generated_prim.succ;
		channel._state = Channel::State::COMPLETED;
		progress = true;

		break;
	default:
		break;
	}
}


void Superblock_control::_init_sb_without_key_values(Superblock const &sb_in,
                                                     Superblock       &sb_out)
{
	sb_out.state                   = sb_in.state;
	sb_out.rekeying_vba            = sb_in.rekeying_vba;
	sb_out.resizing_nr_of_pbas     = sb_in.resizing_nr_of_pbas;
	sb_out.resizing_nr_of_leaves   = sb_in.resizing_nr_of_leaves;
	sb_out.first_pba               = sb_in.first_pba;
	sb_out.nr_of_pbas              = sb_in.nr_of_pbas;
	memset(&sb_out.previous_key.value, 0, sizeof(sb_out.previous_key.value));
	sb_out.previous_key.id         = sb_in.previous_key.id;
	memset(&sb_out.current_key.value,  0, sizeof(sb_out.current_key.value));
	sb_out.current_key.id          = sb_in.current_key.id;
	sb_out.snapshots               = sb_in.snapshots;
	sb_out.last_secured_generation = sb_in.last_secured_generation;
	sb_out.curr_snap               = sb_in.curr_snap;
	sb_out.degree                  = sb_in.degree;
	sb_out.free_gen                = sb_in.free_gen;
	sb_out.free_number             = sb_in.free_number;
	sb_out.free_hash               = sb_in.free_hash;
	sb_out.free_max_level          = sb_in.free_max_level;
	sb_out.free_degree             = sb_in.free_degree;
	sb_out.free_leaves             = sb_in.free_leaves;
	sb_out.meta_gen                = sb_in.meta_gen;
	sb_out.meta_number             = sb_in.meta_number;
	sb_out.meta_hash               = sb_in.meta_hash;
	sb_out.meta_max_level          = sb_in.meta_max_level;
	sb_out.meta_degree             = sb_in.meta_degree;
	sb_out.meta_leaves             = sb_in.meta_leaves;
}


char const *Superblock_control::_state_to_step_label(Channel::State state)
{
	switch (state) {
	case Channel::TREE_EXT_STEP_IN_TREE_COMPLETED: return "tree ext step in tree";
	case Channel::SECURE_SB_COMPLETED: return "secure sb";
	default: break;
	}
	return "?";
}


bool Superblock_control::_handle_failed_generated_req(Channel &chan,
                                                      bool    &progress)
{
	if (chan._generated_prim.succ)
		return false;

	_mark_req_failed(chan, progress, _state_to_step_label(chan._state));
	return true;
}


void Superblock_control::_execute_tree_ext_step(Channel          &chan,
                                                uint64_t          chan_idx,
                                                Superblock::State  tree_ext_sb_state,
                                                bool              tree_ext_verbose,
                                                Tag               tree_ext_tag,
                                                Channel::State    tree_ext_pending_state,
                                                String<4>         tree_name,
                                                bool             &progress)
{
	Request &req { chan._request };
	switch (chan._state) {
	case Channel::SUBMITTED:
	{
		Physical_block_address const last_used_pba     { _sb.first_pba + (_sb.nr_of_pbas - 1) };
		Number_of_blocks       const nr_of_unused_pbas { MAX_PBA - last_used_pba };

		if (req._nr_of_blks > nr_of_unused_pbas) {
			_mark_req_failed(chan, progress, "check number of unused blocks");
			break;
		}
		if (_sb.state == Superblock::NORMAL) {

			req._request_finished = false;
			_sb.state = tree_ext_sb_state;
			_sb.resizing_nr_of_pbas = req._nr_of_blks;
			_sb.resizing_nr_of_leaves = 0;
			chan._pba = last_used_pba + 1;

			if (tree_ext_verbose)
				log(tree_name, " ext init: pbas ", chan._pba, "..",
				    chan._pba + (Number_of_blocks)_sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)_sb.resizing_nr_of_leaves);

			_secure_sb_init(chan, chan_idx, progress);
			break;

		} else if (_sb.state == tree_ext_sb_state) {

			chan._pba = last_used_pba + 1;
			req._nr_of_blks = _sb.resizing_nr_of_pbas;

			if (tree_ext_verbose)
				log(tree_name, " ext step: pbas ", chan._pba, "..",
				    chan._pba + (Number_of_blocks)_sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)_sb.resizing_nr_of_leaves);

			chan._generated_prim = {
				.op     = Generated_prim::READ,
				.succ   = false,
				.tg     = tree_ext_tag,
				.blk_nr = 0,
				.idx    = chan_idx
			};
			chan._state = tree_ext_pending_state;
			progress = true;
			break;

		} else {

			_mark_req_failed(chan, progress, "check superblock state");
			break;
		}
		break;
	}
	case Channel::TREE_EXT_STEP_IN_TREE_COMPLETED:
	{
		if (_handle_failed_generated_req(chan, progress))
			break;

		if (req._nr_of_blks >= _sb.resizing_nr_of_pbas) {
			_mark_req_failed(chan, progress, "check number of pbas");
			break;
		}
		Number_of_blocks       const nr_of_added_pbas     { _sb.resizing_nr_of_pbas - req._nr_of_blks };
		Physical_block_address const new_first_unused_pba { _sb.first_pba + (_sb.nr_of_pbas + nr_of_added_pbas) };

		if (chan._pba != new_first_unused_pba) {
			_mark_req_failed(chan, progress, "check new first unused pba");
			break;
		}
		_sb.nr_of_pbas = _sb.nr_of_pbas + nr_of_added_pbas;
		_sb.resizing_nr_of_pbas = req._nr_of_blks;
		_sb.resizing_nr_of_leaves += chan._nr_of_leaves;

		if (tree_name == "vbd") {

			_sb.snapshots = chan._snapshots;
			_sb.curr_snap = chan._snapshots.newest_snapshot_idx();

		} else if (tree_name == "ft") {

			_sb.free_gen       = chan._ft_root.gen;
			_sb.free_number    = chan._ft_root.pba;
			_sb.free_hash      = chan._ft_root.hash;
			_sb.free_max_level = chan._ft_max_lvl;
			_sb.free_leaves    = chan._ft_nr_of_leaves;

		} else {

			class Exception_1 { };
			throw Exception_1 { };
		}
		if (req._nr_of_blks == 0) {

			_sb.state = Superblock::NORMAL;
			req._request_finished = true;
		}
		_secure_sb_init(chan, chan_idx, progress);
		break;
	}
	case Channel::ENCRYPT_CURRENT_KEY_COMPLETED:  _secure_sb_encr_curr_key_compl(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_COMPLETED: _secure_sb_encr_prev_key_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED:           _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED:             _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED:          _secure_sb_sync_blk_io_compl(chan, chan_idx, progress); break;
	case Channel::SECURE_SB_COMPLETED:

		if (_handle_failed_generated_req(chan, progress))
			break;

		_sb.last_secured_generation = chan._generation;
		_mark_req_successful(chan, progress);
		break;

	default:
		break;
	}
}



void Superblock_control::_execute_rekey_vba(Channel  &chan,
                                            uint64_t  chan_idx,
                                            bool     &progress)
{
	Request &req { chan._request };

	switch (chan._state) {
	case Channel::SUBMITTED:

		if (_sb.state != Superblock::REKEYING) {
			_mark_req_failed(chan, progress, "check superblock state");
			break;
		}
		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_SB_CTRL_VBD_RKG_REKEY_VBA,
			.blk_nr = _sb.rekeying_vba,
			.idx    = chan_idx
		};
		chan._state = Channel::REKEY_VBA_IN_VBD_PENDING;
		progress = true;

		if (VERBOSE_REKEYING) {
			log("rekey vba ", (Virtual_block_address)_sb.rekeying_vba, ":");
			log("  update vbd: keys ", (Key_id)_sb.previous_key.id,
			    ",", (Key_id)_sb.current_key.id,
			    " generations ", (Generation)_sb.last_secured_generation,
			    ",", _curr_gen);
		}
		break;

	case Channel::REKEY_VBA_IN_VBD_COMPLETED:
	{
		if (!chan._generated_prim.succ) {
			_mark_req_failed(chan, progress, "rekey vba at vbd");
			break;
		}
		_sb.snapshots = chan._snapshots;
		Number_of_leaves max_nr_of_leaves { 0 };

		for (Snapshot const &snap : _sb.snapshots.items) {
			if (snap.valid && max_nr_of_leaves < snap.nr_of_leaves)
				max_nr_of_leaves = snap.nr_of_leaves;
		}
		if (_sb.rekeying_vba < max_nr_of_leaves - 1) {

			_sb.rekeying_vba++;
			req._request_finished = false;
			_secure_sb_init(chan, chan_idx, progress);

			if (VERBOSE_REKEYING)
				log("  secure sb: gen ", _curr_gen);

		} else {

			chan._prev_key_plaintext.id = _sb.previous_key.id;
			chan._generated_prim = {
				.op     = Generated_prim::READ,
				.succ   = false,
				.tg     = Channel::TAG_SB_CTRL_CRYPTO_REMOVE_KEY,
				.blk_nr = 0,
				.idx    = chan_idx
			};
			chan._state = Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING;
			progress = true;

			if (VERBOSE_REKEYING)
				log("  remove key ", (Key_id)chan._key_plaintext.id);
		}
		break;
	}
	case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED:

		if (!chan._generated_prim.succ) {
			_mark_req_failed(chan, progress, "remove key at crypto");
			break;
		}
		_sb.previous_key = { };
		_sb.state = Superblock::NORMAL;
		req._request_finished = true;
		_secure_sb_init(chan, chan_idx, progress);

		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);

		break;

	case Channel::ENCRYPT_CURRENT_KEY_COMPLETED:  _secure_sb_encr_curr_key_compl(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_COMPLETED: _secure_sb_encr_prev_key_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED:           _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED:             _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED:          _secure_sb_sync_blk_io_compl(chan, chan_idx, progress); break;
	case Channel::SECURE_SB_COMPLETED:

		if (!chan._generated_prim.succ) {
			_mark_req_failed(chan, progress, "secure superblock");
			break;
		}
		_sb.last_secured_generation = chan._generation;
		_mark_req_successful(chan, progress);
		break;

	default:

		break;
	}
}


void Superblock_control::_secure_sb_init(Channel  &chan,
                                         uint64_t  chan_idx,
                                         bool     &progress)
{
	_sb.snapshots.items[_sb.curr_snap].gen = _curr_gen;
	_init_sb_without_key_values(_sb, chan._sb_ciphertext);
	chan._key_plaintext = _sb.current_key;
	chan._generated_prim = {
		.op     = Generated_prim::READ,
		.succ   = false,
		.tg     = Channel::TAG_SB_CTRL_TA_ENCRYPT_KEY,
		.blk_nr = 0,
		.idx    = chan_idx
	};
	chan._state = Channel::ENCRYPT_CURRENT_KEY_PENDING;
	progress = true;
}


void Superblock_control::_secure_sb_encr_curr_key_compl(Channel  &chan,
                                                        uint64_t  chan_idx,
                                                        bool     &progress)
{
	if (!chan._generated_prim.succ) {
		_mark_req_failed(chan, progress, "encrypt current key");
		return;
	}
	switch (_sb.state) {
	case Superblock::REKEYING:

		chan._key_plaintext = _sb.previous_key;
		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_SB_CTRL_TA_ENCRYPT_KEY,
			.blk_nr = 0,
			.idx    = chan_idx
		};
		chan._state = Channel::ENCRYPT_PREVIOUS_KEY_PENDING;
		progress = true;
		break;

	default:

		chan._generated_prim = {
			.op     = Generated_prim::SYNC,
			.succ   = false,
			.tg     = Channel::TAG_SB_CTRL_CACHE,
			.blk_nr = 0,
			.idx    = chan_idx
		};
		chan._state = Channel::SYNC_CACHE_PENDING;
		progress = true;
		break;
	}
}


void Superblock_control::_secure_sb_encr_prev_key_compl(Channel  &chan,
                                                        uint64_t  chan_idx,
                                                        bool     &progress)
{
	if (!chan._generated_prim.succ) {
		_mark_req_failed(chan, progress, "encrypt previous key");
		return;
	}
	chan._generated_prim = {
		.op     = Generated_prim::SYNC,
		.succ   = false,
		.tg     = Channel::TAG_SB_CTRL_CACHE,
		.blk_nr = 0,
		.idx    = chan_idx
	};
	chan._state = Channel::SYNC_CACHE_PENDING;
	progress = true;
}


void Superblock_control::_secure_sb_sync_cache_compl(Channel  &chan,
                                                     uint64_t  chan_idx,
                                                     bool     &progress)
{
	if (!chan._generated_prim.succ) {
		_mark_req_failed(chan, progress, "sync cache");
		return;
	}
	chan._generated_prim = {
		.op     = Generated_prim::WRITE,
		.succ   = false,
		.tg     = Channel::TAG_SB_CTRL_BLK_IO_WRITE_SB,
		.blk_nr = _sb_idx,
		.idx    = chan_idx
	};
	chan._state = Channel::WRITE_SB_PENDING;
	progress = true;
}


void Superblock_control::_secure_sb_sync_blk_io_compl(Channel  &chan,
                                                      uint64_t  chan_idx,
                                                      bool     &progress)
{
		if (!chan._generated_prim.succ) {
			_mark_req_failed(chan, progress, "sync block io");
			return;
		}
		Block blk { };
		chan._sb_ciphertext.encode_to_blk(blk);
		calc_sha256_4k_hash(blk, chan._hash);

		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_SB_CTRL_TA_SECURE_SB,
			.blk_nr = 0,
			.idx    = chan_idx
		};
		chan._state = Channel::SECURE_SB_PENDING;
		if (_sb_idx < MAX_SUPERBLOCK_INDEX)
			_sb_idx++;
		else
			_sb_idx = 0;

		chan._generation = _curr_gen;
		_curr_gen++;
		progress = true;
}


void Superblock_control::_secure_sb_write_sb_compl(Channel  &chan,
                                                   uint64_t  chan_idx,
                                                   bool     &progress)
{
	if (!chan._generated_prim.succ) {
		_mark_req_failed(chan, progress, "write superblock");
		return;
	}
	chan._generated_prim = {
		.op     = Generated_prim::SYNC,
		.succ   = false,
		.tg     = Channel::TAG_SB_CTRL_BLK_IO_SYNC,
		.blk_nr = _sb_idx,
		.idx    = chan_idx
	};
	chan._state = Channel::SYNC_BLK_IO_PENDING;
	progress = true;
}


void
Superblock_control::_execute_initialize_rekeying(Channel           &chan,
                                                 uint64_t   const   chan_idx,
                                                 bool              &progress)
{
	switch (chan._state) {
	case Channel::SUBMITTED:

		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_SB_CTRL_TA_CREATE_KEY,
			.blk_nr = 0,
			.idx    = chan_idx
		};
		chan._state = Channel::CREATE_KEY_PENDING;
		progress = true;
		break;

	case Channel::CREATE_KEY_COMPLETED:

		if (!chan._generated_prim.succ) {
			_mark_req_failed(chan, progress, "create key");
			break;
		}
		if (_sb.state != Superblock::NORMAL) {
			_mark_req_failed(chan, progress, "check superblock state");
			break;
		}
		_sb.state = Superblock::REKEYING;
		_sb.rekeying_vba = 0;
		_sb.previous_key = _sb.current_key;
		_sb.current_key = {
			.value = chan._key_plaintext.value,
			.id    = _sb.previous_key.id + 1
		};
		chan._key_plaintext = _sb.current_key;
		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_SB_CTRL_CRYPTO_ADD_KEY,
			.blk_nr = 0,
			.idx    = chan_idx
		};
		chan._state = Channel::ADD_KEY_AT_CRYPTO_MODULE_PENDING;
		progress = true;

		if (VERBOSE_REKEYING) {
			log("start rekeying:");
			log("  update sb: keys ", (Key_id)_sb.previous_key.id,
			    ",", (Key_id)chan._key_plaintext.id);
		}
		break;

	case Channel::ADD_KEY_AT_CRYPTO_MODULE_COMPLETED:

		if (!chan._generated_prim.succ) {
			_mark_req_failed(chan, progress, "add key at crypto");
			break;
		}
		_secure_sb_init(chan, chan_idx, progress);

		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);

		break;

	case Channel::ENCRYPT_CURRENT_KEY_COMPLETED:  _secure_sb_encr_curr_key_compl(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_COMPLETED: _secure_sb_encr_prev_key_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED:           _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED:             _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED:          _secure_sb_sync_blk_io_compl(chan, chan_idx, progress); break;
	case Channel::SECURE_SB_COMPLETED:

		if (!chan._generated_prim.succ) {
			_mark_req_failed(chan, progress, "secure superblock");
			break;
		}
		_sb.last_secured_generation = chan._generation;
		_mark_req_successful(chan, progress);
		break;

	default:

		break;
	}
}


void Superblock_control::_execute_sync(Channel           &channel,
                                       uint64_t   const   job_idx,
                                       Superblock        &sb,
                                       Superblock_index  &sb_idx,
                                       Generation        &curr_gen,
                                       bool              &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		sb.snapshots.discard_disposable_snapshots(sb.last_secured_generation, curr_gen);
		sb.last_secured_generation = curr_gen;
		sb.snapshots.items[sb.curr_snap].gen = curr_gen;
		_init_sb_without_key_values(sb, channel._sb_ciphertext);

		channel._key_plaintext = sb.current_key;
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_ENCRYPT_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};
		channel._state = Channel::State::ENCRYPT_CURRENT_KEY_PENDING;
		progress = true;
		break;

	case Channel::State::ENCRYPT_CURRENT_KEY_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Encrypt_current_key_error { };
			throw Encrypt_current_key_error { };
		}
		switch (sb.state) {
		case Superblock::REKEYING:

			channel._key_plaintext = sb.previous_key;
			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_ENCRYPT_KEY,
				.blk_nr = 0,
				.idx    = job_idx
			};
			channel._state = Channel::State::ENCRYPT_PREVIOUS_KEY_PENDING;
			progress = true;
			break;

		default:

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::SYNC,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_CACHE,
				.blk_nr = 0,
				.idx    = job_idx
			};
			channel._state = Channel::State::SYNC_CACHE_PENDING;
			progress = true;
			break;
		}
		break;

	case Channel::State::ENCRYPT_PREVIOUS_KEY_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Encrypt_previous_key_error { };
			throw Encrypt_previous_key_error { };
		}
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::SYNC,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CACHE,
			.blk_nr = 0,
			.idx    = job_idx
		};
		channel._state = Channel::State::SYNC_CACHE_PENDING;
		progress = true;
		break;

	case Channel::State::SYNC_CACHE_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Sync_cache_error { };
			throw Sync_cache_error { };
		}
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::WRITE,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_WRITE_SB,
			.blk_nr = sb_idx,
			.idx    = job_idx
		};
		channel._state = Channel::State::WRITE_SB_PENDING;
		progress = true;
		break;

	case Channel::State::WRITE_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Write_sb_completed_error { };
			throw Write_sb_completed_error { };
		}
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::SYNC,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_SYNC,
			.blk_nr = sb_idx,
			.idx    = job_idx
		};
		channel._state = Channel::State::SYNC_BLK_IO_PENDING;
		progress = true;
		break;

	case Channel::State::SYNC_BLK_IO_COMPLETED:
	{
		if (!channel._generated_prim.succ) {
			class Sync_blk_io_completed_error { };
			throw Sync_blk_io_completed_error { };
		}
		Block blk { };
		channel._sb_ciphertext.encode_to_blk(blk);
		calc_sha256_4k_hash(blk, channel._hash);

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_SECURE_SB,
			.blk_nr = 0,
			.idx    = job_idx
		};
		channel._state = Channel::State::SECURE_SB_PENDING;

		if (sb_idx < MAX_SUPERBLOCK_INDEX)
            sb_idx = sb_idx + 1;
		else
			sb_idx = 0;

		channel._generation = curr_gen;
		curr_gen = curr_gen + 1;
		progress = true;
		break;
	}
	case Channel::State::SECURE_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Secure_sb_completed_error { };
			throw Secure_sb_completed_error { };
		}
		sb.last_secured_generation = channel._generation;
		channel._request._success = true;
		channel._state = Channel::State::COMPLETED;
		progress = true;
		break;

	default:

		break;
	}
}


void Superblock_control::_execute_initialize(Channel           &channel,
                                             uint64_t const     job_idx,
                                             Superblock        &sb,
                                             Superblock_index  &sb_idx,
                                             Generation        &curr_gen,
                                             bool              &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._sb_found = false;
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_LAST_SB_HASH,
			.blk_nr = 0,
			.idx    = job_idx
		};
		channel._state = Channel::State::MAX_SB_HASH_PENDING;
		progress = true;
		break;

	case Channel::State::MAX_SB_HASH_COMPLETED:

		channel._read_sb_idx = 0;
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_READ_SB,
			.blk_nr = channel._read_sb_idx,
			.idx    = job_idx
		};
		channel._state = Channel::State::READ_SB_PENDING;
		progress = true;
		break;

	case Channel::State::READ_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Execute_initialize_error { };
			throw Execute_initialize_error { };
		}
		if (channel._sb_ciphertext.state != Superblock::INVALID) {

			Superblock const &cipher { channel._sb_ciphertext };
			Snapshot_index const snap_index { cipher.snapshots.newest_snapshot_idx() };
			Generation const sb_generation { cipher.snapshots.items[snap_index].gen };

			if (check_sha256_4k_hash(channel._encoded_blk, channel._hash)) {
				channel._generation = sb_generation;
				channel._sb_idx     = channel._read_sb_idx;
				channel._sb_found   = true;
			}
		}
		if (channel._read_sb_idx < MAX_SUPERBLOCK_INDEX) {

			channel._read_sb_idx = channel._read_sb_idx + 1;
			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_READ_SB,
				.blk_nr = channel._read_sb_idx,
				.idx    = job_idx
			};
			channel._state = Channel::State::READ_SB_PENDING;
			progress       = true;

		} else {

			if (!channel._sb_found) {
				class Execute_initialize_sb_found_error { };
				throw Execute_initialize_sb_found_error { };
			}

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_READ_SB,
				.blk_nr = channel._sb_idx,
				.idx    = job_idx
			};

			channel._state = Channel::State::READ_CURRENT_SB_PENDING;
			progress       = true;
		}
		break;

	case Channel::State::READ_CURRENT_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Execute_initialize_read_current_sb_error { };
			throw Execute_initialize_read_current_sb_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_DECRYPT_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::DECRYPT_CURRENT_KEY_PENDING;
		progress       = true;

		break;
	case Channel::State::DECRYPT_CURRENT_KEY_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Execute_initialize_decrypt_current_key_error { };
			throw Execute_initialize_decrypt_current_key_error { };
		}

		channel._curr_key_plaintext.id = channel._sb_ciphertext.current_key.id;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CRYPTO_ADD_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING;
		progress       = true;

		break;
	case Channel::State::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Execute_add_current_key_at_crypto_error { };
			throw Execute_add_current_key_at_crypto_error { };
		}

		switch (channel._sb_ciphertext.state) {
		case Superblock::INVALID:
			class Execute_add_current_key_at_crypto_invalid_error { };
			throw Execute_add_current_key_at_crypto_invalid_error { };

			break;
		case Superblock::REKEYING:

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_DECRYPT_KEY,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::DECRYPT_PREVIOUS_KEY_PENDING;
			progress       = true;

			break;
		case Superblock::NORMAL:
		case Superblock::EXTENDING_VBD:
		case Superblock::EXTENDING_FT:

			_init_sb_without_key_values(channel._sb_ciphertext, sb);

			sb.current_key.value = channel._curr_key_plaintext.value;
			sb_idx               = channel._sb_idx;
			curr_gen             = channel._generation + 1;

			sb_idx = channel._sb_idx;
			curr_gen = channel._generation + 1;

			if (sb.free_max_level < FREE_TREE_MIN_MAX_LEVEL) {
				class Execute_add_current_key_at_crypto_max_level_error { };
				throw Execute_add_current_key_at_crypto_max_level_error { };
			}

			channel._request._sb_state = _sb.state;
			channel._request._success = true;

			channel._state = Channel::State::COMPLETED;
			progress       = true;

			break;
		}

		break;
	case Channel::State::DECRYPT_PREVIOUS_KEY_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Decrypt_previous_key_error { };
			throw Decrypt_previous_key_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CRYPTO_ADD_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING;
		progress       = true;

		break;
	case Channel::State::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Add_previous_key_at_crypto_module_error { };
			throw Add_previous_key_at_crypto_module_error { };
		}

		_init_sb_without_key_values(channel._sb_ciphertext, sb);

		sb.current_key.value  = channel._curr_key_plaintext.value;
		sb.previous_key.value = channel._prev_key_plaintext.value;

		sb_idx   = channel._sb_idx;
		curr_gen = channel._generation + 1;

		channel._request._sb_state = _sb.state;
		channel._request._success = true;

		channel._state = Channel::State::COMPLETED;
		progress       = true;

		break;
	default:
		break;
	}
}


void Superblock_control::_execute_deinitialize(Channel           &channel,
                                               uint64_t const     job_idx,
                                               Superblock        &sb,
                                               Superblock_index  &sb_idx,
                                               Generation        &curr_gen,
                                               bool              &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		sb.snapshots.discard_disposable_snapshots(sb.last_secured_generation, curr_gen);
		sb.last_secured_generation           = curr_gen;
		sb.snapshots.items[sb.curr_snap].gen = curr_gen;

		_init_sb_without_key_values(sb, channel._sb_ciphertext);
		channel._key_plaintext = sb.current_key;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_ENCRYPT_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::ENCRYPT_CURRENT_KEY_PENDING;
		progress       = true;

		break;
	case Channel::State::ENCRYPT_CURRENT_KEY_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Deinitialize_encrypt_current_key_error { };
			throw Deinitialize_encrypt_current_key_error { };
		}

		switch (sb.state) {
		case Superblock::REKEYING:
			channel._key_plaintext = sb.previous_key;

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_ENCRYPT_KEY,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::ENCRYPT_PREVIOUS_KEY_PENDING;
			progress       = true;

			break;

		default:
			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::SYNC,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_CACHE,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::SYNC_CACHE_PENDING;
			progress       = true;

			break;
		}

		break;
	case Channel::State::ENCRYPT_PREVIOUS_KEY_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_encrypt_previous_key_error { };
			throw Deinitialize_encrypt_previous_key_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::SYNC,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CACHE,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::SYNC_CACHE_PENDING;
		progress       = true;

		break;
	case Channel::State::SYNC_CACHE_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_sync_cache_error { };
			throw Deinitialize_sync_cache_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::WRITE,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_WRITE_SB,
			.blk_nr = sb_idx,
			.idx    = job_idx
		};

		channel._state = Channel::State::WRITE_SB_PENDING;
		progress       = true;

		break;
	case Channel::State::WRITE_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_write_sb_error { };
			throw Deinitialize_write_sb_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::SYNC,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_SYNC,
			.blk_nr = sb_idx,
			.idx    = job_idx
		};

		channel._state = Channel::State::SYNC_BLK_IO_PENDING;
		progress       = true;

		break;
	case Channel::State::SYNC_BLK_IO_COMPLETED:
	{
		if (!channel._generated_prim.succ) {
			class Deinitialize_sync_blk_io_error { };
			throw Deinitialize_sync_blk_io_error { };
		}
		Block blk { };
		channel._sb_ciphertext.encode_to_blk(blk);
		calc_sha256_4k_hash(blk, channel._hash);

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_SECURE_SB,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::SECURE_SB_PENDING;

		if (sb_idx < MAX_SUPERBLOCK_INDEX)
			sb_idx = sb_idx + 1;
		else
			sb_idx = 0;

		channel._generation = curr_gen;
		curr_gen = curr_gen + 1;

		progress = true;

		break;
	}
	case Channel::State::SECURE_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_secure_sb_error { };
			throw Deinitialize_secure_sb_error { };
		}

		sb.last_secured_generation = channel._generation;

		channel._request._success = true;

		channel._curr_key_plaintext.id = sb.current_key.id;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CRYPTO_REMOVE_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING;
		progress       = true;

		break;
	case Channel::State::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_remove_current_key_error { };
			throw Deinitialize_remove_current_key_error { };
		}

		switch (sb.state) {
		default:
			class Deinitialize_remove_current_key_invalid_error { };
			throw Deinitialize_remove_current_key_invalid_error { };
			break;
		case Superblock::REKEYING:

			channel._prev_key_plaintext.id = sb.previous_key.id;

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_CRYPTO_REMOVE_KEY,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING;
			progress       = true;

			break;
		case Superblock::NORMAL:
		case Superblock::EXTENDING_VBD:
		case Superblock::EXTENDING_FT:

			channel._request._success = true;

			channel._state = Channel::State::COMPLETED;
			progress       = true;

			break;
		}

		break;
	case Channel::State::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_remove_previous_key_error { };
			throw Deinitialize_remove_previous_key_error { };
		}

		sb.state = Superblock::INVALID;

		channel._request._success = true;

		channel._state = Channel::State::COMPLETED;
		progress       = true;

		break;
	default:
		break;
	}
}


bool Superblock_control::_peek_generated_request(uint8_t *buf_ptr,
                                                 size_t   buf_size)
{
	for (unsigned id = 0; id < NR_OF_CHANNELS; id++) {

		Channel &chan { _channels[id] };
		Request &req { chan._request };
		if (req._type == Request::INVALID)
			continue;

		switch (chan._state) {
		case Channel::CREATE_KEY_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::CREATE_KEY, nullptr,
				nullptr, nullptr, nullptr);

			return 1;

		case Channel::ENCRYPT_CURRENT_KEY_PENDING:
		case Channel::ENCRYPT_PREVIOUS_KEY_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::ENCRYPT_KEY,
				&chan._key_plaintext.value, nullptr, nullptr, nullptr);

			return 1;

		case Channel::DECRYPT_CURRENT_KEY_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::DECRYPT_KEY,
				nullptr, &chan._sb_ciphertext.current_key.value,
				nullptr, nullptr);

			return 1;

		case Channel::DECRYPT_PREVIOUS_KEY_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::DECRYPT_KEY,
				nullptr, &chan._sb_ciphertext.previous_key.value,
				nullptr, nullptr);

			return 1;

		case Channel::SECURE_SB_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::SECURE_SUPERBLOCK,
				nullptr, nullptr, nullptr, &chan._hash);

			return 1;

		case Channel::MAX_SB_HASH_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::GET_LAST_SB_HASH,
				nullptr, nullptr, nullptr, nullptr);

			return 1;

		case Channel::ADD_KEY_AT_CRYPTO_MODULE_PENDING:

			construct_in_buf<Crypto_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::ADD_KEY, 0, 0, (Key_id)chan._key_plaintext.id,
				&chan._key_plaintext.value, 0, 0, nullptr, nullptr);

			return 1;

		case Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING:

			construct_in_buf<Crypto_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::ADD_KEY, 0, 0, (Key_id)chan._curr_key_plaintext.id,
				&chan._curr_key_plaintext.value, 0, 0, nullptr, nullptr);

			return 1;

		case Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING:

			construct_in_buf<Crypto_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::ADD_KEY, 0, 0,
				(Key_id)chan._prev_key_plaintext.id, &chan._prev_key_plaintext.value,
				0, 0, nullptr, nullptr);

			return 1;

		case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING:

			construct_in_buf<Crypto_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::REMOVE_KEY, 0, 0,
				(Key_id)chan._prev_key_plaintext.id, nullptr,
				0, 0, nullptr, nullptr);

			return 1;

		case Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING:

			construct_in_buf<Crypto_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::REMOVE_KEY, 0, 0,
				(Key_id)chan._curr_key_plaintext.id, nullptr,
				0, 0, nullptr, nullptr);

			return 1;

		case Channel::READ_VBA_AT_VBD_PENDING:

			Virtual_block_device_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Virtual_block_device_request::READ_VBA,
				req._client_req_offset, req._client_req_tag,
				_sb.last_secured_generation,
				(addr_t)&_sb.free_number,
				(addr_t)&_sb.free_gen,
				(addr_t)&_sb.free_hash,
				_sb.free_max_level,
				_sb.free_degree,
				_sb.free_leaves,
				(addr_t)&_sb.meta_number,
				(addr_t)&_sb.meta_gen,
				(addr_t)&_sb.meta_hash,
				_sb.meta_max_level,
				_sb.meta_degree,
				_sb.meta_leaves,
				_sb.degree,
				max_vba(),
				_sb.state == Superblock::REKEYING ? 1 : 0,
				req._vba,
				_sb.curr_snap,
				&_sb.snapshots,
				_sb.degree, 0, 0,
				_curr_gen,
				chan._curr_key_plaintext.id, 0, 0);

			return 1;

		case Channel::WRITE_VBA_AT_VBD_PENDING:

			Virtual_block_device_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Virtual_block_device_request::WRITE_VBA,
				req._client_req_offset, req._client_req_tag,
				_sb.last_secured_generation,
				(addr_t)&_sb.free_number,
				(addr_t)&_sb.free_gen,
				(addr_t)&_sb.free_hash,
				_sb.free_max_level,
				_sb.free_degree,
				_sb.free_leaves,
				(addr_t)&_sb.meta_number,
				(addr_t)&_sb.meta_gen,
				(addr_t)&_sb.meta_hash,
				_sb.meta_max_level,
				_sb.meta_degree,
				_sb.meta_leaves,
				_sb.degree,
				max_vba(),
				_sb.state == Superblock::REKEYING ? 1 : 0,
				req._vba,
				_sb.curr_snap,
				&_sb.snapshots,
				_sb.degree, 0, 0,
				_curr_gen,
				chan._curr_key_plaintext.id, 0, 0);

			return 1;

		case Channel::READ_SB_PENDING:
		case Channel::READ_CURRENT_SB_PENDING:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Block_io_request::READ, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, &chan._encoded_blk,
				nullptr);

			return true;

		case Channel::SYNC_BLK_IO_PENDING:
		case Channel::SYNC_CACHE_PENDING:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Block_io_request::SYNC, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, nullptr, nullptr);

			return true;

		case Channel::WRITE_SB_PENDING:

			chan._sb_ciphertext.encode_to_blk(chan._encoded_blk);
			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Block_io_request::WRITE, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, &chan._encoded_blk,
				nullptr);

			return true;

		case Channel::REKEY_VBA_IN_VBD_PENDING:

			Virtual_block_device_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Virtual_block_device_request::REKEY_VBA,
				req._client_req_offset, req._client_req_tag,
				_sb.last_secured_generation,
				(addr_t)&_sb.free_number,
				(addr_t)&_sb.free_gen,
				(addr_t)&_sb.free_hash,
				_sb.free_max_level,
				_sb.free_degree,
				_sb.free_leaves,
				(addr_t)&_sb.meta_number,
				(addr_t)&_sb.meta_gen,
				(addr_t)&_sb.meta_hash,
				_sb.meta_max_level,
				_sb.meta_degree,
				_sb.meta_leaves,
				_sb.degree,
				max_vba(),
				_sb.state == Superblock::REKEYING ? 1 : 0,
				_sb.rekeying_vba,
				_sb.curr_snap,
				&_sb.snapshots,
				_sb.degree,
				_sb.previous_key.id,
				_sb.current_key.id,
				_curr_gen,
				chan._curr_key_plaintext.id, 0, 0);

			return 1;

		case Channel::VBD_EXT_STEP_IN_VBD_PENDING:

			Virtual_block_device_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Virtual_block_device_request::VBD_EXTENSION_STEP,
				req._client_req_offset, req._client_req_tag,
				_sb.last_secured_generation,
				(addr_t)&_sb.free_number,
				(addr_t)&_sb.free_gen,
				(addr_t)&_sb.free_hash,
				_sb.free_max_level,
				_sb.free_degree,
				_sb.free_leaves,
				(addr_t)&_sb.meta_number,
				(addr_t)&_sb.meta_gen,
				(addr_t)&_sb.meta_hash,
				_sb.meta_max_level,
				_sb.meta_degree,
				_sb.meta_leaves,
				_sb.degree,
				max_vba(),
				_sb.state == Superblock::REKEYING ? 1 : 0,
				0,
				_sb.curr_snap,
				&_sb.snapshots,
				_sb.degree,
				0,
				0,
				_curr_gen,
				0,
				_sb.first_pba + _sb.nr_of_pbas,
				_sb.resizing_nr_of_pbas);

			return 1;

		case Channel::FT_EXT_STEP_IN_FT_PENDING:

			construct_in_buf<Ft_resizing_request>(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Ft_resizing_request::FT_EXTENSION_STEP, _curr_gen,
				Type_1_node { _sb.free_number, _sb.free_gen, _sb.free_hash },
				(Tree_level_index)_sb.free_max_level,
				(Number_of_leaves)_sb.free_leaves,
				(Tree_degree)_sb.free_degree,
				(addr_t)&_sb.meta_number,
				(addr_t)&_sb.meta_gen,
				(addr_t)&_sb.meta_hash,
				(Tree_level_index)_sb.meta_max_level,
				(Tree_degree)_sb.meta_degree,
				(Number_of_leaves)_sb.meta_leaves,
				_sb.first_pba + _sb.nr_of_pbas,
				(Number_of_blocks)_sb.resizing_nr_of_pbas);

			return 1;

		default: break;
		}
	}
	return false;
}


void Superblock_control::_drop_generated_request(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_3 { };
		throw Exception_3 { };
	}
	Channel &chan { _channels[id] };
	Request &req { chan._request };
	if (req._type == Request::INVALID) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	switch (chan._state) {
	case Channel::CREATE_KEY_PENDING: chan._state = Channel::CREATE_KEY_IN_PROGRESS; break;
	case Channel::ENCRYPT_CURRENT_KEY_PENDING: chan._state = Channel::ENCRYPT_CURRENT_KEY_IN_PROGRESS; break;
	case Channel::ENCRYPT_PREVIOUS_KEY_PENDING: chan._state = Channel::ENCRYPT_PREVIOUS_KEY_IN_PROGRESS; break;
	case Channel::DECRYPT_CURRENT_KEY_PENDING: chan._state = Channel::DECRYPT_CURRENT_KEY_IN_PROGRESS; break;
	case Channel::DECRYPT_PREVIOUS_KEY_PENDING: chan._state = Channel::DECRYPT_PREVIOUS_KEY_IN_PROGRESS; break;
	case Channel::SECURE_SB_PENDING: chan._state = Channel::SECURE_SB_IN_PROGRESS; break;
	case Channel::MAX_SB_HASH_PENDING: chan._state = Channel::MAX_SB_HASH_IN_PROGRESS; break;
	case Channel::ADD_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::ADD_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::READ_VBA_AT_VBD_PENDING: chan._state = Channel::READ_VBA_AT_VBD_IN_PROGRESS; break;
	case Channel::WRITE_VBA_AT_VBD_PENDING: chan._state = Channel::WRITE_VBA_AT_VBD_IN_PROGRESS; break;
	case Channel::READ_SB_PENDING: chan._state = Channel::READ_SB_IN_PROGRESS; break;
	case Channel::READ_CURRENT_SB_PENDING: chan._state = Channel::READ_CURRENT_SB_IN_PROGRESS; break;
	case Channel::SYNC_BLK_IO_PENDING: chan._state = Channel::SYNC_BLK_IO_IN_PROGRESS; break;
	case Channel::SYNC_CACHE_PENDING: chan._state = Channel::SYNC_CACHE_IN_PROGRESS; break;
	case Channel::WRITE_SB_PENDING: chan._state = Channel::WRITE_SB_IN_PROGRESS; break;
	case Channel::REKEY_VBA_IN_VBD_PENDING: chan._state = Channel::REKEY_VBA_IN_VBD_IN_PROGRESS; break;
	case Channel::VBD_EXT_STEP_IN_VBD_PENDING: chan._state = Channel::VBD_EXT_STEP_IN_VBD_IN_PROGRESS; break;
	case Channel::FT_EXT_STEP_IN_FT_PENDING: chan._state = Channel::FT_EXT_STEP_IN_FT_IN_PROGRESS; break;
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Superblock_control::execute(bool &progress)
{
	for (unsigned idx = 0; idx < NR_OF_CHANNELS; idx++) {

		Channel &channel = _channels[idx];
		Request &request { channel._request };

		switch (request._type) {
		case Request::READ_VBA:
			_execute_read_vba(channel, idx, _sb, progress);

			break;
		case Request::WRITE_VBA:
			_execute_write_vba(channel, idx, _sb, _curr_gen, progress);

			break;
		case Request::SYNC:
			_execute_sync(channel, idx, _sb, _sb_idx, _curr_gen, progress);

			break;
		case Request::INITIALIZE_REKEYING: _execute_initialize_rekeying(channel, idx, progress); break;
		case Request::REKEY_VBA:           _execute_rekey_vba(channel, idx, progress); break;
		case Request::VBD_EXTENSION_STEP:  _execute_tree_ext_step(channel, idx, Superblock::EXTENDING_VBD, VERBOSE_VBD_EXTENSION, Channel::TAG_SB_CTRL_VBD_VBD_EXT_STEP, Channel::VBD_EXT_STEP_IN_VBD_PENDING, "vbd", progress); break;
		case Request::FT_EXTENSION_STEP:   _execute_tree_ext_step(channel, idx, Superblock::EXTENDING_FT, VERBOSE_FT_EXTENSION, Channel::TAG_SB_CTRL_FT_FT_EXT_STEP, Channel::FT_EXT_STEP_IN_FT_PENDING, "ft", progress); break;
		case Request::CREATE_SNAPSHOT:
			class Superblock_control_create_snapshot { };
			throw Superblock_control_create_snapshot { };

			break;
		case Request::DISCARD_SNAPSHOT:
			class Superblock_control_discard_snapshot { };
			throw Superblock_control_discard_snapshot { };

			break;
		case Request::INITIALIZE:
			_execute_initialize(channel, idx, _sb, _sb_idx, _curr_gen,
			                    progress);

			break;
		case Request::DEINITIALIZE:
			_execute_deinitialize (channel, idx, _sb, _sb_idx,
			                       _curr_gen, progress);

			break;
		case Request::INVALID:
			break;
		}
	}
}


void Superblock_control::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (mod_req.dst_module_id()) {
	case TRUST_ANCHOR:
	{
		Trust_anchor_request &gen_req { *static_cast<Trust_anchor_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::CREATE_KEY_IN_PROGRESS:
			chan._state = Channel::CREATE_KEY_COMPLETED;
			memcpy(&chan._key_plaintext.value, gen_req.key_plaintext_ptr(), KEY_SIZE);
			break;
		case Channel::ENCRYPT_CURRENT_KEY_IN_PROGRESS:
			chan._state = Channel::ENCRYPT_CURRENT_KEY_COMPLETED;
			memcpy(&chan._sb_ciphertext.current_key.value, gen_req.key_ciphertext_ptr(), KEY_SIZE);
			break;
		case Channel::ENCRYPT_PREVIOUS_KEY_IN_PROGRESS:
			chan._state = Channel::ENCRYPT_PREVIOUS_KEY_COMPLETED;
			memcpy(&chan._sb_ciphertext.previous_key.value, gen_req.key_ciphertext_ptr(), KEY_SIZE);
			break;
		case Channel::DECRYPT_CURRENT_KEY_IN_PROGRESS:
			chan._state = Channel::DECRYPT_CURRENT_KEY_COMPLETED;
			memcpy(&chan._curr_key_plaintext.value, gen_req.key_plaintext_ptr(), KEY_SIZE);
			break;
		case Channel::DECRYPT_PREVIOUS_KEY_IN_PROGRESS:
			chan._state = Channel::DECRYPT_PREVIOUS_KEY_COMPLETED;
			memcpy(&chan._prev_key_plaintext.value, gen_req.key_plaintext_ptr(), KEY_SIZE);
			break;
		case Channel::SECURE_SB_IN_PROGRESS: chan._state = Channel::SECURE_SB_COMPLETED; break;
		case Channel::MAX_SB_HASH_IN_PROGRESS:
			chan._state = Channel::MAX_SB_HASH_COMPLETED;
			memcpy(&chan._hash, gen_req.hash_ptr(), HASH_SIZE);
			break;
		default:
			class Exception_4 { };
			throw Exception_4 { };
		}
		break;
	}
	case CRYPTO:
	{
		Crypto_request &gen_req { *static_cast<Crypto_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::ADD_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::ADD_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		case Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		case Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		case Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		default:
			class Exception_5 { };
			throw Exception_5 { };
		}
		break;
	}
	case VIRTUAL_BLOCK_DEVICE:
	{
		Virtual_block_device_request &gen_req { *static_cast<Virtual_block_device_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::READ_VBA_AT_VBD_IN_PROGRESS: chan._state = Channel::READ_VBA_AT_VBD_COMPLETED; break;
		case Channel::WRITE_VBA_AT_VBD_IN_PROGRESS:
			chan._state = Channel::WRITE_VBA_AT_VBD_COMPLETED;
			chan._snapshots.items[0] = gen_req.snapshots_ptr()->items[gen_req.curr_snap_idx()];
			break;
		case Channel::REKEY_VBA_IN_VBD_IN_PROGRESS:
			chan._state = Channel::REKEY_VBA_IN_VBD_COMPLETED;
			chan._snapshots = *(gen_req.snapshots_ptr());
			break;
		case Channel::VBD_EXT_STEP_IN_VBD_IN_PROGRESS:
			chan._state = Channel::TREE_EXT_STEP_IN_TREE_COMPLETED;
			chan._snapshots = *(gen_req.snapshots_ptr());
			chan._pba = gen_req.pba();
			chan._request._nr_of_blks = gen_req.nr_of_pbas();
			chan._nr_of_leaves = gen_req.nr_of_leaves();
			break;
		default:
			class Exception_6 { };
			throw Exception_6 { };
		}
		break;
	}
	case FT_RESIZING:
	{
		Ft_resizing_request &gen_req { *static_cast<Ft_resizing_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::FT_EXT_STEP_IN_FT_IN_PROGRESS:
			chan._state = Channel::TREE_EXT_STEP_IN_TREE_COMPLETED;
			chan._ft_root = gen_req.ft_root();
			chan._ft_max_lvl = gen_req.ft_max_lvl();
			chan._ft_nr_of_leaves = gen_req.ft_nr_of_leaves();
			chan._pba = gen_req.pba();
			chan._request._nr_of_blks = gen_req.nr_of_pbas();
			chan._nr_of_leaves = gen_req.nr_of_leaves();
			break;
		default:
			class Exception_16 { };
			throw Exception_16 { };
		}
		break;
	}
	case BLOCK_IO:
	{
		Block_io_request &gen_req { *static_cast<Block_io_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::READ_SB_IN_PROGRESS:
			chan._sb_ciphertext.decode_from_blk(chan._encoded_blk);
			chan._state = Channel::READ_SB_COMPLETED;
			break;
		case Channel::READ_CURRENT_SB_IN_PROGRESS:
			chan._sb_ciphertext.decode_from_blk(chan._encoded_blk);
			chan._state = Channel::READ_CURRENT_SB_COMPLETED;
			break;
		case Channel::SYNC_BLK_IO_IN_PROGRESS: chan._state = Channel::SYNC_BLK_IO_COMPLETED; break;
		case Channel::SYNC_CACHE_IN_PROGRESS: chan._state = Channel::SYNC_CACHE_COMPLETED; break;
		case Channel::WRITE_SB_IN_PROGRESS: chan._state = Channel::WRITE_SB_COMPLETED; break;
		default:
			class Exception_7 { };
			throw Exception_7 { };
		}
		break;
	}
	default:
		class Exception_8 { };
		throw Exception_8 { };
	}
}


bool Superblock_control::_peek_completed_request(uint8_t *buf_ptr,
                                                 size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._request._type != Request::INVALID &&
		    channel._state == Channel::COMPLETED) {

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


void Superblock_control::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._request._type == Request::INVALID) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	if (_channels[id]._state != Channel::COMPLETED) {
		class Exception_3 { };
		throw Exception_3 { };
	}
	_channels[id]._request._type = Request::INVALID;
}


bool Superblock_control::ready_to_submit_request()
{
	for (Channel const &channel : _channels) {
		if (channel._request._type == Request::INVALID)
			return true;
	}
	return false;
}


void Superblock_control::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._request._type == Request::INVALID) {
			req.dst_request_id(id);
			_channels[id]._request = *static_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}
