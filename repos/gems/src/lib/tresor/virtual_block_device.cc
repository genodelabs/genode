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

using namespace Tresor;

bool Virtual_block_device::Read_vba::_check_and_decode_read_blk(bool &progress)
{
	Hash const *node_hash_ptr;
	calc_hash(_blk, _hash);
	if (_lvl) {
		if (_lvl < _attr.in_snap.max_level)
			node_hash_ptr = &_t1_blks.node(_attr.in_vba, _lvl + 1, _attr.in_vbd_degree).hash;
		else
			node_hash_ptr = &_attr.in_snap.hash;
	} else {
		Type_1_node &node = _t1_blks.node(_attr.in_vba, _lvl + 1, _attr.in_vbd_degree);
		if (node.gen == INITIAL_GENERATION)
			return true;

		node_hash_ptr = &node.hash;
	}
	if (_hash != *node_hash_ptr) {
		_helper.mark_failed(progress, "check hash of read block");
		return false;
	}
	if (_lvl)
		_t1_blks.items[_lvl].decode_from_blk(_blk);

	return true;
}


bool Virtual_block_device::Read_vba::execute(Client_data_interface &client_data, Block_io &block_io, Crypto &crypto)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_lvl = _attr.in_snap.max_level;
		_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, _attr.in_snap.pba, _blk);
		if (VERBOSE_READ_VBA)
			log("  load branch:\n    ", Branch_lvl_prefix("root: "), _attr.in_snap);
		break;

	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case READ_BLK_SUCCEEDED:
	{
		if (!_check_and_decode_read_blk(progress))
			break;

		if (!_lvl) {
			if (VERBOSE_READ_VBA)
				log("    ", Branch_lvl_prefix("ciphertext: "), _blk, " ", hash(_blk));

			_decrypt_block.generate(
				_helper, DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED, progress, _attr.in_key_id, _new_pbas.pbas[_lvl], _blk);
			break;
		}
		Type_1_node &node { _t1_blks.node(_attr.in_vba, _lvl, _attr.in_vbd_degree) };
		if (VERBOSE_READ_VBA)
			log("    ", Branch_lvl_prefix("lvl ", _lvl, " node ", tree_node_index(_attr.in_vba, _lvl, _attr.in_vbd_degree), ": "), node);

		_lvl--;
		_new_pbas.pbas[_lvl] = node.pba;
		if (_lvl)
			_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _blk);
		else
			if (node.gen == INITIAL_GENERATION) {
				memset(&_blk, 0, BLOCK_SIZE);
				_helper.state = DECRYPT_BLOCK_SUCCEEDED;
				progress = true;
			} else
				_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _blk);
		break;
	}
	case DECRYPT_BLOCK: progress |= _decrypt_block.execute(crypto); break;
	case DECRYPT_BLOCK_SUCCEEDED:

		if (VERBOSE_READ_VBA)
			log("    ", Branch_lvl_prefix("plaintext:  "), _blk, " ", hash(_blk));

		client_data.supply_data(
			{_attr.in_client_req_offset, _attr.in_client_req_tag, _new_pbas.pbas[_lvl], _attr.in_vba, _blk});

		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


void Virtual_block_device::Rekey_vba::_start_alloc_pbas(bool &progress, Free_tree::Allocate_pbas::Application application)
{
	_alloc_pbas.generate(
		_helper, ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED, progress, _attr.in_out_ft, _attr.in_out_mt, _attr.in_out_snapshots,
		_attr.in_last_secured_gen, _attr.in_curr_gen, _free_gen, _num_blks, _new_pbas, _t1_nodes,
		_attr.in_out_snapshots.items[_snap_idx].max_level, _attr.in_vba, _attr.in_vbd_degree, _attr.in_vbd_highest_vba, true,
		_attr.in_prev_key_id, _attr.in_curr_key_id, _attr.in_vba, application);
}


void Virtual_block_device::Rekey_vba::_generate_write_blk_req(bool &progress)
{
	if (_lvl) {
		_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
		_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _encoded_blk);
	} else
		_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _data_blk);
}


