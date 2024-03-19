/*
 * \brief  Module for initializing the VBD
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2023-03-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/hash.h>
#include <tresor/vbd_initializer.h>

using namespace Tresor;

bool Vbd_initializer::Initialize::_execute_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	Type_1_node &node = _t1_blks.items[lvl].nodes[node_idx];
	Node_state &node_state = _node_states[lvl][node_idx];
	switch (node_state) {
	case DONE: return false;
	case INIT_BLOCK:

		if (lvl == 1) {
			node_state = INIT_NODE;
			progress = true;
		} else
			if (_num_remaining_leaves) {
				_reset_level(lvl - 1, INIT_BLOCK);
				node_state = INIT_NODE;
				progress = true;
				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", lvl, " ", node_idx, " reset level: ", lvl - 1);
			} else {
				node = { };
				node_state = DONE;
				progress = true;
				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", lvl, " ", node_idx, " assign pba 0, inner node unused");
			}
		break;

	case INIT_NODE:

		if (lvl == 1)
			if (_num_remaining_leaves) {
				node = { };
				if (!_attr.in_out_pba_alloc.alloc(node.pba)) {
					_helper.mark_failed(progress, "allocate pba");
					break;
				}
				node_state = DONE;
				_num_remaining_leaves--;
				progress = true;
				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", lvl, " ", node_idx, " assign pba: ", node.pba, " leaves left: ", _num_remaining_leaves);
			} else {
				node = { };
				node_state = DONE;
				progress = true;
				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", lvl, " ", node_idx, " assign pba 0, leaf unused");
			}
		else {
			node = { };
			if (!_attr.in_out_pba_alloc.alloc(node.pba)) {
				_helper.mark_failed(progress, "allocate pba");
				break;
			}
			_t1_blks.items[lvl - 1].encode_to_blk(_blk);
			calc_hash(_blk, node.hash);
			_write_block.generate(_helper, WRITE_BLOCK, EXECUTE_NODES, progress, node.pba, _blk);
			node_state = WRITING_BLOCK;
			if (VERBOSE_VBD_INIT)
				log("[vbd_init] node: ", lvl, " ", node_idx, " assign pba: ", node.pba);
		}
		break;

	case WRITING_BLOCK:

		ASSERT(lvl > 1);
		node_state = DONE;
		progress = true;
		if (VERBOSE_VBD_INIT)
			log("[vbd_init] node: ", lvl, " ", node_idx, " write pba: ", node.pba, " level: ", lvl - 1, " (node: ", node, ")");
		break;
	}
	return true;
}


bool Vbd_initializer::Initialize::execute(Block_io &block_io)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_num_remaining_leaves = _attr.in_tree_cfg.num_leaves;
		for (Tree_level_index lvl = 1; lvl < TREE_MAX_MAX_LEVEL; lvl++)
			_reset_level(lvl, Vbd_initializer::Initialize::DONE);

		_node_states[_attr.in_tree_cfg.max_lvl + 1][0] = Vbd_initializer::Initialize::INIT_BLOCK;
		_helper.state = EXECUTE_NODES;
		progress = true;
		break;

	case EXECUTE_NODES:

		for (Tree_level_index lvl = 1; lvl <= _attr.in_tree_cfg.max_lvl + 1; lvl++)
			for (Tree_node_index node_idx = 0; node_idx < _attr.in_tree_cfg.degree; node_idx++)
				if (_execute_node(lvl, node_idx, progress))
					return progress;

		if (_num_remaining_leaves)
			_helper.mark_failed(progress, "leaves remaining");
		else {
			_attr.out_tree_root = _t1_blks.items[_attr.in_tree_cfg.max_lvl + 1].nodes[0];
			_helper.mark_succeeded(progress);
		}
		break;

	case WRITE_BLOCK: progress |= _write_block.execute(block_io); break;
	default: break;
	}
	return progress;
}


void Vbd_initializer::Initialize::_reset_level(Tree_level_index lvl, Node_state state)
{
	for (unsigned int idx = 0; idx < NUM_NODES_PER_BLK; idx++) {
		_t1_blks.items[lvl].nodes[idx] = { };
		_node_states[lvl][idx] = state;
	}
}
