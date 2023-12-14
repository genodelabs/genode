/*
 * \brief  Module for doing PBA allocations for the Free Tree via the Meta Tree
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
#include <tresor/meta_tree.h>
#include <tresor/hash.h>

using namespace Tresor;

bool Meta_tree::execute(Allocate_pba &req, Block_io &block_io) { return req.execute(block_io); }


void Meta_tree::Allocate_pba::_start_tree_traversal(bool &progress)
{
	_lvl = _attr.in_out_mt.max_lvl;
	_node_idx[_lvl] = 0;
	_t1_blks[_lvl].nodes[_node_idx[_lvl]] = _attr.in_out_mt.t1_node();
	_read_block.generate(_helper, READ_BLK, SEEK_DOWN, progress, _attr.in_out_mt.pba, _blk);
}


bool Meta_tree::Allocate_pba::execute(Block_io &block_io)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT: _start_tree_traversal(progress); break;
	case READ_BLK: progress |= _read_block.execute(block_io); break;
	case SEEK_DOWN:

		if (!check_hash(_blk, _t1_blks[_lvl].nodes[_node_idx[_lvl]].hash)) {
			_helper.mark_failed(progress, "hash mismatch");
			break;
		}
		_lvl--;
		_node_idx[_lvl] = 0;
		if (_lvl)
			_t1_blks[_lvl].decode_from_blk(_blk);
		else
			_t2_blk.decode_from_blk(_blk);
		_traverse_curr_node(progress);
		break;

	case SEEK_LEFT_OR_UP:

		if (_lvl < _attr.in_out_mt.max_lvl) {
			if (_node_idx[_lvl] < _attr.in_out_mt.degree - 1) {
				_node_idx[_lvl]++;
				_traverse_curr_node(progress);
			} else {
				_lvl++;
				_helper.state = SEEK_LEFT_OR_UP;
				progress = true;
			}
		} else
			_helper.mark_failed(progress, "not enough free pbas");
		break;

	case WRITE_BLK: progress |= _write_block.execute(block_io); break;
	case WRITE_BLK_SUCCEEDED:

		if (_lvl < _attr.in_out_mt.max_lvl) {
			if (_lvl)
				_t1_blks[_lvl].encode_to_blk(_blk);
			else
				_t2_blk.encode_to_blk(_blk);
			_lvl++;
			Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
			t1_node.gen = _attr.in_curr_gen;
			calc_hash(_blk, t1_node.hash);
			_write_block.generate(_helper, WRITE_BLK, WRITE_BLK_SUCCEEDED, progress, t1_node.pba, _blk);
		} else {
			_attr.in_out_mt.t1_node(_t1_blks[_lvl].nodes[_node_idx[_lvl]]);
			_helper.mark_succeeded(progress);
		}
		break;

	default: break;
	}
	return progress;
}


bool Meta_tree::Allocate_pba::_can_alloc_pba_of(Type_2_node &node)
{
	return node.valid() && node.alloc_gen != _attr.in_curr_gen;
}


void Meta_tree::Allocate_pba::_alloc_pba_of(Type_2_node &t2_node, Physical_block_address &pba)
{
	Physical_block_address old_pba { pba };
	pba = t2_node.pba;
	t2_node.pba = old_pba;
	t2_node.alloc_gen = _attr.in_curr_gen;
	t2_node.free_gen = _attr.in_curr_gen;
	t2_node.reserved = false;
}

void Meta_tree::Allocate_pba::_traverse_curr_node(bool &progress)
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
		if (_can_alloc_pba_of(t2_node)) {
			_alloc_pba_of(t2_node, _attr.in_out_pba);
			for (Tree_level_index lvl { 1 }; lvl <= _attr.in_out_mt.max_lvl; lvl++) {
				Type_1_node &t1_node { _t1_blks[lvl].nodes[_node_idx[lvl]] };
				if (!t1_node.is_volatile(_attr.in_curr_gen)) {
					bool pba_allocated { false };
					for (Type_2_node &t2_node : _t2_blk.nodes) {
						if (_can_alloc_pba_of(t2_node)) {
							_alloc_pba_of(t2_node, t1_node.pba);
							pba_allocated = true;
							break;
						}
					}
					ASSERT(pba_allocated);
				}
			}
			_helper.state = WRITE_BLK_SUCCEEDED;
		} else
			_helper.state = SEEK_LEFT_OR_UP;
		progress = true;
	}
}
