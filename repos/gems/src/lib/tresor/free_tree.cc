/*
 * \brief  Module for doing VBD COW allocations on the free tree
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
#include <tresor/free_tree.h>
#include <tresor/hash.h>

using namespace Tresor;

bool Free_tree::Allocate_pbas::_can_alloc_pba_of(Type_2_node &node)
{
	if (node.pba == 0 || node.pba == INVALID_PBA || node.free_gen > _attr.in_last_secured_gen)
		return false;

	if (!node.reserved)
		return true;

	if (_attr.in_rekeying && node.last_key_id == _attr.in_prev_key_id && node.last_vba < _attr.in_rekeying_vba)
		return true;

	for (Snapshot const &snap : _attr.in_snapshots.items)
		if (snap.valid && node.free_gen > snap.gen && node.alloc_gen < snap.gen + 1)
			return false;

	return true;
}


void Free_tree::Allocate_pbas::_alloc_pba_of(Type_2_node &t2_node)
{
	Tree_level_index vbd_lvl { 0 };
	for (; vbd_lvl <= _attr.in_max_lvl && _attr.in_out_new_blocks.pbas[vbd_lvl]; vbd_lvl++);

	Virtual_block_address node_min_vba { vbd_node_min_vba(_vbd_degree_log_2, vbd_lvl, _attr.in_vba) };
	_attr.in_out_new_blocks.pbas[vbd_lvl] = t2_node.pba;
	t2_node.alloc_gen = _attr.in_old_blocks.nodes[vbd_lvl].gen;
	t2_node.free_gen = _attr.in_free_gen;
	Virtual_block_address rkg_vba { _attr.in_rekeying_vba };
	switch (_attr.in_application) {
	case NON_REKEYING:

		t2_node.reserved = true;
		t2_node.pba = _attr.in_old_blocks.nodes[vbd_lvl].pba;
		t2_node.last_vba = node_min_vba;
		if (_attr.in_rekeying) {
			if (_attr.in_vba < rkg_vba)
				t2_node.last_key_id = _attr.in_curr_key_id;
			else
				t2_node.last_key_id = _attr.in_prev_key_id;
		} else
			t2_node.last_key_id = _attr.in_curr_key_id;
		break;

	case REKEYING_IN_CURRENT_GENERATION:

		t2_node.reserved = false;
		t2_node.pba = _attr.in_old_blocks.nodes[vbd_lvl].pba;
		t2_node.last_vba = node_min_vba;
		t2_node.last_key_id = _attr.in_prev_key_id;
		break;

	case REKEYING_IN_OLDER_GENERATION:
	{
		t2_node.reserved = true;
		Virtual_block_address node_max_vba { vbd_node_max_vba(_vbd_degree_log_2, vbd_lvl, _attr.in_vba) };
		if (rkg_vba < node_max_vba && rkg_vba < _attr.in_vbd_max_vba) {
			t2_node.last_key_id = _attr.in_prev_key_id;
			t2_node.last_vba = rkg_vba + 1;
		} else if (rkg_vba == node_max_vba || rkg_vba == _attr.in_vbd_max_vba) {
			t2_node.last_key_id = _attr.in_curr_key_id;
			t2_node.last_vba = node_min_vba;
		} else
			ASSERT_NEVER_REACHED;
		break;
	}
	default: ASSERT_NEVER_REACHED;
	}
}


void Free_tree::Allocate_pbas::_traverse_curr_node(bool &progress)
{
	if (_lvl) {
		Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
		if (t1_node.pba)
			_read_block.generate(_helper, READ_BLK, SEEK_DOWN, progress, t1_node.pba, _blk);
		else {
			_helper.state = SEEK_LEFT_OR_UP;
			progress = true;
		}
	} else {
		Type_2_node &t2_node { _t2_blk.nodes[_node_idx[_lvl]] };
		if (_num_pbas < _attr.in_num_required_pbas && _can_alloc_pba_of(t2_node)) {
			if (_apply_allocation)
				_alloc_pba_of(t2_node);
			_num_pbas++;
		}
		_helper.state = SEEK_LEFT_OR_UP;
		progress = true;
	}
}


void Free_tree::Allocate_pbas::_start_tree_traversal(bool &progress)
{
	_num_pbas = 0;
	_lvl = _attr.in_out_ft.max_lvl;
	_node_idx[_lvl] = 0;
	_t1_blks[_lvl].nodes[_node_idx[_lvl]] = _attr.in_out_ft.t1_node();
	_read_block.generate(_helper, READ_BLK, SEEK_DOWN, progress, _attr.in_out_ft.pba, _blk);
}


bool Free_tree::Allocate_pbas::execute(Block_io &block_io, Meta_tree &meta_tree)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_vbd_degree_log_2 = log2<Tree_degree_log_2>(_attr.in_vbd_degree);
		_apply_allocation = false;
		_start_tree_traversal(progress);
		break;

	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case SEEK_DOWN:
	{
		if (!check_hash(_blk, _t1_blks[_lvl].nodes[_node_idx[_lvl]].hash)) {
			_helper.mark_failed(progress, "hash mismatch");
			break;
		}
		_lvl--;
		_node_idx[_lvl] = _attr.in_out_ft.degree - 1;
		if (_lvl)
			_t1_blks[_lvl].decode_from_blk(_blk);
		else
			_t2_blk.decode_from_blk(_blk);
		_traverse_curr_node(progress);
		break;
	}
	case SEEK_LEFT_OR_UP:

		if (_lvl < _attr.in_out_ft.max_lvl) {
			if (_node_idx[_lvl] && _num_pbas < _attr.in_num_required_pbas) {
				_node_idx[_lvl]--;
				_traverse_curr_node(progress);
			} else {
				_lvl++;
				Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
				if (_apply_allocation)
					if (t1_node.is_volatile(_attr.in_curr_gen)) {
						_helper.state = ALLOC_PBA_SUCCEEDED;
						progress = true;
					} else
						_allocate_pba.generate(_helper, ALLOC_PBA, ALLOC_PBA_SUCCEEDED, progress, _attr.in_out_mt, _attr.in_curr_gen, t1_node.pba);
				else {
					_helper.state = SEEK_LEFT_OR_UP;
					progress = true;
				}
			}
		} else {
			if (_apply_allocation) {
				_attr.in_out_ft.t1_node(_t1_blks[_lvl].nodes[_node_idx[_lvl]]);
				_helper.mark_succeeded(progress);
			} else {
				if (_num_pbas < _attr.in_num_required_pbas)
					_helper.mark_failed(progress, "not enough free pbas");
				else {
					_apply_allocation = true;
					_start_tree_traversal(progress);
				}
			}
		}
		break;

	case ALLOC_PBA: progress |= _allocate_pba.execute(meta_tree, block_io); break;
	case ALLOC_PBA_SUCCEEDED:
	{
		if (_lvl > 1)
			_t1_blks[_lvl - 1].encode_to_blk(_blk);
		else
			_t2_blk.encode_to_blk(_blk);
		Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
		t1_node.gen = _attr.in_curr_gen;
		calc_hash(_blk, t1_node.hash);
		_write_block.generate(_helper, WRITE_BLK, SEEK_LEFT_OR_UP, progress, t1_node.pba, _blk);
		break;
	}
	case WRITE_BLK: progress |= _write_block.execute(block_io); break;
	default: break;
	}
	return progress;
}


void Free_tree::Extend_tree::_generate_write_blk_req(bool &progress)
{
	if (_lvl > 1)
		_t1_blks[_lvl].encode_to_blk(_blk);
	else
		_t2_blk.encode_to_blk(_blk);

	_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _blk);
	if (VERBOSE_FT_EXTENSION)
		log("  lvl ", _lvl, " write to pba ", _new_pbas.pbas[_lvl]);
}


bool Free_tree::Extend_tree::_add_new_root_lvl()
{
	if (_attr.in_out_ft.max_lvl >= TREE_MAX_LEVEL)
		return false;

	_attr.in_out_ft.max_lvl++;
	_t1_blks[_attr.in_out_ft.max_lvl] = { };
	_t1_blks[_attr.in_out_ft.max_lvl].nodes[0] = _attr.in_out_ft.t1_node();
	_new_pbas.pbas[_attr.in_out_ft.max_lvl] = alloc_pba_from_range(_attr.in_out_first_pba, _attr.in_out_num_pbas);
	_attr.in_out_ft.t1_node({ _new_pbas.pbas[_attr.in_out_ft.max_lvl], _attr.in_curr_gen });
	if (VERBOSE_FT_EXTENSION)
		log("  set root: ", _attr.in_out_ft, "\n  set lvl ", _attr.in_out_ft.max_lvl, " node 0: ",
		    _t1_blks[_attr.in_out_ft.max_lvl].nodes[0]);

	return true;
}


void Free_tree::Extend_tree::_add_new_branch_at(Tree_level_index dst_lvl, Tree_node_index dst_node_idx)
{
	_num_leaves = 0;
	_lvl = dst_lvl;
	if (dst_lvl > 1) {
		for (Tree_level_index lvl = 1; lvl < dst_lvl; lvl++) {
			if (lvl > 1)
				_t1_blks[lvl] = Type_1_node_block { };
			else
				_t2_blk = Type_2_node_block { };

			if (VERBOSE_FT_EXTENSION)
				log("  reset lvl ", lvl);
		}
	}
	for (; _lvl && _attr.in_out_num_pbas; _lvl--) {
		Tree_node_index node_idx = (_lvl == dst_lvl) ? dst_node_idx : 0;
		if (_lvl > 1) {
			_new_pbas.pbas[_lvl - 1] = alloc_pba_from_range(_attr.in_out_first_pba, _attr.in_out_num_pbas);
			_t1_blks[_lvl].nodes[node_idx] = { _new_pbas.pbas[_lvl - 1], _attr.in_curr_gen };
			if (VERBOSE_FT_EXTENSION)
				log("  set _lvl d ", _lvl, " node ", node_idx, ": ", _t1_blks[_lvl].nodes[node_idx]);

		} else {
			for (; node_idx < _attr.in_out_ft.degree && _attr.in_out_num_pbas; node_idx++) {
				_t2_blk.nodes[node_idx] = { alloc_pba_from_range(_attr.in_out_first_pba, _attr.in_out_num_pbas) };
				_num_leaves++;
				if (VERBOSE_FT_EXTENSION)
					log("  set _lvl e ", _lvl, " node ", node_idx, ": ", _t2_blk.nodes[node_idx]);
			}
		}
	}
	if (!_lvl)
		_lvl = 1;
}


bool Free_tree::Extend_tree::execute(Block_io &block_io, Meta_tree &meta_tree)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_num_leaves = 0;
		_vba = _attr.in_out_ft.num_leaves;
		_old_pbas = { };
		_old_generations = { };
		_new_pbas = { };
		_lvl = _attr.in_out_ft.max_lvl;
		_old_pbas.pbas[_lvl] = _attr.in_out_ft.pba;
		_old_generations.items[_lvl] = _attr.in_out_ft.gen;
		if (_vba <= tree_max_max_vba(_attr.in_out_ft.degree, _attr.in_out_ft.max_lvl)) {

			_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, _attr.in_out_ft.pba, _blk);
			if (VERBOSE_FT_EXTENSION)
				log("  root (", _attr.in_out_ft, "): load to lvl ", _lvl);
		} else {
			if (!_add_new_root_lvl()) {
				_helper.mark_failed(progress, "failed to add new root level to tree");
				break;
			}
			_add_new_branch_at(_attr.in_out_ft.max_lvl, 1);
			_generate_write_blk_req(progress);
			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", _attr.in_curr_gen);
		}
		break;

	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case READ_BLK_SUCCEEDED:

		if (_lvl > 1) {

			_t1_blks[_lvl].decode_from_blk(_blk);
			if (_lvl < _attr.in_out_ft.max_lvl) {
				Tree_node_index node_idx = tree_node_index(_vba, _lvl + 1, _attr.in_out_ft.degree);
				if (!check_hash(_blk, _t1_blks[_lvl + 1].nodes[node_idx].hash))
					_helper.mark_failed(progress, "hash mismatch");
			} else
				if (!check_hash(_blk, _attr.in_out_ft.hash))
					_helper.mark_failed(progress, "hash mismatch");

			Tree_node_index node_idx = tree_node_index(_vba, _lvl, _attr.in_out_ft.degree);
			Type_1_node &t1_node = _t1_blks[_lvl].nodes[node_idx];
			if (t1_node.valid()) {

				_lvl--;
				_old_pbas.pbas [_lvl] = t1_node.pba;
				_old_generations.items[_lvl] = t1_node.gen;
				_read_block.generate(_helper, READ_BLK, READ_BLK_SUCCEEDED, progress, t1_node.pba, _blk);
				if (VERBOSE_FT_EXTENSION)
					log("  lvl ", _lvl + 1, " node ", node_idx, " (", t1_node, "): load to lvl ", _lvl);
			} else {
				_alloc_lvl = _lvl;
				_add_new_branch_at(_lvl, node_idx);
				if (_old_generations.items[_alloc_lvl] == _attr.in_curr_gen) {

					_alloc_pba = _old_pbas.pbas[_alloc_lvl];
					_helper.state = ALLOC_PBA_SUCCEEDED;
					progress = true;
				} else {
					_alloc_pba = _old_pbas.pbas[_alloc_lvl];
					_allocate_pba.generate(_helper, ALLOC_PBA, ALLOC_PBA_SUCCEEDED, progress, _attr.in_out_mt, _attr.in_curr_gen, _alloc_pba);
				}
			}
		} else {
			_t2_blk.decode_from_blk(_blk);
			Tree_node_index t1_node_idx = tree_node_index(_vba, _lvl + 1, _attr.in_out_ft.degree);
			if (!check_hash(_blk, _t1_blks[_lvl + 1].nodes[t1_node_idx].hash))
				_helper.mark_failed(progress, "hash mismatch");

			Tree_node_index t2_node_idx = tree_node_index(_vba, _lvl, _attr.in_out_ft.degree);
			if (_t2_blk.nodes[t2_node_idx].valid())
				_helper.mark_failed(progress, "t2 node valid");

			_add_new_branch_at(_lvl, t2_node_idx);
			_alloc_lvl = _lvl;
			if (VERBOSE_FT_EXTENSION)
				log("  alloc lvl ", _alloc_lvl);

			_alloc_pba = _old_pbas.pbas[_alloc_lvl];
			_allocate_pba.generate(_helper, ALLOC_PBA, ALLOC_PBA_SUCCEEDED, progress, _attr.in_out_mt, _attr.in_curr_gen, _alloc_pba);
		}
		break;

	case ALLOC_PBA: progress |= _allocate_pba.execute(meta_tree, block_io); break;
	case ALLOC_PBA_SUCCEEDED:

		_new_pbas.pbas[_alloc_lvl] = _alloc_pba;
		if (_alloc_lvl < _attr.in_out_ft.max_lvl) {

			_alloc_lvl++;
			if (_old_generations.items[_alloc_lvl] == _attr.in_curr_gen) {

				_alloc_pba = _old_pbas.pbas[_alloc_lvl];
				_helper.state = ALLOC_PBA_SUCCEEDED;
				progress = true;
			} else {
				_alloc_pba = _old_pbas.pbas[_alloc_lvl];
				_allocate_pba.generate(_helper, ALLOC_PBA, ALLOC_PBA_SUCCEEDED, progress, _attr.in_out_mt, _attr.in_curr_gen, _alloc_pba);
			}
		} else {
			_generate_write_blk_req(progress);
			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", _attr.in_curr_gen);
		}
		break;

	case WRITE_BLK: progress |= _write_block.execute(block_io); break;
	case WRITE_BLK_SUCCEEDED:

		if (_lvl < _attr.in_out_ft.max_lvl) {
			if (_lvl > 1) {
				Tree_node_index node_idx = tree_node_index(_vba, _lvl + 1, _attr.in_out_ft.degree);
				Type_1_node &t1_node { _t1_blks[_lvl + 1].nodes[node_idx] };
				t1_node = { _new_pbas.pbas[_lvl], _attr.in_curr_gen };
				calc_hash(_blk, t1_node.hash);
				if (VERBOSE_FT_EXTENSION)
					log("  set lvl ", _lvl + 1, " node ", node_idx, ": ", t1_node);

				_lvl++;
				_generate_write_blk_req(progress);
			} else {
				Tree_node_index node_idx = tree_node_index(_vba, _lvl + 1, _attr.in_out_ft.degree);
				Type_1_node &t1_node = _t1_blks[_lvl + 1].nodes[node_idx];
				t1_node = { _new_pbas.pbas[_lvl], _attr.in_curr_gen };
				calc_hash(_blk, t1_node.hash);
				if (VERBOSE_FT_EXTENSION)
					log("  set lvl ", _lvl + 1, " t1_node ", node_idx, ": ", t1_node);

				_lvl++;
				_generate_write_blk_req(progress);
			}
		} else {
			_attr.in_out_ft.t1_node({ _new_pbas.pbas[_lvl], _attr.in_curr_gen });
			calc_hash(_blk, _attr.in_out_ft.hash);
			_attr.in_out_ft.num_leaves += _num_leaves;
			_helper.mark_succeeded(progress);
		}
		break;

	default: break;
	}
	return progress;
}
