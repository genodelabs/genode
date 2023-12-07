/*
 * \brief  Module for accessing and managing trees of the virtual block device
 * \author Martin Stein
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/virtual_block_device.h>
#include <tresor/hash.h>
#include <tresor/block_io.h>
#include <tresor/crypto.h>
#include <tresor/client_data.h>

using namespace Tresor;

Virtual_block_device_request::
Virtual_block_device_request(Module_id src_module_id, Module_channel_id src_chan_id, Type type,
                             Request_offset client_req_offset, Request_tag client_req_tag,
                             Generation last_secured_gen, Tree_root &ft, Tree_root &mt,
                             Tree_degree vbd_degree, Virtual_block_address vbd_highest_vba, bool rekeying,
                             Virtual_block_address vba, Snapshot_index curr_snap_idx, Snapshots &snapshots,
                             Tree_degree snap_degr, Key_id prev_key_id, Key_id curr_key_id,
                             Generation curr_gen, Physical_block_address &pba, bool &success,
                             Number_of_leaves &num_leaves, Number_of_blocks &num_pbas, Virtual_block_address rekeying_vba)
:
	Module_request { src_module_id, src_chan_id, VIRTUAL_BLOCK_DEVICE }, _type { type }, _vba { vba },
	_snapshots { snapshots }, _curr_snap_idx { curr_snap_idx }, _snap_degr { snap_degr }, _curr_gen { curr_gen },
	_curr_key_id { curr_key_id }, _prev_key_id { prev_key_id }, _ft { ft }, _mt { mt }, _vbd_degree { vbd_degree },
	_vbd_highest_vba { vbd_highest_vba }, _rekeying { rekeying }, _client_req_offset { client_req_offset },
	_client_req_tag { client_req_tag }, _last_secured_gen { last_secured_gen }, _pba { pba },
	_num_pbas { num_pbas }, _num_leaves { num_leaves }, _rekeying_vba { rekeying_vba }, _success { success }
{ }


char const *Virtual_block_device_request::type_to_string(Type op)
{
	switch (op) {
	case READ_VBA: return "read_vba";
	case WRITE_VBA: return "write_vba";
	case REKEY_VBA: return "rekey_vba";
	case EXTENSION_STEP: return "extension_step";
	}
	ASSERT_NEVER_REACHED;
}


void Virtual_block_device_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_gen_req_success) {
		error("virtual block device: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Virtual_block_device_channel::_generate_write_blk_req(bool &progress)
{
	if (_lvl) {
		_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
		_generate_req<Block_io::Write>(WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _encoded_blk);
	} else
		_generate_req<Block_io::Write>(WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _data_blk);
}


void Virtual_block_device_channel::_read_vba(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_snap_idx = req._curr_snap_idx;
		_vba = req._vba;
		_lvl = snap().max_level;
		_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, snap().pba, _encoded_blk);
		if (VERBOSE_READ_VBA)
			log("  load branch:\n    ", Branch_lvl_prefix("root: "), snap());
		break;

	case READ_BLK_SUCCEEDED:
	{
		if (!_check_and_decode_read_blk(progress))
			break;

		if (!_lvl) {
			_mark_req_successful(progress);
			break;
		}
		Type_1_node &node { _node(_lvl, req._vba) };
		if (VERBOSE_READ_VBA)
			log("    ", Branch_lvl_prefix("lvl ", _lvl, " node ", _node_idx(_lvl, req._vba), ": "), node);

		if (_lvl > 1)
			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _encoded_blk);
		else
			if (node.gen == INITIAL_GENERATION) {
				memset(&_data_blk, 0, BLOCK_SIZE);
				_generate_req<Client_data_request>(
					READ_BLK_SUCCEEDED, progress, Client_data_request::SUPPLY_PLAINTEXT_BLK,
					req._client_req_offset, req._client_req_tag, node.pba, _vba, _data_blk);
			} else
				_generate_req<Block_io::Read_client_data>(
					READ_BLK_SUCCEEDED, progress, node.pba, _vba, req._curr_key_id,
					req._client_req_tag, req._client_req_offset, _hash);
		_lvl--;
		break;
	}
	default: break;
	}
}


void Virtual_block_device_channel::_update_nodes_of_branch_of_written_vba()
{
	Request &req { *_req_ptr };
	Type_1_node &node { _node(1, _vba) };
	node = Type_1_node { _new_pbas.pbas[0], req._curr_gen, _hash };
	if (VERBOSE_WRITE_VBA)
		log("    ", Branch_lvl_prefix("lvl 1 node ", _node_idx(1, _vba), ": "), node);

	for (Tree_level_index lvl = 1; lvl < snap().max_level; lvl++) {
		Type_1_node &node { _node(lvl + 1, _vba) };
		node.pba = _new_pbas.pbas[lvl];
		node.gen = req._curr_gen;
		_t1_blks.items[lvl].encode_to_blk(_encoded_blk);
		calc_hash(_encoded_blk, node.hash);
		if (VERBOSE_WRITE_VBA)
			log("    ", Branch_lvl_prefix("lvl ", lvl + 1, " node ", _node_idx(lvl + 1, _vba), ": "), node);
	}
	snap().pba = _new_pbas.pbas[snap().max_level];
	snap().gen = req._curr_gen;
	_t1_blks.items[snap().max_level].encode_to_blk(_encoded_blk);
	calc_hash(_encoded_blk, snap().hash);
	if (VERBOSE_WRITE_VBA)
		log("    ", Branch_lvl_prefix("root: "), snap());
}


bool Virtual_block_device_channel::_check_and_decode_read_blk(bool &progress)
{
	Request &req { *_req_ptr };
	Hash *node_hash_ptr;
	if (_lvl) {
		calc_hash(_encoded_blk, _hash);
		if (_lvl < snap().max_level)
			node_hash_ptr = &_node(_lvl + 1, _vba).hash;
		else
			node_hash_ptr = &snap().hash;
	} else {
		Type_1_node &node { _node(_lvl + 1, _vba) };
		if (req._type == Request::READ_VBA) {
			if (node.gen == INITIAL_GENERATION)
				return true;
		} else
			calc_hash(_data_blk, _hash);

		node_hash_ptr = &node.hash;
	}
	if (_hash != *node_hash_ptr) {
		_mark_req_failed(progress, "check hash of read block");
		return false;
	}
	if (_lvl)
		_t1_blks.items[_lvl].decode_from_blk(_encoded_blk);

	return true;
}


Tree_node_index Virtual_block_device_channel::_node_idx(Tree_level_index lvl, Virtual_block_address vba) const
{
	return t1_node_idx_for_vba(vba, lvl, _req_ptr->_snap_degr);
}


Type_1_node &Virtual_block_device_channel::_node(Tree_level_index lvl, Virtual_block_address vba)
{
	return _t1_blks.items[lvl].nodes[_node_idx(lvl, vba)];
}


void Virtual_block_device_channel::_set_new_pbas_and_num_blks_for_alloc()
{
	Request &req { *_req_ptr };
	_num_blks = 0;
	for (Tree_level_index lvl = 0; lvl < TREE_MAX_NR_OF_LEVELS; lvl++) {
		if (lvl > snap().max_level)
			_new_pbas.pbas[lvl] = 0;
		else if (lvl == snap().max_level) {
			if (snap().gen < req._curr_gen) {
				_num_blks++;
				_new_pbas.pbas[lvl] = 0;
			} else if (snap().gen == req._curr_gen)
				_new_pbas.pbas[lvl] = snap().pba;
			else
				ASSERT_NEVER_REACHED;
		} else {
			Type_1_node const &node { _node(lvl + 1, _vba) };
			if (node.gen < req._curr_gen) {
				if (lvl == 0 && node.gen == INVALID_GENERATION)
					_new_pbas.pbas[lvl] = node.pba;
				else {
					_num_blks++;
					_new_pbas.pbas[lvl] = 0;
				}
			} else if (node.gen == req._curr_gen)
				_new_pbas.pbas[lvl] = node.pba;
			else
				ASSERT_NEVER_REACHED;
		}
	}
}


void Virtual_block_device_channel::_generate_ft_alloc_req_for_write_vba(bool &progress)
{
	for (Tree_level_index lvl = 0; lvl < TREE_MAX_NR_OF_LEVELS; lvl++)
		if (lvl > snap().max_level)
			_t1_nodes.nodes[lvl] = Type_1_node { };
		else if (lvl == snap().max_level)
			_t1_nodes.nodes[lvl] = Type_1_node { snap().pba, snap().gen, snap().hash };
		else
			_t1_nodes.nodes[lvl] = _node(lvl + 1, _vba);

	_free_gen = _req_ptr->_curr_gen;
	_generate_ft_req(ALLOC_PBAS_SUCCEEDED, progress, Free_tree_request::ALLOC_FOR_NON_RKG);
}


void Virtual_block_device_channel::_write_vba(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_snap_idx = req._curr_snap_idx;
		_vba = req._vba;
		_lvl = snap().max_level;
		_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, snap().pba, _encoded_blk);
		if (VERBOSE_WRITE_VBA)
			log("  load branch:\n    ", Branch_lvl_prefix("root: "), snap());
		break;

	case READ_BLK_SUCCEEDED:

		if (!_check_and_decode_read_blk(progress))
			break;

		if (VERBOSE_WRITE_VBA)
			log("    ", Branch_lvl_prefix("lvl ", _lvl, " node ", _node_idx(_lvl, _vba), ": "), _node(_lvl, _vba));

		if (_lvl > 1)
			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, _node(_lvl, _vba).pba, _encoded_blk);
		else {
			_set_new_pbas_and_num_blks_for_alloc();
			if (_num_blks)
				_generate_ft_alloc_req_for_write_vba(progress);
			else
				_generate_req<Block_io::Write_client_data>(
					WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[0], _vba,
					req._curr_key_id, req._client_req_tag, req._client_req_offset, _hash);
		}
		_lvl--;
		break;

	case ALLOC_PBAS_SUCCEEDED:

		if (VERBOSE_WRITE_VBA)
			log("  alloc pba", _num_blks > 1 ? "s" : "", ": ", Pba_allocation(_t1_nodes, _new_pbas));

		_generate_req<Block_io::Write_client_data>(
			WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[0], _vba, req._curr_key_id,
			req._client_req_tag, req._client_req_offset, _hash);

		break;

	case WRITE_BLK_SUCCEEDED:

		if (!_lvl)
			_update_nodes_of_branch_of_written_vba();

		if (_lvl < snap().max_level) {
			_lvl++;
			_generate_write_blk_req(progress);
		} else
		 	_mark_req_successful(progress);
		break;

	default: break;
	}
}


void Virtual_block_device_channel::_mark_req_failed(bool &progress, char const *str)
{
	error(Request::type_to_string(_req_ptr->_type), " request failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Virtual_block_device_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


bool Virtual_block_device_channel::_find_next_snap_to_rekey_vba_at(Snapshot_index &result) const
{
	bool result_valid { false };
	Request &req { *_req_ptr };
	Snapshot &curr_snap { req._snapshots.items[_snap_idx] };
	for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
		Snapshot &snap { req._snapshots.items[idx] };
		if (snap.valid && snap.contains_vba(req._vba)) {
			if (result_valid) {
				if (snap.gen < curr_snap.gen && snap.gen > req._snapshots.items[result].gen)
					result = idx;
			} else
				if (snap.gen < curr_snap.gen) {
					result = idx;
					result_valid = true;
				}
		}
	}
	return result_valid;
}


void Virtual_block_device_channel::_generate_ft_alloc_req_for_rekeying(Tree_level_index min_lvl, bool &progress)
{
	Request &req { *_req_ptr };
	ASSERT(min_lvl <= snap().max_level);
	_num_blks = 0;
	if (_first_snapshot)
		_free_gen = req._curr_gen;
	else
		_free_gen = snap().gen + 1;

	for (Tree_level_index lvl = 0; lvl < TREE_MAX_NR_OF_LEVELS; lvl++)
		if (lvl > snap().max_level) {
			_t1_nodes.nodes[lvl] = { };
			_new_pbas.pbas[lvl] = 0;
		} else if (lvl == snap().max_level) {
			_num_blks++;
			_new_pbas.pbas[lvl] = 0;
			_t1_nodes.nodes[lvl] = { snap().pba, snap().gen, snap().hash };
		} else if (lvl >= min_lvl) {
			_num_blks++;
			_new_pbas.pbas[lvl] = 0;
			_t1_nodes.nodes[lvl] = _node(lvl + 1, req._vba);
		} else {
			Type_1_node &node { _node(lvl + 1, req._vba) };
			_t1_nodes.nodes[lvl] = { _new_pbas.pbas[lvl], node.gen, node.hash};
		}

	_generate_ft_req(ALLOC_PBAS_SUCCEEDED, progress, _first_snapshot ?
		Free_tree_request::ALLOC_FOR_RKG_CURR_GEN_BLKS :
		Free_tree_request::ALLOC_FOR_RKG_OLD_GEN_BLKS);
}


void Virtual_block_device_channel::_rekey_vba(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_vba = req._vba;
		_snap_idx = req._snapshots.newest_snap_idx();
		_first_snapshot = true;
		_lvl = snap().max_level;
		_old_pbas.pbas[_lvl] = snap().pba;
		_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, snap().pba, _encoded_blk);
		if (VERBOSE_REKEYING)
			log("    snapshot ", _snap_idx, ":\n      load branch:\n        ",
			    Branch_lvl_prefix("root: "), snap());
		break;

	case READ_BLK_SUCCEEDED:

		if (!_check_and_decode_read_blk(progress))
			break;

		if (_lvl) {
			Type_1_node &node { _node(_lvl, req._vba) };
			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("lvl ", _lvl, " node ", _node_idx(_lvl, req._vba), ": "), node);

			if (!_first_snapshot && _old_pbas.pbas[_lvl - 1] == node.pba) {

				/* skip rest of this branch as it was rekeyed while rekeying the vba at another snap */
				_generate_ft_alloc_req_for_rekeying(_lvl, progress);
				if (VERBOSE_REKEYING)
					log("        [node already rekeyed at pba ", _new_pbas.pbas[_lvl - 1], "]");

			} else if (_lvl == 1 && node.gen == INITIAL_GENERATION) {

				/* skip yet unused leaf node because the lib will read its data as 0 regardless of the key */
				_generate_ft_alloc_req_for_rekeying(_lvl - 1, progress);
				if (VERBOSE_REKEYING)
					log("        [node needs no rekeying]");

			} else {
				_lvl--;
				_old_pbas.pbas[_lvl] = node.pba;
				_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _lvl ? _encoded_blk : _data_blk);
			}
		} else {
			_generate_req<Crypto::Decrypt>(
				DECRYPT_LEAF_DATA_SUCCEEDED, progress, req._prev_key_id, _old_pbas.pbas[_lvl], _data_blk);
			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("leaf data: "), _data_blk);
		}
		break;

	case DECRYPT_LEAF_DATA_SUCCEEDED:

		_generate_ft_alloc_req_for_rekeying(_lvl, progress);
		if (VERBOSE_REKEYING)
			log("      re-encrypt leaf data: plaintext ", _data_blk, " hash ", hash(_data_blk));
		break;

	case ALLOC_PBAS_SUCCEEDED:

		if (VERBOSE_REKEYING)
			log("      alloc pba", _num_blks > 1 ? "s" : "", ": ", Pba_allocation { _t1_nodes, _new_pbas });

		if (_lvl) {
			if (VERBOSE_REKEYING)
				log("      update branch:");

			_lvl--;
			_state = WRITE_BLK_SUCCEEDED;
			progress = true;
			if (_lvl)
				_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
		} else
			_generate_req<Crypto::Encrypt>(
				ENCRYPT_LEAF_DATA_SUCCEEDED, progress, req._curr_key_id, _new_pbas.pbas[0], _data_blk);
		break;

	case ENCRYPT_LEAF_DATA_SUCCEEDED:

		_generate_write_blk_req(progress);
		if (VERBOSE_REKEYING)
			log("      update branch:\n        ", Branch_lvl_prefix("leaf data: "), _data_blk);
		break;

	case WRITE_BLK_SUCCEEDED:

		if (_lvl < snap().max_level) {
			Type_1_node &node { _node(_lvl + 1, req._vba) };
			node.pba = _new_pbas.pbas[_lvl];
			calc_hash(_lvl ? _encoded_blk : _data_blk, node.hash);
			_lvl++;
			_generate_write_blk_req(progress);
			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("lvl ", _lvl, " node ", _node_idx(_lvl + 1, req._vba), ": "), node);
		} else {
			snap().pba = _new_pbas.pbas[_lvl];
			calc_hash(_encoded_blk, snap().hash);
			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("root: "), snap());

			Snapshot_index snap_idx;
			if (_find_next_snap_to_rekey_vba_at(snap_idx)) {
				_snap_idx = snap_idx;
				_first_snapshot = false;
				_lvl = snap().max_level;
				if (_old_pbas.pbas[_lvl] == snap().pba)
					progress = true;
				else {
					_old_pbas.pbas[_lvl] = snap().pba;
					_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, snap().pba, _encoded_blk);
					if (VERBOSE_REKEYING)
						log("    snapshot ", _snap_idx, ":\n      load branch:\n        ",
						    Branch_lvl_prefix("root: "), snap());
				}
			} else
				_mark_req_successful(progress);
		}
		break;

	default: break;
	}
}