void Virtual_block_device::Rekey_vba::_generate_ft_alloc_req_for_rekeying(Tree_level_index min_lvl, bool &progress)
{
	Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
	ASSERT(min_lvl <= snap.max_level);
	_num_blks = 0;
	if (_first_snapshot)
		_free_gen = _attr.in_curr_gen;
	else
		_free_gen = snap.gen + 1;

	for (Tree_level_index lvl = 0; lvl < TREE_MAX_NR_OF_LEVELS; lvl++)
		if (lvl > snap.max_level) {
			_t1_nodes.nodes[lvl] = { };
			_new_pbas.pbas[lvl] = 0;
		} else if (lvl == snap.max_level) {
			_num_blks++;
			_new_pbas.pbas[lvl] = 0;
			_t1_nodes.nodes[lvl] = { snap.pba, snap.gen, snap.hash };
		} else if (lvl >= min_lvl) {
			_num_blks++;
			_new_pbas.pbas[lvl] = 0;
			_t1_nodes.nodes[lvl] = _t1_blks.node(_attr.in_vba, lvl + 1, _attr.in_vbd_degree);
		} else {
			Type_1_node &node { _t1_blks.node(_attr.in_vba, lvl + 1, _attr.in_vbd_degree) };
			_t1_nodes.nodes[lvl] = { _new_pbas.pbas[lvl], node.gen, node.hash};
		}

	_start_alloc_pbas(progress, _first_snapshot ?
		Free_tree::Allocate_pbas::REKEYING_IN_CURRENT_GENERATION :
		Free_tree::Allocate_pbas::REKEYING_IN_OLDER_GENERATION);
}


bool Virtual_block_device::Rekey_vba::_check_and_decode_read_blk(bool &progress)
{
	Hash const *node_hash_ptr;
	if (_lvl) {
		calc_hash(_encoded_blk, _hash);
		Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
		if (_lvl < snap.max_level)
			node_hash_ptr = &_t1_blks.node(_attr.in_vba, _lvl + 1, _attr.in_vbd_degree).hash;
		else
			node_hash_ptr = &snap.hash;
	} else {
		Type_1_node &node { _t1_blks.node(_attr.in_vba, _lvl + 1, _attr.in_vbd_degree) };
		calc_hash(_data_blk, _hash);
		node_hash_ptr = &node.hash;
	}
	if (_hash != *node_hash_ptr) {
		_helper.mark_failed(progress, "check hash of read block");
		return false;
	}
	if (_lvl)
		_t1_blks.items[_lvl].decode_from_blk(_encoded_blk);

	return true;
}