void Virtual_block_device_channel::_add_new_root_lvl_to_snap()
{
	Request &req { *_req_ptr };
	Snapshot_index old_idx { _snap_idx };
	Snapshot_index &idx { _snap_idx };
	Snapshot *snap { req._snapshots.items };
	ASSERT(snap[idx].max_level < TREE_MAX_LEVEL);
	Tree_level_index new_lvl { snap[old_idx].max_level + 1 };
	_t1_blks.items[new_lvl] = { };
	_t1_blks.items[new_lvl].nodes[0] = { snap[idx].pba, snap[idx].gen, snap[idx].hash };

	if (snap[idx].gen < req._curr_gen) {
		idx = req._snapshots.alloc_idx(req._curr_gen, req._last_secured_gen);
		if (VERBOSE_VBD_EXTENSION)
			log("  new snap ", idx);
	}
	snap[idx] = {
		{ }, alloc_pba_from_range(req._pba, req._num_pbas), req._curr_gen, snap[old_idx].nr_of_leaves,
		new_lvl, true, 0, false };

	if (VERBOSE_VBD_EXTENSION)
		log("  update snap ", idx, " ", snap[idx], "\n  update lvl ", new_lvl, " child 0 ", _t1_blks.items[new_lvl].nodes[0]);
}


void Virtual_block_device_channel::_add_new_branch_to_snap(Tree_level_index mount_lvl, Tree_node_index mount_node_idx)
{
	Request &req { *_req_ptr };
	req._num_leaves = 0;
	_lvl = mount_lvl;
	if (mount_lvl > 1)
		for (Tree_level_index lvl { 1 }; lvl < mount_lvl; lvl++)
			_t1_blks.items[lvl] = { };

	if (!req._num_pbas)
		return;

	for (Tree_level_index lvl { mount_lvl }; lvl > 0; lvl--) {
		_lvl = lvl;
		Tree_node_index node_idx { lvl == mount_lvl ? mount_node_idx : 0 };
		auto try_add_node_at_lvl_and_node_idx = [&] () {
			if (!req._num_pbas)
				return false;

			Type_1_node &node { _t1_blks.items[lvl].nodes[node_idx] };
			node = { alloc_pba_from_range(req._pba, req._num_pbas), INITIAL_GENERATION, { } };
			if (VERBOSE_VBD_EXTENSION)
				log("  update lvl ", lvl, " node ", node_idx, " ", node);

			return true;
		};
		if (lvl > 1) {
			if (!try_add_node_at_lvl_and_node_idx())
				return;
		} else
			for (; node_idx < req._snap_degr; node_idx++) {
				if (!try_add_node_at_lvl_and_node_idx())
					return;

				req._num_leaves++;
			}
	}
}