bool Virtual_block_device::Rekey_vba::execute(Block_io &block_io, Crypto &crypto, Free_tree &free_tree, Meta_tree &meta_tree)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		_snap_idx = _attr.in_out_snapshots.newest_snap_idx();
		Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
		_first_snapshot = true;
		_lvl = snap.max_level;
		_old_pbas.pbas[_lvl] = snap.pba;
		_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, snap.pba, _encoded_blk);
		if (VERBOSE_REKEYING)
			log("    snapshot ", _snap_idx, ":\n      load branch:\n        ",
			    Branch_lvl_prefix("root: "), snap);
		break;
	}
	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case READ_BLK_SUCCEEDED:

		if (!_check_and_decode_read_blk(progress))
			break;

		if (_lvl) {
			Type_1_node &node { _t1_blks.node(_attr.in_vba, _lvl, _attr.in_vbd_degree) };
			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("lvl ", _lvl, " node ", tree_node_index(_attr.in_vba, _lvl, _attr.in_vbd_degree), ": "), node);

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
				_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, node.pba, _lvl ? _encoded_blk : _data_blk);
			}
		} else {
			_decrypt_block.generate(_helper, DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED, progress, _attr.in_prev_key_id, _old_pbas.pbas[_lvl], _data_blk);
			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("leaf data: "), _data_blk);
		}
		break;

	case DECRYPT_BLOCK: progress |= _decrypt_block.execute(crypto); break;
	case DECRYPT_BLOCK_SUCCEEDED:

		_generate_ft_alloc_req_for_rekeying(_lvl, progress);
		if (VERBOSE_REKEYING)
			log("      re-encrypt leaf data: plaintext ", _data_blk, " hash ", hash(_data_blk));
		break;

	case ALLOC_PBAS: progress |= _alloc_pbas.execute(free_tree, block_io, meta_tree); break;
	case ALLOC_PBAS_SUCCEEDED:

		if (VERBOSE_REKEYING)
			log("      alloc pba", _num_blks > 1 ? "s" : "", ": ", Pba_allocation { _t1_nodes, _new_pbas });

		if (_lvl) {
			if (VERBOSE_REKEYING)
				log("      update branch:");

			_lvl--;
			_helper.state = WRITE_BLK_SUCCEEDED;
			progress = true;
			if (_lvl)
				_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
		} else
			_encrypt_block.generate(
				_helper, ENCRYPT_BLOCK, ENCRYPT_BLOCK_SUCCEEDED, progress, _attr.in_curr_key_id, _new_pbas.pbas[0], _data_blk);
		break;

	case ENCRYPT_BLOCK: progress |= _encrypt_block.execute(crypto); break;
	case ENCRYPT_BLOCK_SUCCEEDED:

		_generate_write_blk_req(progress);
		if (VERBOSE_REKEYING)
			log("      update branch:\n        ", Branch_lvl_prefix("leaf data: "), _data_blk);
		break;

	case WRITE_BLK: progress |= _write_block.execute(block_io); break;
	case WRITE_BLK_SUCCEEDED:
	{
		Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
		if (_lvl < snap.max_level) {
			Type_1_node &node { _t1_blks.node(_attr.in_vba, _lvl + 1, _attr.in_vbd_degree) };
			node.pba = _new_pbas.pbas[_lvl];
			calc_hash(_lvl ? _encoded_blk : _data_blk, node.hash);
			_lvl++;
			_generate_write_blk_req(progress);
			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("lvl ", _lvl, " node ", tree_node_index(_attr.in_vba, _lvl + 1, _attr.in_vbd_degree), ": "), node);
		} else {
			snap.pba = _new_pbas.pbas[_lvl];
			calc_hash(_encoded_blk, snap.hash);
			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("root: "), snap);

			Snapshot_index snap_idx;
			if (_find_next_snap_to_rekey_vba_at(snap_idx)) {
				_snap_idx = snap_idx;
				Snapshot &next_snap = _attr.in_out_snapshots.items[_snap_idx];
				_first_snapshot = false;
				_lvl = next_snap.max_level;
				if (_old_pbas.pbas[_lvl] == next_snap.pba)
					progress = true;
				else {
					_old_pbas.pbas[_lvl] = next_snap.pba;
					_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, next_snap.pba, _encoded_blk);
					if (VERBOSE_REKEYING)
						log("    snapshot ", _snap_idx, ":\n      load branch:\n        ",
						    Branch_lvl_prefix("root: "), next_snap);
				}
			} else
				_helper.mark_succeeded(progress);
		}
		break;
	}
	default: break;
	}
	return progress;
}