void Virtual_block_device_channel::_set_new_pbas_identical_to_curr_pbas()
{
	for (Tree_level_index lvl { 0 }; lvl < TREE_MAX_NR_OF_LEVELS; lvl++)
		if (lvl > snap().max_level)
			_new_pbas.pbas[lvl] = 0;
		else if (lvl == snap().max_level)
			_new_pbas.pbas[lvl] = snap().pba;
		else
			_new_pbas.pbas[lvl] = _node(lvl + 1, _vba).pba;
}


void Virtual_block_device_channel::_generate_ft_alloc_req_for_resizing(Tree_level_index min_lvl, bool &progress)
{
	Request &req { *_req_ptr };
	Snapshot &snap { req._snapshots.items[_snap_idx] };
	if (min_lvl > snap.max_level) {
		_mark_req_failed(progress, "check parent lvl for alloc");
		return;
	}
	_num_blks = 0;
	_free_gen = req._curr_gen;
	for (Tree_level_index lvl = 0; lvl < TREE_MAX_NR_OF_LEVELS; lvl++) {

		if (lvl > snap.max_level) {
			_new_pbas.pbas[lvl] = 0;
			_t1_nodes.nodes[lvl] = { };
		} else if (lvl == snap.max_level) {
			_num_blks++;
			_new_pbas.pbas[lvl] = 0;
			_t1_nodes.nodes[lvl] = { snap.pba, snap.gen, snap.hash };
		} else {
			Type_1_node &node { _node(lvl + 1, _vba) };
			if (lvl >= min_lvl) {
				_num_blks++;
				_new_pbas.pbas[lvl] = 0;
				_t1_nodes.nodes[lvl] = node;
			} else {
				/*
				 * FIXME This is done only because the Free Tree module would otherwise get stuck. It is normal that
				 * the lowest levels have PBA 0 when creating the new branch stopped at an inner node because of a
				 * depleted PBA contingent. As soon as the strange behavior in the Free Tree module has been fixed,
				 * the whole 'if' statement can be removed.
				 */
				if (!node.pba) {
					_new_pbas.pbas[lvl] = INVALID_PBA;
					_t1_nodes.nodes[lvl] = { INVALID_PBA, node.gen, node.hash };
				} else {
					_new_pbas.pbas[lvl] = node.pba;
					_t1_nodes.nodes[lvl] = node;
				}
			}
		}
	}
	_generate_ft_req(ALLOC_PBAS_SUCCEEDED, progress, Free_tree_request::ALLOC_FOR_NON_RKG);
}