bool Virtual_block_device::Rekey_vba::_find_next_snap_to_rekey_vba_at(Snapshot_index &result) const
{
	bool result_valid { false };
	Snapshot &curr_snap { _attr.in_out_snapshots.items[_snap_idx] };
	for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
		Snapshot const &snap { _attr.in_out_snapshots.items[idx] };
		if (snap.valid && snap.contains_vba(_attr.in_vba)) {
			if (result_valid) {
				if (snap.gen < curr_snap.gen && snap.gen > _attr.in_out_snapshots.items[result].gen)
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


void Virtual_block_device::Write_vba::_update_nodes_of_branch_of_written_vba()
{
	Type_1_node &node { _t1_blks.node(_attr.in_vba, 1, _attr.in_vbd_degree) };
	node = Type_1_node { _new_pbas.pbas[0], _attr.in_curr_gen, _hash };
	if (VERBOSE_WRITE_VBA)
		log("    ", Branch_lvl_prefix("lvl 1 node ", tree_node_index(_attr.in_vba, 1, _attr.in_vbd_degree), ": "), node);

	for (Tree_level_index lvl = 1; lvl < _attr.in_out_snap.max_level; lvl++) {
		Type_1_node &node { _t1_blks.node(_attr.in_vba, lvl + 1, _attr.in_vbd_degree) };
		node.pba = _new_pbas.pbas[lvl];
		node.gen = _attr.in_curr_gen;
		_t1_blks.items[lvl].encode_to_blk(_encoded_blk);
		calc_hash(_encoded_blk, node.hash);
		if (VERBOSE_WRITE_VBA)
			log("    ", Branch_lvl_prefix("lvl ", lvl + 1, " node ", tree_node_index(_attr.in_vba, lvl + 1, _attr.in_vbd_degree), ": "), node);
	}
	_attr.in_out_snap.pba = _new_pbas.pbas[_attr.in_out_snap.max_level];
	_attr.in_out_snap.gen = _attr.in_curr_gen;
	_t1_blks.items[_attr.in_out_snap.max_level].encode_to_blk(_encoded_blk);
	calc_hash(_encoded_blk, _attr.in_out_snap.hash);
	if (VERBOSE_WRITE_VBA)
		log("    ", Branch_lvl_prefix("root: "), _attr.in_out_snap);
}


bool Virtual_block_device::Write_vba::_check_and_decode_read_blk(bool &progress)
{
	Hash *node_hash_ptr;
	if (_lvl) {
		calc_hash(_encoded_blk, _hash);
		if (_lvl < _attr.in_out_snap.max_level)
			node_hash_ptr = &_t1_blks.node(_attr.in_vba, _lvl + 1, _attr.in_vbd_degree).hash;
		else
			node_hash_ptr = &_attr.in_out_snap.hash;
	} else {
		Type_1_node &node { _t1_blks.node(_attr.in_vba, _lvl + 1, _attr.in_vbd_degree) };
		calc_hash(_data_blk, _hash);
		node_hash_ptr = &node.hash;
	}
	if (_hash != *node_hash_ptr) {
		_helper.mark_failed(progress, "check hash of read block");
		return false;
	}
	if (_lvl)
		_t1_blks.items[_lvl].decode_from_blk(_encoded_blk);

	return true;
}


void Virtual_block_device::Write_vba::_set_new_pbas_and_num_blks_for_alloc()
{
	_num_blks = 0;
	for (Tree_level_index lvl = 0; lvl < TREE_MAX_NR_OF_LEVELS; lvl++) {
		if (lvl > _attr.in_out_snap.max_level)
			_new_pbas.pbas[lvl] = 0;
		else if (lvl == _attr.in_out_snap.max_level) {
			if (_attr.in_out_snap.gen < _attr.in_curr_gen) {
				_num_blks++;
				_new_pbas.pbas[lvl] = 0;
			} else if (_attr.in_out_snap.gen == _attr.in_curr_gen)
				_new_pbas.pbas[lvl] = _attr.in_out_snap.pba;
			else
				ASSERT_NEVER_REACHED;
		} else {
			Type_1_node const &node { _t1_blks.node(_attr.in_vba, lvl + 1, _attr.in_vbd_degree) };
			if (node.gen < _attr.in_curr_gen) {
				if (lvl == 0 && node.gen == INVALID_GENERATION)
					_new_pbas.pbas[lvl] = node.pba;
				else {
					_num_blks++;
					_new_pbas.pbas[lvl] = 0;
				}
			} else if (node.gen == _attr.in_curr_gen)
				_new_pbas.pbas[lvl] = node.pba;
			else
				ASSERT_NEVER_REACHED;
		}
	}
}


bool Virtual_block_device::Write_vba::execute(Client_data_interface &client_data, Block_io &block_io, Free_tree &free_tree, Meta_tree &meta_tree, Crypto &crypto)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_lvl = _attr.in_out_snap.max_level;
		_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, _attr.in_out_snap.pba, _encoded_blk);
		if (VERBOSE_WRITE_VBA)
			log("  load branch:\n    ", Branch_lvl_prefix("root: "), _attr.in_out_snap);
		break;

	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case READ_BLK_SUCCEEDED:
	{
		if (!_check_and_decode_read_blk(progress))
			break;

		Type_1_node &node = _t1_blks.node(_attr.in_vba, _lvl, _attr.in_vbd_degree);
		if (VERBOSE_WRITE_VBA)
			log("    ", Branch_lvl_prefix("lvl ", _lvl, " node ", tree_node_index(_attr.in_vba, _lvl, _attr.in_vbd_degree), ": "), node);

		if (_lvl > 1)
			_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, node.pba, _encoded_blk);
		else {
			_set_new_pbas_and_num_blks_for_alloc();
			if (_num_blks)
				_generate_ft_alloc_req_for_write_vba(progress);
			else {
				client_data.obtain_data({_attr.in_client_req_offset, _attr.in_client_req_tag, _new_pbas.pbas[0], _attr.in_vba, _data_blk});
				_encrypt_block.generate(
					_helper, ENCRYPT_BLOCK, ENCRYPT_BLOCK_SUCCEEDED, progress, _attr.in_curr_key_id, _new_pbas.pbas[0], _data_blk);
			}
		}
		_lvl--;
		break;
	}
	case ALLOC_PBAS: progress |= _alloc_pbas.execute(free_tree, block_io, meta_tree); break;
	case ALLOC_PBAS_SUCCEEDED:

		if (VERBOSE_WRITE_VBA)
			log("  alloc pba", _num_blks > 1 ? "s" : "", ": ", Pba_allocation(_t1_nodes, _new_pbas));

		client_data.obtain_data({_attr.in_client_req_offset, _attr.in_client_req_tag, _new_pbas.pbas[0], _attr.in_vba, _data_blk});
		_encrypt_block.generate(
			_helper, ENCRYPT_BLOCK, ENCRYPT_BLOCK_SUCCEEDED, progress, _attr.in_curr_key_id, _new_pbas.pbas[0], _data_blk);
		break;

	case ENCRYPT_BLOCK: progress |= _encrypt_block.execute(crypto); break;
	case ENCRYPT_BLOCK_SUCCEEDED:

		calc_hash(_data_blk, _hash);
		_generate_write_blk_req(progress);
		break;

	case WRITE_BLK: progress |= _write_block.execute(block_io); break;
	case WRITE_BLK_SUCCEEDED:

		if (!_lvl)
			_update_nodes_of_branch_of_written_vba();

		if (_lvl < _attr.in_out_snap.max_level) {
			_lvl++;
			_generate_write_blk_req(progress);
		} else
		 	_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


void Virtual_block_device::Write_vba::_generate_ft_alloc_req_for_write_vba(bool &progress)
{
	for (Tree_level_index lvl = 0; lvl < TREE_MAX_NR_OF_LEVELS; lvl++)
		if (lvl > _attr.in_out_snap.max_level)
			_t1_nodes.nodes[lvl] = Type_1_node { };
		else if (lvl == _attr.in_out_snap.max_level)
			_t1_nodes.nodes[lvl] = Type_1_node { _attr.in_out_snap.pba, _attr.in_out_snap.gen, _attr.in_out_snap.hash };
		else
			_t1_nodes.nodes[lvl] = _t1_blks.node(_attr.in_vba, lvl + 1, _attr.in_vbd_degree);

	_free_gen = _attr.in_curr_gen;
	_alloc_pbas.generate(
		_helper, ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED, progress, _attr.in_out_ft, _attr.in_out_mt, _attr.in_snapshots,
		_attr.in_last_secured_gen, _attr.in_curr_gen, _free_gen, _num_blks, _new_pbas, _t1_nodes,
		_attr.in_out_snap.max_level, _attr.in_vba, _attr.in_vbd_degree, _attr.in_vbd_highest_vba, _attr.in_rekeying,
		_attr.in_prev_key_id, 0, _attr.in_rekeying_vba, Free_tree::Allocate_pbas::NON_REKEYING);
}


void Virtual_block_device::Write_vba::_generate_write_blk_req(bool &progress)
{
	if (_lvl) {
		_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
		_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _encoded_blk);
	} else
		_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _data_blk);
}