void Virtual_block_device_channel::_request_submitted(Module_request &req)
{
	_req_ptr = static_cast<Request *>(&req);
	_state = SUBMITTED;
}


void Virtual_block_device_channel::_extension_step(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case State::SUBMITTED:

		req._num_leaves = 0;
		_snap_idx = req._snapshots.newest_snap_idx();
		_vba = snap().nr_of_leaves;
		_lvl = snap().max_level;
		_old_pbas.pbas[_lvl] = snap().pba;
		if (_vba <= tree_max_max_vba(req._snap_degr, snap().max_level)) {
			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, snap().pba, _encoded_blk);
			if (VERBOSE_VBD_EXTENSION)
				log("  read lvl ", _lvl, " parent snap ", _snap_idx, " ", snap());
		} else {
			_add_new_root_lvl_to_snap();
			_add_new_branch_to_snap(req._snapshots.items[_snap_idx].max_level, 1);
			_set_new_pbas_identical_to_curr_pbas();
			_generate_write_blk_req(progress);
			if (VERBOSE_VBD_EXTENSION)
				log("  write 1 lvl ", _lvl, " pba ", _new_pbas.pbas[_lvl]);
		}
		break;

	case READ_BLK_SUCCEEDED:
	{
		_t1_blks.items[_lvl].decode_from_blk(_encoded_blk);
		if (_lvl == snap().max_level) {
			if (!check_hash(_encoded_blk, snap().hash)) {
				_mark_req_failed(progress, "check root node hash");
				break;
			}
		} else {
			if (!check_hash(_encoded_blk, _node(_lvl + 1, _vba).hash)) {
				_mark_req_failed(progress, "check inner node hash");
				break;
			}
		}
		Tree_node_index node_idx { _node_idx(_lvl, _vba) };
		if (_lvl > 1) {
			Type_1_node &node { _node(_lvl, _vba) };
			if (node.valid()) {
				_old_pbas.pbas[_lvl - 1] = node.pba;
				_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _encoded_blk);
				if (VERBOSE_VBD_EXTENSION)
					log("  read lvl ", _lvl - 1, " parent lvl ", _lvl, " node ", node_idx, " ", node);
				_lvl--;
			} else {
				_add_new_branch_to_snap(_lvl, node_idx);
				_generate_ft_alloc_req_for_resizing(_lvl, progress);
			}
		} else {
			_add_new_branch_to_snap(_lvl, node_idx);
			_generate_ft_alloc_req_for_resizing(_lvl, progress);
		}
		break;
	}
	case ALLOC_PBAS_SUCCEEDED:
	{
		if (VERBOSE_VBD_EXTENSION) {
			log("  allocated ", _num_blks, " pbas");
			for (Tree_level_index lvl { 0 }; lvl < snap().max_level; lvl++)
				log("    lvl ", lvl, " ", _t1_nodes.nodes[lvl], " -> pba ", _new_pbas.pbas[lvl]);

			log("  write 1 lvl ", _lvl, " pba ", _new_pbas.pbas[_lvl]);
		}
		_generate_write_blk_req(progress);
		break;
	}
	case WRITE_BLK_SUCCEEDED:
	{
		if (_lvl < snap().max_level) {
			_lvl++;
			Type_1_node &node { _node(_lvl, _vba) };
			calc_hash(_encoded_blk, node.hash);
			node.pba = _new_pbas.pbas[_lvl - 1];
			_generate_write_blk_req(progress);
			if (VERBOSE_VBD_EXTENSION)
				log("  update lvl ", _lvl, " node ", _node_idx(_lvl, _vba), " ", node, "\n  write 2 lvl ", _lvl, " pba ", _new_pbas.pbas[_lvl]);
		} else {
			Snapshot &old_snap { snap() };
			if (snap().gen < req._curr_gen) {
				_snap_idx = req._snapshots.alloc_idx(req._curr_gen, req._last_secured_gen);
				if (VERBOSE_VBD_EXTENSION)
					log("  new snap ", _snap_idx);
			}
			Number_of_leaves num_leaves { old_snap.nr_of_leaves + req._num_leaves };
			snap() = { { }, _new_pbas.pbas[_lvl], req._curr_gen, num_leaves, old_snap.max_level, true, 0, false };
			calc_hash(_encoded_blk, snap().hash);
			_mark_req_successful(progress);
			if (VERBOSE_VBD_EXTENSION)
				log("  update snap ", _snap_idx, " ", snap());
		}
		break;
	}
	default: break;
	}
}


void Virtual_block_device_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_type) {
	case Request::READ_VBA: _read_vba(progress); break;
	case Request::WRITE_VBA: _write_vba(progress); break;
	case Request::REKEY_VBA: _rekey_vba(progress); break;
	case Request::EXTENSION_STEP: _extension_step(progress); break;
	}
}


void Virtual_block_device::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


void Virtual_block_device_channel::_generate_ft_req(State complete_state, bool progress, Free_tree_request::Type type)
{
	Request &req { *_req_ptr };
	_generate_req<Free_tree_request>(
		complete_state, progress, type, req._ft, req._mt, req._snapshots, req._last_secured_gen, req._curr_gen,
		_free_gen, _num_blks, _new_pbas, _t1_nodes, req._snapshots.items[_snap_idx].max_level, _vba, req._vbd_degree,
		req._vbd_highest_vba, req._rekeying, req._prev_key_id, req._curr_key_id, req._rekeying_vba, *(Physical_block_address*)0,
		*(Number_of_blocks*)0);
}


Virtual_block_device::Virtual_block_device()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}