bool Virtual_block_device::Extend_tree::execute(Block_io &block_io, Free_tree &free_tree, Meta_tree &meta_tree)
{
	bool progress = false;
	switch (_helper.state) {
	case State::INIT:
	{
		_attr.out_num_leaves = 0;
		_snap_idx = _attr.in_out_snapshots.newest_snap_idx();
		Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
		_vba = snap.nr_of_leaves;
		_lvl = snap.max_level;
		_old_pbas.pbas[_lvl] = snap.pba;
		if (_vba <= tree_max_max_vba(_attr.in_snap_degr, snap.max_level)) {
			_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, snap.pba, _encoded_blk);
			if (VERBOSE_VBD_EXTENSION)
				log("  read lvl ", _lvl, " parent snap ", _snap_idx, " ", snap);
		} else {
			if (!_add_new_root_lvl_to_snap()) {
				_helper.mark_failed(progress, "failed to add new root level to snapshot");
				break;
			}
			_add_new_branch_to_snap(_attr.in_out_snapshots.items[_snap_idx].max_level, 1);
			_set_new_pbas_identical_to_curr_pbas();
			_generate_write_blk_req(progress);
			if (VERBOSE_VBD_EXTENSION)
				log("  write 1 lvl ", _lvl, " pba ", _new_pbas.pbas[_lvl]);
		}
		break;
	}
	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case READ_BLK_SUCCEEDED:
	{
		_t1_blks.items[_lvl].decode_from_blk(_encoded_blk);
		Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
		if (_lvl == snap.max_level) {
			if (!check_hash(_encoded_blk, snap.hash)) {
				_helper.mark_failed(progress, "check root node hash");
				break;
			}
		} else {
			if (!check_hash(_encoded_blk, _t1_blks.node(_vba, _lvl + 1, _attr.in_vbd_degree).hash)) {
				_helper.mark_failed(progress, "check inner node hash");
				break;
			}
		}
		Tree_node_index node_idx { tree_node_index(_vba, _lvl, _attr.in_vbd_degree) };
		if (_lvl > 1) {
			Type_1_node &node { _t1_blks.node(_vba, _lvl, _attr.in_vbd_degree) };
			if (node.valid()) {
				_old_pbas.pbas[_lvl - 1] = node.pba;
				_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, node.pba, _encoded_blk);
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
	case ALLOC_PBAS: progress |= _alloc_pbas.execute(free_tree, block_io, meta_tree); break;
	case ALLOC_PBAS_SUCCEEDED:
	{
		if (VERBOSE_VBD_EXTENSION) {
			Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
			log("  allocated ", _num_blks, " pbas");
			for (Tree_level_index lvl { 0 }; lvl < snap.max_level; lvl++)
				log("    lvl ", lvl, " ", _t1_nodes.nodes[lvl], " -> pba ", _new_pbas.pbas[lvl]);

			log("  write 1 lvl ", _lvl, " pba ", _new_pbas.pbas[_lvl]);
		}
		_generate_write_blk_req(progress);
		break;
	}
	case WRITE_BLK: progress |= _write_block.execute(block_io); break;
	case WRITE_BLK_SUCCEEDED:
	{
		Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
		if (_lvl < snap.max_level) {
			_lvl++;
			Type_1_node &node { _t1_blks.node(_vba, _lvl, _attr.in_vbd_degree) };
			calc_hash(_encoded_blk, node.hash);
			node.pba = _new_pbas.pbas[_lvl - 1];
			_generate_write_blk_req(progress);
			if (VERBOSE_VBD_EXTENSION)
				log("  update lvl ", _lvl, " node ", tree_node_index(_vba, _lvl, _attr.in_vbd_degree), " ", node,
				    "\n  write 2 lvl ", _lvl, " pba ", _new_pbas.pbas[_lvl]);
		} else {
			if (snap.gen < _attr.in_curr_gen) {
				_snap_idx = _attr.in_out_snapshots.alloc_idx(_attr.in_curr_gen, _attr.in_last_secured_gen);
				if (VERBOSE_VBD_EXTENSION)
					log("  new snap ", _snap_idx);
			}
			Snapshot &new_snap = _attr.in_out_snapshots.items[_snap_idx];
			Number_of_leaves num_leaves { snap.nr_of_leaves + _attr.out_num_leaves };
			new_snap = { { }, _new_pbas.pbas[_lvl], _attr.in_curr_gen, num_leaves, snap.max_level, true, 0, false };
			calc_hash(_encoded_blk, new_snap.hash);
			_helper.mark_succeeded(progress);
			if (VERBOSE_VBD_EXTENSION)
				log("  update snap ", _snap_idx, " ", new_snap);
		}
		break;
	}
	default: break;
	}
	return progress;
}


bool Virtual_block_device::Extend_tree::_add_new_root_lvl_to_snap()
{
	Snapshot_index old_idx { _snap_idx };
	Snapshot_index &idx { _snap_idx };
	Snapshot *snap { _attr.in_out_snapshots.items };
	if (snap[idx].max_level >= TREE_MAX_MAX_LEVEL)
		return false;

	Tree_level_index new_lvl { snap[old_idx].max_level + 1 };
	_t1_blks.items[new_lvl] = { };
	_t1_blks.items[new_lvl].nodes[0] = { snap[idx].pba, snap[idx].gen, snap[idx].hash };

	if (snap[idx].gen < _attr.in_curr_gen) {
		idx = _attr.in_out_snapshots.alloc_idx(_attr.in_curr_gen, _attr.in_last_secured_gen);
		if (VERBOSE_VBD_EXTENSION)
			log("  new snap ", idx);
	}
	snap[idx] = {
		{ }, alloc_pba_from_range(_attr.in_out_first_pba, _attr.in_out_num_pbas), _attr.in_curr_gen, snap[old_idx].nr_of_leaves,
		new_lvl, true, 0, false };

	if (VERBOSE_VBD_EXTENSION)
		log("  update snap ", idx, " ", snap[idx], "\n  update lvl ", new_lvl, " child 0 ", _t1_blks.items[new_lvl].nodes[0]);

	return true;
}


void Virtual_block_device::Extend_tree::_add_new_branch_to_snap(Tree_level_index mount_lvl, Tree_node_index mount_node_idx)
{
	_attr.out_num_leaves = 0;
	_lvl = mount_lvl;
	if (mount_lvl > 1)
		for (Tree_level_index lvl { 1 }; lvl < mount_lvl; lvl++)
			_t1_blks.items[lvl] = { };

	if (!_attr.in_out_num_pbas)
		return;

	for (Tree_level_index lvl { mount_lvl }; lvl > 0; lvl--) {
		_lvl = lvl;
		Tree_node_index node_idx { lvl == mount_lvl ? mount_node_idx : 0 };
		auto try_add_node_at_lvl_and_node_idx = [&] () {
			if (!_attr.in_out_num_pbas)
				return false;

			Type_1_node &node { _t1_blks.items[lvl].nodes[node_idx] };
			node = { alloc_pba_from_range(_attr.in_out_first_pba, _attr.in_out_num_pbas), INITIAL_GENERATION, { } };
			if (VERBOSE_VBD_EXTENSION)
				log("  update lvl ", lvl, " node ", node_idx, " ", node);

			return true;
		};
		if (lvl > 1) {
			if (!try_add_node_at_lvl_and_node_idx())
				return;
		} else
			for (; node_idx < _attr.in_snap_degr; node_idx++) {
				if (!try_add_node_at_lvl_and_node_idx())
					return;

				_attr.out_num_leaves++;
			}
	}
}


void Virtual_block_device::Extend_tree::_set_new_pbas_identical_to_curr_pbas()
{
	Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
	for (Tree_level_index lvl { 0 }; lvl < TREE_MAX_NR_OF_LEVELS; lvl++)
		if (lvl > snap.max_level)
			_new_pbas.pbas[lvl] = 0;
		else if (lvl == snap.max_level)
			_new_pbas.pbas[lvl] = snap.pba;
		else
			_new_pbas.pbas[lvl] = _t1_blks.node(_vba, lvl + 1, _attr.in_vbd_degree).pba;
}


void Virtual_block_device::Extend_tree::_generate_write_blk_req(bool &progress)
{
	if (_lvl) {
		_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
		_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _encoded_blk);
	} else
		_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _data_blk);
}


void Virtual_block_device::Extend_tree::_generate_ft_alloc_req_for_resizing(Tree_level_index min_lvl, bool &progress)
{
	Snapshot &snap = _attr.in_out_snapshots.items[_snap_idx];
	if (min_lvl > snap.max_level) {
		_helper.mark_failed(progress, "check parent lvl for alloc");
		return;
	}
	_num_blks = 0;
	_free_gen = _attr.in_curr_gen;
	for (Tree_level_index lvl = 0; lvl < TREE_MAX_NR_OF_LEVELS; lvl++) {

		if (lvl > snap.max_level) {
			_new_pbas.pbas[lvl] = 0;
			_t1_nodes.nodes[lvl] = { };
		} else if (lvl == snap.max_level) {
			_num_blks++;
			_new_pbas.pbas[lvl] = 0;
			_t1_nodes.nodes[lvl] = { snap.pba, snap.gen, snap.hash };
		} else {
			Type_1_node &node { _t1_blks.node(_vba, lvl + 1, _attr.in_vbd_degree) };
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
	_alloc_pbas.generate(
		_helper, ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED, progress, _attr.in_out_ft, _attr.in_out_mt, _attr.in_out_snapshots,
		_attr.in_last_secured_gen, _attr.in_curr_gen, _free_gen, _num_blks, _new_pbas, _t1_nodes,
		snap.max_level, _vba, _attr.in_vbd_degree, _attr.in_vbd_highest_vba, _attr.in_rekeying,
		_attr.in_prev_key_id, 0, _attr.in_rekeying_vba, Free_tree::Allocate_pbas::NON_REKEYING);
}
